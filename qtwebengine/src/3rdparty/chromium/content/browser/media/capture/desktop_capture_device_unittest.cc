// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include <string>

#include "base/basictypes.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace content {

namespace {

MATCHER_P2(EqualsCaptureCapability, width, height, "") {
  return arg.width == width && arg.height == height;
}

const int kTestFrameWidth1 = 100;
const int kTestFrameHeight1 = 100;
const int kTestFrameWidth2 = 200;
const int kTestFrameHeight2 = 150;

const int kFrameRate = 30;

class MockDeviceClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD2(ReserveOutputBuffer,
               scoped_refptr<Buffer>(media::VideoFrame::Format format,
                                     const gfx::Size& dimensions));
  MOCK_METHOD1(OnError, void(const std::string& reason));
  MOCK_METHOD5(OnIncomingCapturedData,
               void(const uint8* data,
                    int length,
                    const media::VideoCaptureFormat& frame_format,
                    int rotation,
                    base::TimeTicks timestamp));
  MOCK_METHOD4(OnIncomingCapturedVideoFrame,
               void(const scoped_refptr<Buffer>& buffer,
                    const media::VideoCaptureFormat& buffer_format,
                    const scoped_refptr<media::VideoFrame>& frame,
                    base::TimeTicks timestamp));
};

// DesktopFrame wrapper that flips wrapped frame upside down by inverting
// stride.
class InvertedDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Takes ownership of |frame|.
  explicit InvertedDesktopFrame(webrtc::DesktopFrame* frame)
      : webrtc::DesktopFrame(
            frame->size(),
            -frame->stride(),
            frame->data() + (frame->size().height() - 1) * frame->stride(),
            frame->shared_memory()),
        original_frame_(frame) {
    set_dpi(frame->dpi());
    set_capture_time_ms(frame->capture_time_ms());
    mutable_updated_region()->Swap(frame->mutable_updated_region());
  }
  ~InvertedDesktopFrame() override {}

 private:
  scoped_ptr<webrtc::DesktopFrame> original_frame_;

  DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

// TODO(sergeyu): Move this to a separate file where it can be reused.
class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  FakeScreenCapturer()
      : callback_(NULL),
        frame_index_(0),
        generate_inverted_frames_(false) {
  }
  ~FakeScreenCapturer() override {}

  void set_generate_inverted_frames(bool generate_inverted_frames) {
    generate_inverted_frames_ = generate_inverted_frames;
  }

  // VideoFrameCapturer interface.
  void Start(Callback* callback) override { callback_ = callback; }

  void Capture(const webrtc::DesktopRegion& region) override {
    webrtc::DesktopSize size;
    if (frame_index_ % 2 == 0) {
      size = webrtc::DesktopSize(kTestFrameWidth1, kTestFrameHeight1);
    } else {
      size = webrtc::DesktopSize(kTestFrameWidth2, kTestFrameHeight2);
    }
    frame_index_++;

    webrtc::DesktopFrame* frame = new webrtc::BasicDesktopFrame(size);
    if (generate_inverted_frames_)
      frame = new InvertedDesktopFrame(frame);
    callback_->OnCaptureCompleted(frame);
  }

  void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) override {}

  bool GetScreenList(ScreenList* screens) override { return false; }

  bool SelectScreen(webrtc::ScreenId id) override { return false; }

 private:
  Callback* callback_;
  int frame_index_;
  bool generate_inverted_frames_;
};

}  // namespace

class DesktopCaptureDeviceTest : public testing::Test {
 public:
  void CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer> capturer) {
    capture_device_.reset(
        new DesktopCaptureDevice(capturer.Pass(), DesktopMediaID::TYPE_SCREEN));
  }

 protected:
  scoped_ptr<DesktopCaptureDevice> capture_device_;
};

// There is currently no screen capturer implementation for ozone. So disable
// the test that uses a real screen-capturer instead of FakeScreenCapturer.
// http://crbug.com/260318
#if defined(USE_OZONE)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
TEST_F(DesktopCaptureDeviceTest, MAYBE_Capture) {
  scoped_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::ScreenCapturer::Create());
  CreateScreenCaptureDevice(capturer.Pass());

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(SaveArg<1>(&frame_size),
            SaveArg<2>(&format),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  capture_device_->AllocateAndStart(capture_params, client.Pass());
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  capture_device_->StopAndDeAllocate();

  EXPECT_GT(format.frame_size.width(), 0);
  EXPECT_GT(format.frame_size.height(), 0);
  EXPECT_EQ(kFrameRate, format.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);

  EXPECT_EQ(format.frame_size.GetArea() * 4, frame_size);
}

// Test that screen capturer behaves correctly if the source frame size changes
// but the caller cannot cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeConstantResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);
  int frame_size;

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(SaveArg<1>(&frame_size),
            SaveArg<2>(&format),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth1,
                                                     kTestFrameHeight1);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(capture_params, client.Pass());

  // Capture at least two frames, to ensure that the source frame size has
  // changed while capturing.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));

  capture_device_->StopAndDeAllocate();

  EXPECT_EQ(kTestFrameWidth1, format.frame_size.width());
  EXPECT_EQ(kTestFrameHeight1, format.frame_size.height());
  EXPECT_EQ(kFrameRate, format.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);

  EXPECT_EQ(format.frame_size.GetArea() * 4, frame_size);
}

// Test that screen capturer behaves correctly if the source frame size changes
// and the caller can cope with variable resolution output.
TEST_F(DesktopCaptureDeviceTest, ScreenResolutionChangeVariableResolution) {
  FakeScreenCapturer* mock_capturer = new FakeScreenCapturer();

  CreateScreenCaptureDevice(scoped_ptr<webrtc::DesktopCapturer>(mock_capturer));

  media::VideoCaptureFormat format;
  base::WaitableEvent done_event(false, false);

  scoped_ptr<MockDeviceClient> client(new MockDeviceClient());
  EXPECT_CALL(*client, OnError(_)).Times(0);
  EXPECT_CALL(*client, OnIncomingCapturedData(_, _, _, _, _)).WillRepeatedly(
      DoAll(SaveArg<2>(&format),
            InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal)));

  media::VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(kTestFrameWidth2,
                                                     kTestFrameHeight2);
  capture_params.requested_format.frame_rate = kFrameRate;
  capture_params.requested_format.pixel_format = media::PIXEL_FORMAT_I420;

  capture_device_->AllocateAndStart(
      capture_params, client.Pass());

  // Capture at least three frames, to ensure that the source frame size has
  // changed at least twice while capturing.
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));
  done_event.Reset();
  EXPECT_TRUE(done_event.TimedWait(TestTimeouts::action_max_timeout()));

  capture_device_->StopAndDeAllocate();

  EXPECT_EQ(kTestFrameWidth1, format.frame_size.width());
  EXPECT_EQ(kTestFrameHeight1, format.frame_size.height());
  EXPECT_EQ(kFrameRate, format.frame_rate);
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, format.pixel_format);
}

}  // namespace content
