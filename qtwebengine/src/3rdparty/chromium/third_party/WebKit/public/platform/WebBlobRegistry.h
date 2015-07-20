/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebBlobRegistry_h
#define WebBlobRegistry_h

#include "WebCommon.h"

namespace blink {

class WebBlobData;
class WebString;
class WebURL;

class WebBlobRegistry {
public:
    virtual ~WebBlobRegistry() { }

    virtual void registerBlobData(const WebString& uuid, const WebBlobData&) { }
    virtual void addBlobDataRef(const WebString& uuid) { }
    virtual void removeBlobDataRef(const WebString& uuid) { }
    virtual void registerPublicBlobURL(const WebURL&, const WebString& uuid) { }
    virtual void revokePublicBlobURL(const WebURL&) { }

    // Registers a stream URL referring to a stream with the specified media
    // type.
    virtual void registerStreamURL(const WebURL&, const WebString&) { BLINK_ASSERT_NOT_REACHED(); }

    // Registers a stream URL referring to the stream identified by the
    // specified srcURL.
    virtual void registerStreamURL(const WebURL&, const WebURL& srcURL) { BLINK_ASSERT_NOT_REACHED(); };

    // Add data to the stream referred by the URL.
    virtual void addDataToStream(const WebURL&, const char* data, size_t length) { BLINK_ASSERT_NOT_REACHED(); }

    // Tell the registry that construction of this stream has completed
    // successfully and so it won't receive any more data.
    virtual void finalizeStream(const WebURL&) { BLINK_ASSERT_NOT_REACHED(); }

    // Tell the registry that construction of this stream has been aborted and
    // so it won't receive any more data.
    virtual void abortStream(const WebURL&) { BLINK_ASSERT_NOT_REACHED(); }

    // Unregisters a stream referred by the URL.
    virtual void unregisterStreamURL(const WebURL&) { BLINK_ASSERT_NOT_REACHED(); }
};

} // namespace blink

#endif // WebBlobRegistry_h
