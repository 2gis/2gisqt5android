// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_device_info.h"

namespace device {

const char kInvalidHidDeviceId[] = "";

HidDeviceInfo::HidDeviceInfo()
    : device_id(kInvalidHidDeviceId),
      vendor_id(0),
      product_id(0),
      bus_type(kHIDBusTypeUSB),
      has_report_id(false),
      max_input_report_size(0),
      max_output_report_size(0),
      max_feature_report_size(0) {
}

HidDeviceInfo::~HidDeviceInfo() {}

}  // namespace device
