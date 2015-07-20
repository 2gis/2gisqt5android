// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_types.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"

namespace media {

VideoCaptureFormat::VideoCaptureFormat()
    : frame_rate(0.0f), pixel_format(PIXEL_FORMAT_UNKNOWN) {}

VideoCaptureFormat::VideoCaptureFormat(const gfx::Size& frame_size,
                                       float frame_rate,
                                       VideoPixelFormat pixel_format)
    : frame_size(frame_size),
      frame_rate(frame_rate),
      pixel_format(pixel_format) {}

bool VideoCaptureFormat::IsValid() const {
  return (frame_size.width() < media::limits::kMaxDimension) &&
         (frame_size.height() < media::limits::kMaxDimension) &&
         (frame_size.GetArea() >= 0) &&
         (frame_size.GetArea() < media::limits::kMaxCanvas) &&
         (frame_rate >= 0.0f) &&
         (frame_rate < media::limits::kMaxFramesPerSecond) &&
         (pixel_format >= 0) &&
         (pixel_format < PIXEL_FORMAT_MAX);
}

std::string VideoCaptureFormat::ToString() const {
  return base::StringPrintf("resolution: %s, fps: %f, pixel format: %s",
                            frame_size.ToString().c_str(),
                            frame_rate,
                            PixelFormatToString(pixel_format).c_str());
}

std::string VideoCaptureFormat::PixelFormatToString(VideoPixelFormat format) {
  switch (format) {
  case PIXEL_FORMAT_UNKNOWN:
    return "UNKNOWN";
  case PIXEL_FORMAT_I420:
    return "I420";
  case PIXEL_FORMAT_YUY2:
    return "YUY2";
  case PIXEL_FORMAT_UYVY:
    return "UYUY";
  case PIXEL_FORMAT_RGB24:
    return "RGB24";
  case PIXEL_FORMAT_ARGB:
    return "ARGB";
  case PIXEL_FORMAT_MJPEG:
    return "MJPEG";
  case PIXEL_FORMAT_NV21:
    return "YV12";
  case PIXEL_FORMAT_YV12:
    return "YV12";
  case PIXEL_FORMAT_TEXTURE:
    return "TEXTURE";
  case PIXEL_FORMAT_MAX:
    break;
  }
  NOTREACHED() << "Invalid VideoPixelFormat provided: " << format;
  return "";
}

VideoCaptureParams::VideoCaptureParams()
    : resolution_change_policy(RESOLUTION_POLICY_FIXED) {}

ImageCaptureFormat::ImageCaptureFormat() : pixel_format(PIXEL_FORMAT_UNKNOWN) {
}

ImageCaptureFormat::ImageCaptureFormat(const gfx::Size& frame_size,
                                       VideoPixelFormat pixel_format)
    : frame_size(frame_size), pixel_format(pixel_format) {
}

}  // namespace media
