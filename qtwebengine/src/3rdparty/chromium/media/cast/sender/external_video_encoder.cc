// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/external_video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cast/cast_defines.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
namespace cast {
class LocalVideoEncodeAcceleratorClient;
}  // namespace cast
}  // namespace media

namespace {
static const size_t kOutputBufferCount = 3;

void LogFrameEncodedEvent(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    base::TimeTicks event_time,
    media::cast::RtpTimestamp rtp_timestamp,
    uint32 frame_id) {
  cast_environment->Logging()->InsertFrameEvent(
      event_time, media::cast::FRAME_ENCODED, media::cast::VIDEO_EVENT,
      rtp_timestamp, frame_id);
}
}  // namespace

namespace media {
namespace cast {

// Container for the associated data of a video frame being processed.
struct InProgressFrameEncode {
  const RtpTimestamp rtp_timestamp;
  const base::TimeTicks reference_time;
  const VideoEncoder::FrameEncodedCallback frame_encoded_callback;

  InProgressFrameEncode(RtpTimestamp rtp,
                        base::TimeTicks r_time,
                        VideoEncoder::FrameEncodedCallback callback)
      : rtp_timestamp(rtp),
        reference_time(r_time),
        frame_encoded_callback(callback) {}
};

// The ExternalVideoEncoder class can be deleted directly by cast, while
// LocalVideoEncodeAcceleratorClient stays around long enough to properly shut
// down the VideoEncodeAccelerator.
class LocalVideoEncodeAcceleratorClient
    : public VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient> {
 public:
  // Create an instance of this class and post a task to create
  // video_encode_accelerator_. A ref to |this| will be kept, awaiting reply
  // via ProxyCreateVideoEncodeAccelerator, which will provide us with the
  // encoder task runner and vea instance. We cannot be destroyed until we
  // receive the reply, otherwise the VEA object created may leak.
  static scoped_refptr<LocalVideoEncodeAcceleratorClient> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      const base::WeakPtr<ExternalVideoEncoder>& weak_owner) {
    scoped_refptr<LocalVideoEncodeAcceleratorClient> client(
        new LocalVideoEncodeAcceleratorClient(
            cast_environment, create_video_encode_mem_cb, weak_owner));

    // This will keep a ref to |client|, if weak_owner is destroyed before
    // ProxyCreateVideoEncodeAccelerator is called, we will stay alive until
    // we can properly destroy the VEA.
    create_vea_cb.Run(base::Bind(
        &LocalVideoEncodeAcceleratorClient::OnCreateVideoEncodeAcceleratorProxy,
        client));

    return client;
  }

  // Initialize the real HW encoder.
  void Initialize(const VideoSenderConfig& video_config) {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    VideoCodecProfile output_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    switch (video_config.codec) {
      case CODEC_VIDEO_VP8:
        output_profile = media::VP8PROFILE_ANY;
        break;
      case CODEC_VIDEO_H264:
        output_profile = media::H264PROFILE_MAIN;
        break;
      case CODEC_VIDEO_FAKE:
        NOTREACHED() << "Fake software video encoder cannot be external";
        break;
      default:
        NOTREACHED() << "Video codec not specified or not supported";
        break;
    }
    max_frame_rate_ = video_config.max_frame_rate;

    bool result = video_encode_accelerator_->Initialize(
        media::VideoFrame::I420,
        gfx::Size(video_config.width, video_config.height),
        output_profile,
        video_config.start_bitrate,
        this);

    UMA_HISTOGRAM_BOOLEAN("Cast.Sender.VideoEncodeAcceleratorInitializeSuccess",
                          result);
    if (!result) {
      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(&ExternalVideoEncoder::EncoderInitialized, weak_owner_,
                     false));
      return;
    }

    // Wait until shared memory is allocated to indicate that encoder is
    // initialized.
  }

