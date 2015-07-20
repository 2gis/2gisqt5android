// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/chromecast_switches.h"

namespace switches {

#if defined(OS_ANDROID)
// Enable file accesses for debug.
const char kEnableLocalFileAccesses[] = "enable-local-file-accesses";
#endif  // defined(OS_ANDROID)

// Override the URL to which metrics logs are sent for debugging.
const char kOverrideMetricsUploadUrl[] = "override-metrics-upload-url";

}  // namespace switches
