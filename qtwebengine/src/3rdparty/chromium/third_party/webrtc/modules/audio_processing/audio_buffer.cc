/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_buffer.h"

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {
namespace {

enum {
  kSamplesPer8kHzChannel = 80,
  kSamplesPer16kHzChannel = 160,
  kSamplesPer32kHzChannel = 320
};

bool HasKeyboardChannel(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }
  assert(false);
  return false;
}

int KeyboardChannelIndex(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      assert(false);
      return -1;
    case AudioProcessing::kMonoAndKeyboard:
      return 1;
    case AudioProcessing::kStereoAndKeyboard:
      return 2;
  }
  assert(false);
  return -1;
}


void StereoToMono(const float* left, const float* right, float* out,
                  int samples_per_channel) {
  for (int i = 0; i < samples_per_channel; ++i) {
    out[i] = (left[i] + right[i]) / 2;
  }
}

void StereoToMono(const int16_t* left, const int16_t* right, int16_t* out,
                  int samples_per_channel) {
  for (int i = 0; i < samples_per_channel; ++i) {
    out[i] = (left[i] + right[i]) >> 1;
  }
}

}  // namespace

// One int16_t and one float ChannelBuffer that are kept in sync. The sync is
// broken when someone requests write access to either ChannelBuffer, and
// reestablished when someone requests the outdated ChannelBuffer. It is
// therefore safe to use the return value of ibuf() and fbuf() until the next
// call to the other method.
class IFChannelBuffer {
 public:
  IFChannelBuffer(int samples_per_channel, int num_channels)
      : ivalid_(true),
        ibuf_(samples_per_channel, num_channels),
        fvalid_(true),
        fbuf_(samples_per_channel, num_channels) {}

  ChannelBuffer<int16_t>* ibuf() {
    RefreshI();
    fvalid_ = false;
    return &ibuf_;
  }

  ChannelBuffer<float>* fbuf() {
    RefreshF();
    ivalid_ = false;
    return &fbuf_;
  }

 private:
  void RefreshF() {
    if (!fvalid_) {
      assert(ivalid_);
      const int16_t* const int_data = ibuf_.data();
      float* const float_data = fbuf_.data();
      const int length = fbuf_.length();
      for (int i = 0; i < length; ++i)
        float_data[i] = int_data[i];
      fvalid_ = true;
    }
  }

  void RefreshI() {
    if (!ivalid_) {
      assert(fvalid_);
      const float* const float_data = fbuf_.data();
      int16_t* const int_data = ibuf_.data();
      const int length = ibuf_.length();
      for (int i = 0; i < length; ++i)
        int_data[i] = WEBRTC_SPL_SAT(std::numeric_limits<int16_t>::max(),
                                     float_data[i],
                                     std::numeric_limits<int16_t>::min());
      ivalid_ = true;
    }
  }

  bool ivalid_;
  ChannelBuffer<int16_t> ibuf_;
  bool fvalid_;
  ChannelBuffer<float> fbuf_;
};

class SplitChannelBuffer {
 public:
  SplitChannelBuffer(int samples_per_split_channel, int num_channels)
      : low_(samples_per_split_channel, num_channels),
        high_(samples_per_split_channel, num_channels) {
  }
  ~SplitChannelBuffer() {}

  int16_t* low_channel(int i) { return low_.ibuf()->channel(i); }
  int16_t* high_channel(int i) { return high_.ibuf()->channel(i); }
  float* low_channel_f(int i) { return low_.fbuf()->channel(i); }
  float* high_channel_f(int i) { return high_.fbuf()->channel(i); }

 private:
  IFChannelBuffer low_;
  IFChannelBuffer high_;
};

AudioBuffer::AudioBuffer(int input_samples_per_channel,
                         int num_input_channels,
                         int process_samples_per_channel,
                         int num_process_channels,
                         int output_samples_per_channel)
  : input_samples_per_channel_(input_samples_per_channel),
    num_input_channels_(num_input_channels),
    proc_samples_per_channel_(process_samples_per_channel),
    num_proc_channels_(num_process_channels),
    output_samples_per_channel_(output_samples_per_channel),
    samples_per_split_channel_(proc_samples_per_channel_),
    num_mixed_channels_(0),
    num_mixed_low_pass_channels_(0),
    reference_copied_(false),
    activity_(AudioFrame::kVadUnknown),
    keyboard_data_(NULL),
    channels_(new IFChannelBuffer(proc_samples_per_channel_,
                                  num_proc_channels_)) {
  assert(input_samples_per_channel_ > 0);
  assert(proc_samples_per_channel_ > 0);
  assert(output_samples_per_channel_ > 0);
  assert(num_input_channels_ > 0 && num_input_channels_ <= 2);
  assert(num_proc_channels_ <= num_input_channels);

  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    input_buffer_.reset(new ChannelBuffer<float>(input_samples_per_channel_,
                                                 num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_ ||
      output_samples_per_channel_ != proc_samples_per_channel_) {
    // Create an intermediate buffer for resampling.
    process_buffer_.reset(new ChannelBuffer<float>(proc_samples_per_channel_,
                                                   num_proc_channels_));
  }

  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    input_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_.push_back(
          new PushSincResampler(input_samples_per_channel_,
                                proc_samples_per_channel_));
    }
  }

  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    output_resamplers_.reserve(num_proc_channels_);
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_.push_back(
          new PushSincResampler(proc_samples_per_channel_,
                                output_samples_per_channel_));
    }
  }

  if (proc_samples_per_channel_ == kSamplesPer32kHzChannel) {
    samples_per_split_channel_ = kSamplesPer16kHzChannel;
    split_channels_.reset(new SplitChannelBuffer(samples_per_split_channel_,
                                                 num_proc_channels_));
    filter_states_.reset(new SplitFilterStates[num_proc_channels_]);
  }
}