  // Destroy the VEA on the correct thread.
  void Destroy() {
    DCHECK(encoder_task_runner_.get());
    if (!video_encode_accelerator_)
      return;

    if (encoder_task_runner_->RunsTasksOnCurrentThread()) {
      video_encode_accelerator_.reset();
    } else {
      // We do this instead of just reposting to encoder_task_runner_, because
      // we are called from the destructor.
      encoder_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&DestroyVideoEncodeAcceleratorOnEncoderThread,
                     base::Passed(&video_encode_accelerator_)));
    }
  }

  void SetBitRate(uint32 bit_rate) {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    video_encode_accelerator_->RequestEncodingParametersChange(bit_rate,
                                                               max_frame_rate_);
  }

  void EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& reference_time,
      bool key_frame_requested,
      const VideoEncoder::FrameEncodedCallback& frame_encoded_callback) {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    in_progress_frame_encodes_.push_back(InProgressFrameEncode(
        TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
        reference_time,
        frame_encoded_callback));

    // BitstreamBufferReady will be called once the encoder is done.
    video_encode_accelerator_->Encode(video_frame, key_frame_requested);
  }

 protected:
  void NotifyError(VideoEncodeAccelerator::Error error) override {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    VLOG(1) << "ExternalVideoEncoder NotifyError: " << error;

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderError, weak_owner_));
  }

  // Called to allocate the input and output buffers.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    DCHECK(video_encode_accelerator_);

    for (size_t j = 0; j < kOutputBufferCount; ++j) {
      create_video_encode_memory_cb_.Run(
          output_buffer_size,
          base::Bind(&LocalVideoEncodeAcceleratorClient::OnCreateSharedMemory,
                     this));
    }
  }

  // Encoder has encoded a frame and it's available in one of out output
  // buffers.
  void BitstreamBufferReady(int32 bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame) override {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    if (bitstream_buffer_id < 0 ||
        bitstream_buffer_id >= static_cast<int32>(output_buffers_.size())) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid bitstream_buffer_id="
              << bitstream_buffer_id;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    base::SharedMemory* output_buffer = output_buffers_[bitstream_buffer_id];
    if (payload_size > output_buffer->mapped_size()) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid payload_size = "
              << payload_size;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    if (key_frame)
      key_frame_encountered_ = true;
    if (!key_frame_encountered_) {
      // Do not send video until we have encountered the first key frame.
      // Save the bitstream buffer in |stream_header_| to be sent later along
      // with the first key frame.
      stream_header_.append(static_cast<const char*>(output_buffer->memory()),
                            payload_size);
    } else if (!in_progress_frame_encodes_.empty()) {
      const InProgressFrameEncode& request = in_progress_frame_encodes_.front();

      scoped_ptr<EncodedFrame> encoded_frame(new EncodedFrame());
      encoded_frame->dependency = key_frame ? EncodedFrame::KEY :
          EncodedFrame::DEPENDENT;
      encoded_frame->frame_id = ++last_encoded_frame_id_;
      if (key_frame)
        encoded_frame->referenced_frame_id = encoded_frame->frame_id;
      else
        encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
      encoded_frame->rtp_timestamp = request.rtp_timestamp;
      encoded_frame->reference_time = request.reference_time;
      if (!stream_header_.empty()) {
        encoded_frame->data = stream_header_;
        stream_header_.clear();
      }
      encoded_frame->data.append(
          static_cast<const char*>(output_buffer->memory()), payload_size);

      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(&LogFrameEncodedEvent,
                     cast_environment_,
                     cast_environment_->Clock()->NowTicks(),
                     encoded_frame->rtp_timestamp,
                     encoded_frame->frame_id));

      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(request.frame_encoded_callback,
                     base::Passed(&encoded_frame)));

      in_progress_frame_encodes_.pop_front();
    } else {
      VLOG(1) << "BitstreamBufferReady(): no encoded frame data available";
    }

    // We need to re-add the output buffer to the encoder after we are done
    // with it.
    video_encode_accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id]->handle(),
        output_buffers_[bitstream_buffer_id]->mapped_size()));
  }

 private:
  LocalVideoEncodeAcceleratorClient(
      scoped_refptr<CastEnvironment> cast_environment,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
      const base::WeakPtr<ExternalVideoEncoder>& weak_owner)
      : cast_environment_(cast_environment),
        create_video_encode_memory_cb_(create_video_encode_mem_cb),
        weak_owner_(weak_owner),
        last_encoded_frame_id_(kStartFrameId),
        key_frame_encountered_(false) {}

  // Trampoline VEA creation callback to OnCreateVideoEncodeAccelerator()
  // on encoder_task_runner. Normally we would just repost the same method to
  // it, and would not need a separate proxy method, but we can't
  // ThreadTaskRunnerHandle::Get() in unittests just yet.
  void OnCreateVideoEncodeAcceleratorProxy(
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      scoped_ptr<media::VideoEncodeAccelerator> vea) {
    encoder_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&media::cast::LocalVideoEncodeAcceleratorClient::
                       OnCreateVideoEncodeAccelerator,
                   this,
                   encoder_task_runner,
                   base::Passed(&vea)));
  }

  void OnCreateVideoEncodeAccelerator(
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      scoped_ptr<media::VideoEncodeAccelerator> vea) {
    encoder_task_runner_ = encoder_task_runner;
    video_encode_accelerator_.reset(vea.release());

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::OnCreateVideoEncodeAccelerator,
                   weak_owner_,
                   encoder_task_runner_));
  }

  // Note: This method can be called on any thread.
  void OnCreateSharedMemory(scoped_ptr<base::SharedMemory> memory) {
    encoder_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalVideoEncodeAcceleratorClient::ReceivedSharedMemory,
                   this,
                   base::Passed(&memory)));
  }

  void ReceivedSharedMemory(scoped_ptr<base::SharedMemory> memory) {
    DCHECK(encoder_task_runner_.get());
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    output_buffers_.push_back(memory.release());

    // Wait until all requested buffers are received.
    if (output_buffers_.size() < kOutputBufferCount)
      return;

    // Immediately provide all output buffers to the VEA.
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      video_encode_accelerator_->UseOutputBitstreamBuffer(
          media::BitstreamBuffer(static_cast<int32>(i),
                                 output_buffers_[i]->handle(),
                                 output_buffers_[i]->mapped_size()));
    }

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderInitialized, weak_owner_,
                   true));
  }

  static void DestroyVideoEncodeAcceleratorOnEncoderThread(
      scoped_ptr<media::VideoEncodeAccelerator> vea) {
    // VEA::~VEA specialization takes care of calling Destroy() on the VEA impl.
  }

  friend class base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient>;

  ~LocalVideoEncodeAcceleratorClient() override {
    Destroy();
    DCHECK(!video_encode_accelerator_);
  }

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  scoped_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;
  const CreateVideoEncodeMemoryCallback create_video_encode_memory_cb_;
  const base::WeakPtr<ExternalVideoEncoder> weak_owner_;
  int max_frame_rate_;
  uint32 last_encoded_frame_id_;
  bool key_frame_encountered_;
  std::string stream_header_;

  // Shared memory buffers for output with the VideoAccelerator.
  ScopedVector<base::SharedMemory> output_buffers_;

  // FIFO list.
  std::list<InProgressFrameEncode> in_progress_frame_encodes_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoEncodeAcceleratorClient);
};

