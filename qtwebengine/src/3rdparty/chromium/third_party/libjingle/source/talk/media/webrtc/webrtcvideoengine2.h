/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
#define TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_

#include <map>
#include <string>
#include <vector>

#include "talk/media/base/mediaengine.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "webrtc/base/cpumonitor.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/transport.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_renderer.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
class VideoCaptureModule;
class VideoDecoder;
class VideoEncoder;
class VideoRender;
class VideoSendStreamInput;
class VideoReceiveStream;
}

namespace rtc {
class CpuMonitor;
class Thread;
}  // namespace rtc

namespace cricket {

class VideoCapturer;
class VideoFrame;
class VideoProcessor;
class VideoRenderer;
class VoiceMediaChannel;
class WebRtcDecoderObserver;
class WebRtcEncoderObserver;
class WebRtcLocalStreamInfo;
class WebRtcRenderAdapter;
class WebRtcVideoChannelRecvInfo;
class WebRtcVideoChannelSendInfo;
class WebRtcVoiceEngine;

struct CapturedFrame;
struct Device;

class WebRtcVideoRenderer;

class UnsignalledSsrcHandler {
 public:
  enum Action {
    kDropPacket,
    kDeliverPacket,
  };
  virtual Action OnUnsignalledSsrc(VideoMediaChannel* engine,
                                   uint32_t ssrc) = 0;
};

// TODO(pbos): Remove, use external handlers only.
class DefaultUnsignalledSsrcHandler : public UnsignalledSsrcHandler {
 public:
  DefaultUnsignalledSsrcHandler();
  virtual Action OnUnsignalledSsrc(VideoMediaChannel* engine,
                                   uint32_t ssrc) OVERRIDE;

  VideoRenderer* GetDefaultRenderer() const;
  void SetDefaultRenderer(VideoMediaChannel* channel, VideoRenderer* renderer);

 private:
  uint32_t default_recv_ssrc_;
  VideoRenderer* default_renderer_;
};

class WebRtcVideoEncoderFactory2 {
 public:
  virtual ~WebRtcVideoEncoderFactory2();
  virtual std::vector<webrtc::VideoStream> CreateVideoStreams(
      const VideoCodec& codec,
      const VideoOptions& options,
      size_t num_streams);

  virtual void* CreateVideoEncoderSettings(const VideoCodec& codec,
                                           const VideoOptions& options);

  virtual void DestroyVideoEncoderSettings(const VideoCodec& codec,
                                           void* encoder_settings);
};

// CallFactory, overridden for testing to verify that webrtc::Call is configured
// properly.
class WebRtcCallFactory {
 public:
  virtual ~WebRtcCallFactory();
  virtual webrtc::Call* CreateCall(const webrtc::Call::Config& config);
};

// WebRtcVideoEngine2 is used for the new native WebRTC Video API (webrtc:1667).
class WebRtcVideoEngine2 : public sigslot::has_slots<> {
 public:
  // Creates the WebRtcVideoEngine2 with internal VideoCaptureModule.
  WebRtcVideoEngine2();
  virtual ~WebRtcVideoEngine2();

  // Used for testing to be able to check and use the webrtc::Call config.
  void SetCallFactory(WebRtcCallFactory* call_factory);

  // Basic video engine implementation.
  bool Init(rtc::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);

  WebRtcVideoChannel2* CreateChannel(const VideoOptions& options,
                                     VoiceMediaChannel* voice_channel);

  const std::vector<VideoCodec>& codecs() const;
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;
  void SetLogging(int min_sev, const char* filter);

  // Set a WebRtcVideoDecoderFactory for external decoding. Video engine does
  // not take the ownership of |decoder_factory|. The caller needs to make sure
  // that |decoder_factory| outlives the video engine.
  void SetExternalDecoderFactory(WebRtcVideoDecoderFactory* decoder_factory);
  // Set a WebRtcVideoEncoderFactory for external encoding. Video engine does
  // not take the ownership of |encoder_factory|. The caller needs to make sure
  // that |encoder_factory| outlives the video engine.
  virtual void SetExternalEncoderFactory(
      WebRtcVideoEncoderFactory* encoder_factory);

  bool EnableTimedRender();
  // This is currently ignored.
  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

  // Set the VoiceEngine for A/V sync. This can only be called before Init.
  bool SetVoiceEngine(WebRtcVoiceEngine* voice_engine);

