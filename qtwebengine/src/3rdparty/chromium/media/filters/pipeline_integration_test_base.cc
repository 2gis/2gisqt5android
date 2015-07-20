// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "media/base/media_log.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/filters/renderer_impl.h"
#include "media/filters/vpx_video_decoder.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace media {

const char kNullVideoHash[] = "d41d8cd98f00b204e9800998ecf8427e";
const char kNullAudioHash[] = "0.00,0.00,0.00,0.00,0.00,0.00,";

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      clockless_playback_(false),
      pipeline_(
          new Pipeline(message_loop_.message_loop_proxy(), new MediaLog())),
      ended_(false),
      pipeline_status_(PIPELINE_OK),
      last_video_frame_format_(VideoFrame::UNKNOWN),
      hardware_config_(AudioParameters(), AudioParameters()) {
  base::MD5Init(&md5_context_);
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (!pipeline_->IsRunning())
    return;

  Stop();
}

void PipelineIntegrationTestBase::SaveStatus(PipelineStatus status) {
  pipeline_status_ = status;
}

void PipelineIntegrationTestBase::OnStatusCallback(
    PipelineStatus status) {
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

void PipelineIntegrationTestBase::OnStatusCallbackChecked(
    PipelineStatus expected_status,
    PipelineStatus status) {
  EXPECT_EQ(expected_status, status);
  OnStatusCallback(status);
}

PipelineStatusCB PipelineIntegrationTestBase::QuitOnStatusCB(
    PipelineStatus expected_status) {
  return base::Bind(&PipelineIntegrationTestBase::OnStatusCallbackChecked,
                    base::Unretained(this),
                    expected_status);
}

void PipelineIntegrationTestBase::DemuxerNeedKeyCB(
    const std::string& type,
    const std::vector<uint8>& init_data) {
  DCHECK(!init_data.empty());
  CHECK(!need_key_cb_.is_null());
  need_key_cb_.Run(type, init_data);
}

void PipelineIntegrationTestBase::OnEnded() {
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = PIPELINE_OK;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  if (ended_)
    return (pipeline_status_ == PIPELINE_OK);
  message_loop_.Run();
  EXPECT_TRUE(ended_);
  return ended_ && (pipeline_status_ == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (ended_ || pipeline_status_ != PIPELINE_OK)
    return pipeline_status_;
  message_loop_.Run();
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

bool PipelineIntegrationTestBase::Start(const base::FilePath& file_path,
                                        PipelineStatus expected_status) {
  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .Times(AtMost(1));
  CreateDemuxer(file_path);
  pipeline_->Start(
      demuxer_.get(),
      CreateRenderer(NULL),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      QuitOnStatusCB(expected_status),
      base::Bind(&PipelineIntegrationTestBase::OnMetadata,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingStateChanged,
                 base::Unretained(this)),
      base::Closure(),
      base::Bind(&PipelineIntegrationTestBase::OnAddTextTrack,
                 base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Start(const base::FilePath& file_path,
                                        PipelineStatus expected_status,
                                        kTestType test_type) {
  hashing_enabled_ = test_type == kHashed;
  clockless_playback_ = test_type == kClockless;
  return Start(file_path, expected_status);
}

bool PipelineIntegrationTestBase::Start(const base::FilePath& file_path) {
  return Start(file_path, NULL);
}

bool PipelineIntegrationTestBase::Start(const base::FilePath& file_path,
                                        Decryptor* decryptor) {
  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .Times(AtMost(1));

  CreateDemuxer(file_path);
  pipeline_->Start(
      demuxer_.get(),
      CreateRenderer(decryptor),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnMetadata,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingStateChanged,
                 base::Unretained(this)),
      base::Closure(),
      base::Bind(&PipelineIntegrationTestBase::OnAddTextTrack,
                 base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  ended_ = false;

  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&message_loop_, &base::MessageLoop::QuitNow));
  pipeline_->Seek(seek_time,
                  base::Bind(&PipelineIntegrationTestBase::SaveStatus,
                             base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop(base::MessageLoop::QuitClosure());
  message_loop_.Run();
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    const base::TimeDelta& quit_time) {
  if (pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    message_loop_.Quit();
    return;
  }

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this), quit_time),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this),
                 wait_time),
      base::TimeDelta::FromMilliseconds(10));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::CreateDemuxer(
    const base::FilePath& file_path) {
  FileDataSource* file_data_source = new FileDataSource();
  CHECK(file_data_source->Initialize(file_path)) << "Is " << file_path.value()
                                                 << " missing?";
  data_source_.reset(file_data_source);

  Demuxer::NeedKeyCB need_key_cb = base::Bind(
      &PipelineIntegrationTestBase::DemuxerNeedKeyCB, base::Unretained(this));
  demuxer_ =
      scoped_ptr<Demuxer>(new FFmpegDemuxer(message_loop_.message_loop_proxy(),
                                            data_source_.get(),
                                            need_key_cb,
                                            new MediaLog()));
}

scoped_ptr<Renderer> PipelineIntegrationTestBase::CreateRenderer(
    Decryptor* decryptor) {
  ScopedVector<VideoDecoder> video_decoders;
#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(
      new VpxVideoDecoder(message_loop_.message_loop_proxy()));
#endif  // !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(
      new FFmpegVideoDecoder(message_loop_.message_loop_proxy()));

  // Disable frame dropping if hashing is enabled.
  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      message_loop_.message_loop_proxy(),
      video_decoders.Pass(),
      base::Bind(&PipelineIntegrationTestBase::SetDecryptor,
                 base::Unretained(this),
                 decryptor),
      base::Bind(&PipelineIntegrationTestBase::OnVideoRendererPaint,
                 base::Unretained(this)),
      false,
      new MediaLog()));

  if (!clockless_playback_) {
    audio_sink_ = new NullAudioSink(message_loop_.message_loop_proxy());
  } else {
    clockless_audio_sink_ = new ClocklessAudioSink();
  }

  ScopedVector<AudioDecoder> audio_decoders;
  audio_decoders.push_back(
      new FFmpegAudioDecoder(message_loop_.message_loop_proxy(), LogCB()));
  audio_decoders.push_back(
      new OpusAudioDecoder(message_loop_.message_loop_proxy()));

  AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             CHANNEL_LAYOUT_STEREO,
                             44100,
                             16,
                             512);
  hardware_config_.UpdateOutputConfig(out_params);

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      message_loop_.message_loop_proxy(),
      (clockless_playback_)
          ? static_cast<AudioRendererSink*>(clockless_audio_sink_.get())
          : audio_sink_.get(),
      audio_decoders.Pass(),
      base::Bind(&PipelineIntegrationTestBase::SetDecryptor,
                 base::Unretained(this),
                 decryptor),
      hardware_config_,
      new MediaLog()));
  if (hashing_enabled_)
    audio_sink_->StartAudioHashForTesting();

  scoped_ptr<RendererImpl> renderer_impl(
      new RendererImpl(message_loop_.message_loop_proxy(),
                       audio_renderer.Pass(),
                       video_renderer.Pass()));

  // Prevent non-deterministic buffering state callbacks from firing (e.g., slow
  // machine, valgrind).
  renderer_impl->DisableUnderflowForTesting();

  if (clockless_playback_)
    renderer_impl->EnableClocklessVideoPlaybackForTesting();

  return renderer_impl.Pass();
}

void PipelineIntegrationTestBase::SetDecryptor(
    Decryptor* decryptor,
    const DecryptorReadyCB& decryptor_ready_cb) {
  decryptor_ready_cb.Run(
      decryptor,
      base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                 base::Unretained(this)));
  EXPECT_CALL(*this, DecryptorAttached(true));
}

void PipelineIntegrationTestBase::OnVideoRendererPaint(
    const scoped_refptr<VideoFrame>& frame) {
  last_video_frame_format_ = frame->format();
  if (!hashing_enabled_)
    return;
  frame->HashFrameForTesting(&md5_context_);
}

std::string PipelineIntegrationTestBase::GetVideoHash() {
  DCHECK(hashing_enabled_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

std::string PipelineIntegrationTestBase::GetAudioHash() {
  DCHECK(hashing_enabled_);
  return audio_sink_->GetAudioHashForTesting();
}

base::TimeDelta PipelineIntegrationTestBase::GetAudioTime() {
  DCHECK(clockless_playback_);
  return clockless_audio_sink_->render_time();
}

base::TimeTicks DummyTickClock::NowTicks() {
  now_ += base::TimeDelta::FromSeconds(60);
  return now_;
}

}  // namespace media
