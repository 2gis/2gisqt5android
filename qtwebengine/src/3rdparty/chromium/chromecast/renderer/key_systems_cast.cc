// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/key_systems_cast.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "chromecast/media/base/key_systems_common.h"
#include "components/cdm/renderer/widevine_key_systems.h"
#include "media/base/eme_constants.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

namespace chromecast {
namespace shell {

void AddKeySystemWithCodecs(
    const std::string& key_system_name,
    std::vector< ::media::KeySystemInfo>* concrete_key_systems) {
  ::media::KeySystemInfo info(key_system_name);
  info.supported_codecs = ::media::EME_CODEC_MP4_ALL;
  concrete_key_systems->push_back(info);
}

void AddChromecastKeySystems(
    std::vector< ::media::KeySystemInfo>* key_systems_info) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  AddWidevineWithCodecs(cdm::WIDEVINE,
                        ::media::EME_CODEC_MP4_ALL,
                        key_systems_info);
#endif

#if defined(PLAYREADY_CDM_AVAILABLE)
  AddKeySystemWithCodecs(media::kChromecastPlayreadyKeySystem,
                         key_systems_info);
#endif
}

}  // namespace shell
}  // namespace chromecast
