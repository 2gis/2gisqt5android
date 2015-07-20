// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFederatedCredential_h
#define WebFederatedCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

class PlatformFederatedCredential;

class WebFederatedCredential : public WebCredential {
public:
    BLINK_PLATFORM_EXPORT WebFederatedCredential(const WebString& id, const WebURL& federation, const WebString& name, const WebURL& avatarURL);

    // FIXME: Throw this away once it's unused on the chromium side.
    BLINK_PLATFORM_EXPORT WebFederatedCredential(const WebString& id, const WebString& name, const WebURL& avatarURL, const WebURL& federation);

    BLINK_PLATFORM_EXPORT void assign(const WebFederatedCredential&);

    BLINK_PLATFORM_EXPORT WebURL federation() const;

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebFederatedCredential(PlatformCredential*);
    BLINK_PLATFORM_EXPORT WebFederatedCredential& operator=(PlatformCredential*);
#endif
};

} // namespace blink

#endif // WebFederatedCredential_h