  bool FindCodec(const VideoCodec& in);
  bool CanSendCodec(const VideoCodec& in,
                    const VideoCodec& current,
                    VideoCodec* out);
  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);

  VideoFormat GetStartCaptureFormat() const { return default_codec_format_; }

  rtc::CpuMonitor* cpu_monitor() { return cpu_monitor_.get(); }

  virtual WebRtcVideoEncoderFactory2* GetVideoEncoderFactory();

 private:
  std::vector<VideoCodec> GetSupportedCodecs() const;

  rtc::Thread* worker_thread_;
  WebRtcVoiceEngine* voice_engine_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  VideoFormat default_codec_format_;

  bool initialized_;

  rtc::scoped_ptr<rtc::CpuMonitor> cpu_monitor_;
  WebRtcVideoEncoderFactory2 default_video_encoder_factory_;

  WebRtcCallFactory default_call_factory_;
  WebRtcCallFactory* call_factory_;

  WebRtcVideoDecoderFactory* external_decoder_factory_;
  WebRtcVideoEncoderFactory* external_encoder_factory_;
};

class WebRtcVideoChannel2 : public rtc::MessageHandler,
                            public VideoMediaChannel,
                            public webrtc::newapi::Transport,
                            public webrtc::LoadObserver {
 public:
  WebRtcVideoChannel2(WebRtcCallFactory* call_factory,
                      WebRtcVoiceEngine* voice_engine,
                      VoiceMediaChannel* voice_channel,
                      const VideoOptions& options,
                      WebRtcVideoEncoderFactory* external_encoder_factory,
                      WebRtcVideoDecoderFactory* external_decoder_factory,
                      WebRtcVideoEncoderFactory2* encoder_factory);
  ~WebRtcVideoChannel2();
  bool Init();

  // VideoMediaChannel implementation
  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) OVERRIDE;
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs) OVERRIDE;
  virtual bool GetSendCodec(VideoCodec* send_codec) OVERRIDE;
  virtual bool SetSendStreamFormat(uint32 ssrc,
                                   const VideoFormat& format) OVERRIDE;
  virtual bool SetRender(bool render) OVERRIDE;
  virtual bool SetSend(bool send) OVERRIDE;

  virtual bool AddSendStream(const StreamParams& sp) OVERRIDE;
  virtual bool RemoveSendStream(uint32 ssrc) OVERRIDE;
  virtual bool AddRecvStream(const StreamParams& sp) OVERRIDE;
  virtual bool RemoveRecvStream(uint32 ssrc) OVERRIDE;
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) OVERRIDE;
  virtual bool GetStats(const StatsOptions& options,
                        VideoMediaInfo* info) OVERRIDE;
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) OVERRIDE;
  virtual bool SendIntraFrame() OVERRIDE;
  virtual bool RequestIntraFrame() OVERRIDE;

  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time)
      OVERRIDE;
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time)
      OVERRIDE;
  virtual void OnReadyToSend(bool ready) OVERRIDE;
  virtual bool MuteStream(uint32 ssrc, bool mute) OVERRIDE;

  // Set send/receive RTP header extensions. This must be done before creating
  // streams as it only has effect on future streams.
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) OVERRIDE;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) OVERRIDE;
  virtual bool SetMaxSendBandwidth(int bps) OVERRIDE;
  virtual bool SetOptions(const VideoOptions& options) OVERRIDE;
  virtual bool GetOptions(VideoOptions* options) const OVERRIDE {
    *options = options_;
    return true;
  }
  virtual void SetInterface(NetworkInterface* iface) OVERRIDE;
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) OVERRIDE;

  virtual void OnMessage(rtc::Message* msg) OVERRIDE;

  virtual void OnLoadUpdate(Load load) OVERRIDE;

  // Implemented for VideoMediaChannelTest.
  bool sending() const { return sending_; }
  uint32 GetDefaultSendChannelSsrc() { return default_send_ssrc_; }
  bool GetRenderer(uint32 ssrc, VideoRenderer** renderer);

 private:
  void ConfigureReceiverRtp(webrtc::VideoReceiveStream::Config* config,
                            const StreamParams& sp) const;
  bool CodecIsExternallySupported(const std::string& name) const;

  struct VideoCodecSettings {
    VideoCodecSettings();

    bool operator ==(const VideoCodecSettings& other) const;

    VideoCodec codec;
    webrtc::FecConfig fec;
    int rtx_payload_type;
  };

  // Wrapper for the sender part, this is where the capturer is connected and
  // frames are then converted from cricket frames to webrtc frames.
  class WebRtcVideoSendStream : public sigslot::has_slots<> {
   public:
    WebRtcVideoSendStream(
        webrtc::Call* call,
        WebRtcVideoEncoderFactory* external_encoder_factory,
        WebRtcVideoEncoderFactory2* encoder_factory,
        const VideoOptions& options,
        const Settable<VideoCodecSettings>& codec_settings,
        const StreamParams& sp,
        const std::vector<webrtc::RtpExtension>& rtp_extensions);

    ~WebRtcVideoSendStream();
    void SetOptions(const VideoOptions& options);
    void SetCodec(const VideoCodecSettings& codec);
    void SetRtpExtensions(
        const std::vector<webrtc::RtpExtension>& rtp_extensions);

    void InputFrame(VideoCapturer* capturer, const VideoFrame* frame);
    bool SetCapturer(VideoCapturer* capturer);
    bool SetVideoFormat(const VideoFormat& format);
    void MuteStream(bool mute);
    bool DisconnectCapturer();

    void Start();
    void Stop();

    VideoSenderInfo GetVideoSenderInfo();
    void FillBandwidthEstimationInfo(BandwidthEstimationInfo* bwe_info);

    void OnCpuResolutionRequest(
        CoordinatedVideoAdapter::AdaptRequest adapt_request);

   private:
    // Parameters needed to reconstruct the underlying stream.
    // webrtc::VideoSendStream doesn't support setting a lot of options on the
    // fly, so when those need to be changed we tear down and reconstruct with
    // similar parameters depending on which options changed etc.
    struct VideoSendStreamParameters {
      VideoSendStreamParameters(
          const webrtc::VideoSendStream::Config& config,
          const VideoOptions& options,
          const Settable<VideoCodecSettings>& codec_settings);
      webrtc::VideoSendStream::Config config;
      VideoOptions options;
      Settable<VideoCodecSettings> codec_settings;
      // Sent resolutions + bitrates etc. by the underlying VideoSendStream,
      // typically changes when setting a new resolution or reconfiguring
      // bitrates.
      webrtc::VideoEncoderConfig encoder_config;
    };

    struct AllocatedEncoder {
      AllocatedEncoder(webrtc::VideoEncoder* encoder,
                       webrtc::VideoCodecType type,
                       bool external)
          : encoder(encoder), type(type), external(external) {}
      webrtc::VideoEncoder* encoder;
      webrtc::VideoCodecType type;
      bool external;
    };

    struct Dimensions {
      Dimensions() : width(-1), height(-1), is_screencast(false) {}
      int width;
      int height;
      bool is_screencast;
    };

    AllocatedEncoder CreateVideoEncoder(const VideoCodec& codec)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void DestroyVideoEncoder(AllocatedEncoder* encoder)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void SetCodecAndOptions(const VideoCodecSettings& codec,
                            const VideoOptions& options)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void RecreateWebRtcStream() EXCLUSIVE_LOCKS_REQUIRED(lock_);
    webrtc::VideoEncoderConfig CreateVideoEncoderConfig(
        const Dimensions& dimensions,
        const VideoCodec& codec) const EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void SetDimensions(int width, int height, bool is_screencast)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);

    webrtc::Call* const call_;
    WebRtcVideoEncoderFactory* const external_encoder_factory_
        GUARDED_BY(lock_);
    WebRtcVideoEncoderFactory2* const encoder_factory_ GUARDED_BY(lock_);

    rtc::CriticalSection lock_;
    webrtc::VideoSendStream* stream_ GUARDED_BY(lock_);
    VideoSendStreamParameters parameters_ GUARDED_BY(lock_);
    AllocatedEncoder allocated_encoder_ GUARDED_BY(lock_);
    Dimensions last_dimensions_ GUARDED_BY(lock_);

    VideoCapturer* capturer_ GUARDED_BY(lock_);
    bool sending_ GUARDED_BY(lock_);
    bool muted_ GUARDED_BY(lock_);
    VideoFormat format_ GUARDED_BY(lock_);

    rtc::CriticalSection frame_lock_;
    webrtc::I420VideoFrame video_frame_ GUARDED_BY(frame_lock_);
  };

  // Wrapper for the receiver part, contains configs etc. that are needed to
  // reconstruct the underlying VideoReceiveStream. Also serves as a wrapper
  // between webrtc::VideoRenderer and cricket::VideoRenderer.
  class WebRtcVideoReceiveStream : public webrtc::VideoRenderer {
   public:
    WebRtcVideoReceiveStream(
        webrtc::Call*,
        WebRtcVideoDecoderFactory* external_decoder_factory,
        const webrtc::VideoReceiveStream::Config& config,
        const std::vector<VideoCodecSettings>& recv_codecs);
    ~WebRtcVideoReceiveStream();

    void SetRecvCodecs(const std::vector<VideoCodecSettings>& recv_codecs);
    void SetRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions);

    virtual void RenderFrame(const webrtc::I420VideoFrame& frame,
                             int time_to_render_ms) OVERRIDE;

    void SetRenderer(cricket::VideoRenderer* renderer);
    cricket::VideoRenderer* GetRenderer();

    VideoReceiverInfo GetVideoReceiverInfo();

   private:
    struct AllocatedDecoder {
      AllocatedDecoder(webrtc::VideoDecoder* decoder,
                       webrtc::VideoCodecType type,
                       bool external)
          : decoder(decoder), type(type), external(external) {}
      webrtc::VideoDecoder* decoder;
      webrtc::VideoCodecType type;
      bool external;
    };

    void SetSize(int width, int height);
    void RecreateWebRtcStream();

    AllocatedDecoder CreateOrReuseVideoDecoder(
        std::vector<AllocatedDecoder>* old_decoder,
        const VideoCodec& codec);
    void ClearDecoders(std::vector<AllocatedDecoder>* allocated_decoders);

    webrtc::Call* const call_;

    webrtc::VideoReceiveStream* stream_;
    webrtc::VideoReceiveStream::Config config_;

    WebRtcVideoDecoderFactory* const external_decoder_factory_;
    std::vector<AllocatedDecoder> allocated_decoders_;

    rtc::CriticalSection renderer_lock_;
    cricket::VideoRenderer* renderer_ GUARDED_BY(renderer_lock_);
    int last_width_ GUARDED_BY(renderer_lock_);
    int last_height_ GUARDED_BY(renderer_lock_);
  };

  void Construct(webrtc::Call* call, WebRtcVideoEngine2* engine);
  void SetDefaultOptions();

  virtual bool SendRtp(const uint8_t* data, size_t len) OVERRIDE;
  virtual bool SendRtcp(const uint8_t* data, size_t len) OVERRIDE;

  void StartAllSendStreams();
  void StopAllSendStreams();

  static std::vector<VideoCodecSettings> MapCodecs(
      const std::vector<VideoCodec>& codecs);
  std::vector<VideoCodecSettings> FilterSupportedCodecs(
      const std::vector<VideoCodecSettings>& mapped_codecs) const;

  void FillSenderStats(VideoMediaInfo* info);
  void FillReceiverStats(VideoMediaInfo* info);
  void FillBandwidthEstimationStats(VideoMediaInfo* info);

  uint32_t rtcp_receiver_report_ssrc_;
  bool sending_;
  rtc::scoped_ptr<webrtc::Call> call_;
  WebRtcCallFactory* call_factory_;

  uint32_t default_send_ssrc_;

  DefaultUnsignalledSsrcHandler default_unsignalled_ssrc_handler_;
  UnsignalledSsrcHandler* const unsignalled_ssrc_handler_;

  rtc::CriticalSection stream_crit_;
  // Using primary-ssrc (first ssrc) as key.
  std::map<uint32, WebRtcVideoSendStream*> send_streams_
      GUARDED_BY(stream_crit_);
  std::map<uint32, WebRtcVideoReceiveStream*> receive_streams_
      GUARDED_BY(stream_crit_);

  Settable<VideoCodecSettings> send_codec_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  VoiceMediaChannel* const voice_channel_;
  WebRtcVideoEncoderFactory* const external_encoder_factory_;
  WebRtcVideoDecoderFactory* const external_decoder_factory_;
  WebRtcVideoEncoderFactory2* const encoder_factory_;
  std::vector<VideoCodecSettings> recv_codecs_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;
  VideoOptions options_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
