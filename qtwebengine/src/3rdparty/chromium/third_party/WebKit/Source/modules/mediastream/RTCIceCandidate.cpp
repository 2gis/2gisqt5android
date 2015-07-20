/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "config.h"
#include "modules/mediastream/RTCIceCandidate.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

RTCIceCandidate* RTCIceCandidate::create(const Dictionary& dictionary, ExceptionState& exceptionState)
{
    String candidate;
    bool ok = DictionaryHelper::get(dictionary, "candidate", candidate);
    if (!ok || !candidate.length()) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::incorrectPropertyType("candidate", "is not a string, or is empty."));
        return nullptr;
    }

    String sdpMid;
    DictionaryHelper::get(dictionary, "sdpMid", sdpMid);

    unsigned short sdpMLineIndex = 0;
    DictionaryHelper::get(dictionary, "sdpMLineIndex", sdpMLineIndex);

    return new RTCIceCandidate(WebRTCICECandidate(candidate, sdpMid, sdpMLineIndex));
}

RTCIceCandidate* RTCIceCandidate::create(WebRTCICECandidate webCandidate)
{
    return new RTCIceCandidate(webCandidate);
}

RTCIceCandidate::RTCIceCandidate(WebRTCICECandidate webCandidate)
    : m_webCandidate(webCandidate)
{
}

String RTCIceCandidate::candidate() const
{
    return m_webCandidate.candidate();
}

String RTCIceCandidate::sdpMid() const
{
    return m_webCandidate.sdpMid();
}

unsigned short RTCIceCandidate::sdpMLineIndex() const
{
    return m_webCandidate.sdpMLineIndex();
}

WebRTCICECandidate RTCIceCandidate::webCandidate() const
{
    return m_webCandidate;
}

void RTCIceCandidate::setCandidate(String candidate)
{
    m_webCandidate.setCandidate(candidate);
}

void RTCIceCandidate::setSdpMid(String sdpMid)
{
    m_webCandidate.setSdpMid(sdpMid);
}

void RTCIceCandidate::setSdpMLineIndex(unsigned short sdpMLineIndex)
{
    m_webCandidate.setSdpMLineIndex(sdpMLineIndex);
}

} // namespace blink