AudioBuffer::~AudioBuffer() {}

void AudioBuffer::CopyFrom(const float* const* data,
                           int samples_per_channel,
                           AudioProcessing::ChannelLayout layout) {
  assert(samples_per_channel == input_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_input_channels_);
  InitForNewData();

  if (HasKeyboardChannel(layout)) {
    keyboard_data_ = data[KeyboardChannelIndex(layout)];
  }

  // Downmix.
  const float* const* data_ptr = data;
  if (num_input_channels_ == 2 && num_proc_channels_ == 1) {
    StereoToMono(data[0],
                 data[1],
                 input_buffer_->channel(0),
                 input_samples_per_channel_);
    data_ptr = input_buffer_->channels();
  }

  // Resample.
  if (input_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      input_resamplers_[i]->Resample(data_ptr[i],
                                     input_samples_per_channel_,
                                     process_buffer_->channel(i),
                                     proc_samples_per_channel_);
    }
    data_ptr = process_buffer_->channels();
  }

  // Convert to int16.
  for (int i = 0; i < num_proc_channels_; ++i) {
    ScaleAndRoundToInt16(data_ptr[i], proc_samples_per_channel_,
                         channels_->ibuf()->channel(i));
  }
}

void AudioBuffer::CopyTo(int samples_per_channel,
                         AudioProcessing::ChannelLayout layout,
                         float* const* data) {
  assert(samples_per_channel == output_samples_per_channel_);
  assert(ChannelsFromLayout(layout) == num_proc_channels_);

  // Convert to float.
  float* const* data_ptr = data;
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    // Convert to an intermediate buffer for subsequent resampling.
    data_ptr = process_buffer_->channels();
  }
  for (int i = 0; i < num_proc_channels_; ++i) {
    ScaleToFloat(channels_->ibuf()->channel(i),
                 proc_samples_per_channel_,
                 data_ptr[i]);
  }

  // Resample.
  if (output_samples_per_channel_ != proc_samples_per_channel_) {
    for (int i = 0; i < num_proc_channels_; ++i) {
      output_resamplers_[i]->Resample(data_ptr[i],
                                      proc_samples_per_channel_,
                                      data[i],
                                      output_samples_per_channel_);
    }
  }
}

void AudioBuffer::InitForNewData() {
  keyboard_data_ = NULL;
  num_mixed_channels_ = 0;
  num_mixed_low_pass_channels_ = 0;
  reference_copied_ = false;
  activity_ = AudioFrame::kVadUnknown;
}

const int16_t* AudioBuffer::data(int channel) const {
  assert(channel >= 0 && channel < num_proc_channels_);
  return channels_->ibuf()->channel(channel);
}

int16_t* AudioBuffer::data(int channel) {
  const AudioBuffer* t = this;
  return const_cast<int16_t*>(t->data(channel));
}

float* AudioBuffer::data_f(int channel) {
  assert(channel >= 0 && channel < num_proc_channels_);
  return channels_->fbuf()->channel(channel);
}

const int16_t* AudioBuffer::low_pass_split_data(int channel) const {
  assert(channel >= 0 && channel < num_proc_channels_);
  return split_channels_.get() ? split_channels_->low_channel(channel)
                               : data(channel);
}

int16_t* AudioBuffer::low_pass_split_data(int channel) {
  const AudioBuffer* t = this;
  return const_cast<int16_t*>(t->low_pass_split_data(channel));
}

float* AudioBuffer::low_pass_split_data_f(int channel) {
  assert(channel >= 0 && channel < num_proc_channels_);
  return split_channels_.get() ? split_channels_->low_channel_f(channel)
                               : data_f(channel);
}

const int16_t* AudioBuffer::high_pass_split_data(int channel) const {
  assert(channel >= 0 && channel < num_proc_channels_);
  return split_channels_.get() ? split_channels_->high_channel(channel) : NULL;
}

int16_t* AudioBuffer::high_pass_split_data(int channel) {
  const AudioBuffer* t = this;
  return const_cast<int16_t*>(t->high_pass_split_data(channel));
}

