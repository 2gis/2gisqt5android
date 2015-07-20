// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "content/browser/compositor/software_output_device_mac.h"

#include "content/browser/compositor/browser_compositor_ca_layer_tree_mac.h"
#include "ui/compositor/compositor.h"

namespace content {

SoftwareOutputDeviceMac::SoftwareOutputDeviceMac(ui::Compositor* compositor)
    : compositor_(compositor) {
}

SoftwareOutputDeviceMac::~SoftwareOutputDeviceMac() {
}

void SoftwareOutputDeviceMac::EndPaint(cc::SoftwareFrameData* frame_data) {
  SoftwareOutputDevice::EndPaint(frame_data);
  BrowserCompositorCALayerTreeMacGotSoftwareFrame(
      compositor_->widget(), frame_data, scale_factor_, canvas_.get());
}

}  // namespace content
