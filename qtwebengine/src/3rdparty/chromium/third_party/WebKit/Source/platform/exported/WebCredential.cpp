// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebCredential.h"

#include "platform/credentialmanager/PlatformCredential.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebLocalCredential.h"

namespace blink {

WebCredential WebCredential::create(PlatformCredential* credential)
{
    if (credential->isLocal()) {
        WebLocalCredential local(credential);
        return local;
    }

    if (credential->isFederated()) {
        WebFederatedCredential federated(credential);
        return federated;
    }

    ASSERT_NOT_REACHED();
    return WebCredential(credential);
}

WebCredential::WebCredential(const WebString& id, const WebString& name, const WebURL& avatarURL)
    : m_platformCredential(PlatformCredential::create(id, name, avatarURL))
{
}

WebCredential::WebCredential(const WebCredential& other)
{
    assign(other);
}

void WebCredential::assign(const WebCredential& other)
{
    m_platformCredential = other.m_platformCredential;
}

WebCredential::WebCredential(PlatformCredential* credential)
    : m_platformCredential(credential)
{
}

WebCredential& WebCredential::operator=(PlatformCredential* credential)
{
    m_platformCredential = credential;
    return *this;
}

void WebCredential::reset()
{
    m_platformCredential.reset();
}

WebString WebCredential::id() const
{
    return m_platformCredential->id();
}

WebString WebCredential::name() const
{
    return m_platformCredential->name();
}

WebURL WebCredential::avatarURL() const
{
    return m_platformCredential->avatarURL();
}

bool WebCredential::isLocalCredential() const
{
    return m_platformCredential->isLocal();
}

bool WebCredential::isFederatedCredential() const
{
    return m_platformCredential->isFederated();
}

} // namespace blink