ExternalVideoEncoder::ExternalVideoEncoder(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CastInitializationCallback& initialization_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb)
    : video_config_(video_config),
      cast_environment_(cast_environment),
      encoder_active_(false),
      key_frame_requested_(false),
      initialization_cb_(initialization_cb),
      weak_factory_(this) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  video_accelerator_client_ =
      LocalVideoEncodeAcceleratorClient::Create(cast_environment_,
                                                create_vea_cb,
                                                create_video_encode_mem_cb,
                                                weak_factory_.GetWeakPtr());
  DCHECK(video_accelerator_client_.get());
}

ExternalVideoEncoder::~ExternalVideoEncoder() {
}

void ExternalVideoEncoder::EncoderInitialized(bool success) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = success;
  DCHECK(!initialization_cb_.is_null());
  initialization_cb_.Run(
      success ?
      STATUS_VIDEO_INITIALIZED : STATUS_HW_VIDEO_ENCODER_NOT_SUPPORTED);
  initialization_cb_.Reset();
}

void ExternalVideoEncoder::EncoderError() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = false;
}

void ExternalVideoEncoder::OnCreateVideoEncodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_task_runner_ = encoder_task_runner;

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::Initialize,
                 video_accelerator_client_,
                 video_config_));
}

bool ExternalVideoEncoder::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& reference_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (!encoder_active_)
    return false;

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::EncodeVideoFrame,
                 video_accelerator_client_,
                 video_frame,
                 reference_time,
                 key_frame_requested_,
                 frame_encoded_callback));

  key_frame_requested_ = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void ExternalVideoEncoder::SetBitRate(int new_bit_rate) {
  if (!encoder_active_) {
    // If we receive SetBitRate() before VEA creation callback is invoked,
    // cache the new bit rate in the encoder config and use the new settings
    // to initialize VEA.
    video_config_.start_bitrate = new_bit_rate;
    return;
  }

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::SetBitRate,
                 video_accelerator_client_,
                 new_bit_rate));
}

// Inform the encoder to encode the next frame as a key frame.
void ExternalVideoEncoder::GenerateKeyFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  key_frame_requested_ = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void ExternalVideoEncoder::LatestFrameIdToReference(uint32 /*frame_id*/) {
  // Do nothing not supported.
}
}  //  namespace cast
}  //  namespace media
