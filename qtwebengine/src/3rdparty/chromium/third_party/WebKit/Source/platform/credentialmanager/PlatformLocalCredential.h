// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformLocalCredential_h
#define PlatformLocalCredential_h

#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformLocalCredential final : public PlatformCredential {
    WTF_MAKE_NONCOPYABLE(PlatformLocalCredential);
public:
    static PlatformLocalCredential* create(const String& id, const String& password, const String& name, const KURL& avatarURL);
    virtual ~PlatformLocalCredential();

    const String& password() const { return m_password; }

    virtual bool isLocal() override { return true; }

private:
    PlatformLocalCredential(const String& id, const String& password, const String& name, const KURL& avatarURL);

    String m_password;
};

} // namespace blink

#endif // PlatformLocalCredential_h
