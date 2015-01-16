// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include "base/logging.h"

namespace media {

DecryptConfig::DecryptConfig(const std::string& key_id,
                             const std::string& iv,
                             const std::vector<SubsampleEntry>& subsamples)
    : key_id_(key_id),
      iv_(iv),
      subsamples_(subsamples) {
  CHECK_GT(key_id.size(), 0u);
  CHECK(iv.size() == static_cast<size_t>(DecryptConfig::kDecryptionKeySize) ||
        iv.empty());
}

DecryptConfig::~DecryptConfig() {}

}  // namespace media
