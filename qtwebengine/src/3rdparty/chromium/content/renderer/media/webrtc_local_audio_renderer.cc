// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_renderer.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_frame_impl.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_bus.h"

namespace content {

namespace {

enum LocalRendererSinkStates {
  kSinkStarted = 0,
  kSinkNeverStarted,
  kSinkStatesMax  // Must always be last!
};

}  // namespace

// media::AudioRendererSink::RenderCallback implementation
int WebRtcLocalAudioRenderer::Render(
    media::AudioBus* audio_bus, int audio_delay_milliseconds) {
  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::Render");
  base::AutoLock auto_lock(thread_lock_);

  if (!playing_ || !volume_ || !loopback_fifo_) {
    audio_bus->Zero();
    return 0;
  }

  // Provide data by reading from the FIFO if the FIFO contains enough
  // to fulfill the request.
  if (loopback_fifo_->available_blocks()) {
    const media::AudioBus* audio_data = loopback_fifo_->Consume();
    DCHECK_EQ(audio_data->frames(), audio_bus->frames());
    audio_data->CopyTo(audio_bus);
  } else {
    audio_bus->Zero();
    // This warning is perfectly safe if it happens for the first audio
    // frames. It should not happen in a steady-state mode.
    DVLOG(2) << "loopback FIFO is empty";
  }

  return audio_bus->frames();
}

void WebRtcLocalAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
}

// content::MediaStreamAudioSink implementation
void WebRtcLocalAudioRenderer::OnData(const int16* audio_data,
                                      int sample_rate,
                                      int number_of_channels,
                                      int number_of_frames) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("audio", "WebRtcLocalAudioRenderer::CaptureData");
  base::AutoLock auto_lock(thread_lock_);
  if (!playing_ || !volume_ || !loopback_fifo_)
    return;

  // Push captured audio to FIFO so it can be read by a local sink.
  if (loopback_fifo_->GetUnfilledFrames() >= number_of_frames) {
    loopback_fifo_->Push(audio_data, number_of_frames, sizeof(audio_data[0]));

    const base::TimeTicks now = base::TimeTicks::Now();
    total_render_time_ += now - last_render_time_;
    last_render_time_ = now;
  } else {
    DVLOG(1) << "FIFO is full";
  }
}

void WebRtcLocalAudioRenderer::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::OnSetFormat()";
  // If the source is restarted, we might have changed to another capture
  // thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  // Post a task on the main render thread to reconfigure the |sink_| with the
  // new format.
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcLocalAudioRenderer::ReconfigureSink, this,
                 params));
}

// WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer implementation.
WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int source_render_view_id,
    int source_render_frame_id,
    int session_id,
    int frames_per_buffer)
    : audio_track_(audio_track),
      source_render_view_id_(source_render_view_id),
      source_render_frame_id_(source_render_frame_id),
      session_id_(session_id),
      message_loop_(base::MessageLoopProxy::current()),
      playing_(false),
      frames_per_buffer_(frames_per_buffer),
      volume_(0.0),
      sink_started_(false) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::WebRtcLocalAudioRenderer()";
}

WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!sink_.get());
  DVLOG(1) << "WebRtcLocalAudioRenderer::~WebRtcLocalAudioRenderer()";
}

void WebRtcLocalAudioRenderer::Start() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Start()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  // We get audio data from |audio_track_|...
  MediaStreamAudioSink::AddToAudioTrack(this, audio_track_);
  // ...and |sink_| will get audio data from us.
  DCHECK(!sink_.get());
  sink_ = AudioDeviceFactory::NewOutputDevice(source_render_view_id_,
                                              source_render_frame_id_);

  base::AutoLock auto_lock(thread_lock_);
  last_render_time_ = base::TimeTicks::Now();
  playing_ = false;
}

void WebRtcLocalAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Stop()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    playing_ = false;
    loopback_fifo_.reset();
  }

  // Stop the output audio stream, i.e, stop asking for data to render.
  // It is safer to call Stop() on the |sink_| to clean up the resources even
  // when the |sink_| is never started.
  if (sink_.get()) {
    sink_->Stop();
    sink_ = NULL;
  }

  if (!sink_started_) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                              kSinkNeverStarted, kSinkStatesMax);
  }
  sink_started_ = false;

  // Ensure that the capturer stops feeding us with captured audio.
  MediaStreamAudioSink::RemoveFromAudioTrack(this, audio_track_);
}

void WebRtcLocalAudioRenderer::Play() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Play()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  {
    base::AutoLock auto_lock(thread_lock_);
    // Resumes rendering by ensuring that WebRtcLocalAudioRenderer::Render()
    // now reads data from the local FIFO.
    playing_ = true;
    last_render_time_ = base::TimeTicks::Now();
  }

  // Note: If volume_ is currently muted, the |sink_| will not be started yet.
  MaybeStartSink();
}

void WebRtcLocalAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcLocalAudioRenderer::Pause()";
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!sink_.get())
    return;

  base::AutoLock auto_lock(thread_lock_);
  // Temporarily suspends rendering audio.
  // WebRtcLocalAudioRenderer::Render() will return early during this state
  // and only zeros will be provided to the active sink.
  playing_ = false;
}

void WebRtcLocalAudioRenderer::SetVolume(float volume) {
  DVLOG(1) << "WebRtcLocalAudioRenderer::SetVolume(" << volume << ")";
  DCHECK(message_loop_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    // Cache the volume.
    volume_ = volume;
  }

  // Lazily start the |sink_| when the local renderer is unmuted during
  // playing.
  MaybeStartSink();

  if (sink_.get())
    sink_->SetVolume(volume);
}

base::TimeDelta WebRtcLocalAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(thread_lock_);
  if (!sink_.get())
    return base::TimeDelta();
  return total_render_time();
}

bool WebRtcLocalAudioRenderer::IsLocalRenderer() const {
  return true;
}

void WebRtcLocalAudioRenderer::MaybeStartSink() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DVLOG(1) << "WebRtcLocalAudioRenderer::MaybeStartSink()";

  if (!sink_.get() || !source_params_.IsValid())
    return;

  {
    // Clear up the old data in the FIFO.
    base::AutoLock auto_lock(thread_lock_);
    loopback_fifo_->Clear();
  }

  if (!sink_params_.IsValid() || !playing_ || !volume_ || sink_started_)
    return;

  DVLOG(1) << "WebRtcLocalAudioRenderer::MaybeStartSink() -- Starting sink_.";
  sink_->InitializeWithSessionId(sink_params_, this, session_id_);
  sink_->Start();
  sink_started_ = true;
  UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                            kSinkStarted, kSinkStatesMax);
}

void WebRtcLocalAudioRenderer::ReconfigureSink(
    const media::AudioParameters& params) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DVLOG(1) << "WebRtcLocalAudioRenderer::ReconfigureSink()";

  int implicit_ducking_effect = 0;
  RenderFrameImpl* const frame =
      RenderFrameImpl::FromRoutingID(source_render_frame_id_);
  MediaStreamDispatcher* const dispatcher = frame ?
      frame->GetMediaStreamDispatcher() : NULL;
  if (dispatcher && dispatcher->IsAudioDuckingActive()) {
    DVLOG(1) << "Forcing DUCKING to be ON for output";
    implicit_ducking_effect = media::AudioParameters::DUCKING;
  } else {
    DVLOG(1) << "DUCKING not forced ON for output";
  }

  if (source_params_.Equals(params))
    return;

  // Reset the |source_params_|, |sink_params_| and |loopback_fifo_| to match
  // the new format.

  source_params_ = params;

  sink_params_ = media::AudioParameters(source_params_.format(),
      source_params_.channel_layout(), source_params_.sample_rate(),
      source_params_.bits_per_sample(),
      WebRtcAudioRenderer::GetOptimalBufferSize(source_params_.sample_rate(),
                                                frames_per_buffer_),
      // If DUCKING is enabled on the source, it needs to be enabled on the
      // sink as well.
      source_params_.effects() | implicit_ducking_effect);

  {
    // TODO(henrika): we could add a more dynamic solution here but I prefer
    // a fixed size combined with bad audio at overflow. The alternative is
    // that we start to build up latency and that can be more difficult to
    // detect. Tests have shown that the FIFO never contains more than 2 or 3
    // audio frames but I have selected a max size of ten buffers just
    // in case since these tests were performed on a 16 core, 64GB Win 7
    // machine. We could also add some sort of error notifier in this area if
    // the FIFO overflows.
    const int blocks_of_buffers =
        10 * params.frames_per_buffer() / sink_params_.frames_per_buffer() + 1;
    media::AudioBlockFifo* new_fifo = new media::AudioBlockFifo(
        params.channels(), sink_params_.frames_per_buffer(), blocks_of_buffers);

    base::AutoLock auto_lock(thread_lock_);
    loopback_fifo_.reset(new_fifo);
  }

  if (!sink_.get())
    return;  // WebRtcLocalAudioRenderer has not yet been started.

  // Stop |sink_| and re-create a new one to be initialized with different audio
  // parameters.  Then, invoke MaybeStartSink() to restart everything again.
  if (sink_started_) {
    sink_->Stop();
    sink_started_ = false;
  }

  sink_ = AudioDeviceFactory::NewOutputDevice(source_render_view_id_,
                                              source_render_frame_id_);
  MaybeStartSink();
}

}  // namespace content