float* AudioBuffer::high_pass_split_data_f(int channel) {
  assert(channel >= 0 && channel < num_proc_channels_);
  return split_channels_.get() ? split_channels_->high_channel_f(channel)
                               : NULL;
}

const int16_t* AudioBuffer::mixed_data(int channel) const {
  assert(channel >= 0 && channel < num_mixed_channels_);

  return mixed_channels_->channel(channel);
}

const int16_t* AudioBuffer::mixed_low_pass_data(int channel) const {
  assert(channel >= 0 && channel < num_mixed_low_pass_channels_);

  return mixed_low_pass_channels_->channel(channel);
}

const int16_t* AudioBuffer::low_pass_reference(int channel) const {
  assert(channel >= 0 && channel < num_proc_channels_);
  if (!reference_copied_) {
    return NULL;
  }

  return low_pass_reference_channels_->channel(channel);
}

const float* AudioBuffer::keyboard_data() const {
  return keyboard_data_;
}

SplitFilterStates* AudioBuffer::filter_states(int channel) {
  assert(channel >= 0 && channel < num_proc_channels_);
  return &filter_states_[channel];
}

void AudioBuffer::set_activity(AudioFrame::VADActivity activity) {
  activity_ = activity;
}

AudioFrame::VADActivity AudioBuffer::activity() const {
  return activity_;
}

int AudioBuffer::num_channels() const {
  return num_proc_channels_;
}

int AudioBuffer::samples_per_channel() const {
  return proc_samples_per_channel_;
}

int AudioBuffer::samples_per_split_channel() const {
  return samples_per_split_channel_;
}

int AudioBuffer::samples_per_keyboard_channel() const {
  // We don't resample the keyboard channel.
  return input_samples_per_channel_;
}

// TODO(andrew): Do deinterleaving and mixing in one step?
void AudioBuffer::DeinterleaveFrom(AudioFrame* frame) {
  assert(proc_samples_per_channel_ == input_samples_per_channel_);
  assert(num_proc_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_proc_channels_);
  assert(frame->samples_per_channel_ ==  proc_samples_per_channel_);
  InitForNewData();
  activity_ = frame->vad_activity_;

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_proc_channels_; i++) {
    int16_t* deinterleaved = channels_->ibuf()->channel(i);
    int interleaved_idx = i;
    for (int j = 0; j < proc_samples_per_channel_; j++) {
      deinterleaved[j] = interleaved[interleaved_idx];
      interleaved_idx += num_proc_channels_;
    }
  }
}

void AudioBuffer::InterleaveTo(AudioFrame* frame, bool data_changed) const {
  assert(proc_samples_per_channel_ == output_samples_per_channel_);
  assert(num_proc_channels_ == num_input_channels_);
  assert(frame->num_channels_ == num_proc_channels_);
  assert(frame->samples_per_channel_ == proc_samples_per_channel_);
  frame->vad_activity_ = activity_;

  if (!data_changed) {
    return;
  }

  int16_t* interleaved = frame->data_;
  for (int i = 0; i < num_proc_channels_; i++) {
    int16_t* deinterleaved = channels_->ibuf()->channel(i);
    int interleaved_idx = i;
    for (int j = 0; j < proc_samples_per_channel_; j++) {
      interleaved[interleaved_idx] = deinterleaved[j];
      interleaved_idx += num_proc_channels_;
    }
  }
}

void AudioBuffer::CopyAndMix(int num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_proc_channels_ == 2);
  assert(num_mixed_channels == 1);
  if (!mixed_channels_.get()) {
    mixed_channels_.reset(
        new ChannelBuffer<int16_t>(proc_samples_per_channel_,
                                   num_mixed_channels));
  }

  StereoToMono(channels_->ibuf()->channel(0),
               channels_->ibuf()->channel(1),
               mixed_channels_->channel(0),
               proc_samples_per_channel_);

  num_mixed_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyAndMixLowPass(int num_mixed_channels) {
  // We currently only support the stereo to mono case.
  assert(num_proc_channels_ == 2);
  assert(num_mixed_channels == 1);
  if (!mixed_low_pass_channels_.get()) {
    mixed_low_pass_channels_.reset(
        new ChannelBuffer<int16_t>(samples_per_split_channel_,
                                   num_mixed_channels));
  }

  StereoToMono(low_pass_split_data(0),
               low_pass_split_data(1),
               mixed_low_pass_channels_->channel(0),
               samples_per_split_channel_);

  num_mixed_low_pass_channels_ = num_mixed_channels;
}

void AudioBuffer::CopyLowPassToReference() {
  reference_copied_ = true;
  if (!low_pass_reference_channels_.get()) {
    low_pass_reference_channels_.reset(
        new ChannelBuffer<int16_t>(samples_per_split_channel_,
                                   num_proc_channels_));
  }
  for (int i = 0; i < num_proc_channels_; i++) {
    low_pass_reference_channels_->CopyFrom(low_pass_split_data(i), i);
  }
}

}  // namespace webrtc
