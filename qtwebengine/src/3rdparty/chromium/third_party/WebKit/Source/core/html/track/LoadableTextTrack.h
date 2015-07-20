/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LoadableTextTrack_h
#define LoadableTextTrack_h

#include "core/html/track/TextTrack.h"
#include "core/loader/TextTrackLoader.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class HTMLTrackElement;
class LoadableTextTrack;

class LoadableTextTrack final : public TextTrack, private TextTrackLoaderClient {
public:
    static PassRefPtrWillBeRawPtr<LoadableTextTrack> create(HTMLTrackElement* track)
    {
        return adoptRefWillBeNoop(new LoadableTextTrack(track));
    }
    virtual ~LoadableTextTrack();

    void scheduleLoad(const KURL&);

    // TextTrack method.
    virtual void setMode(const AtomicString&) override;

    size_t trackElementIndex();
    HTMLTrackElement* trackElement() { return m_trackElement; }
#if !ENABLE(OILPAN)
    void clearTrackElement();
#endif

    virtual bool isDefault() const override { return m_isDefault; }
    virtual void setIsDefault(bool isDefault) override  { m_isDefault = isDefault; }

    virtual void trace(Visitor*) override;

private:
    // TextTrackLoaderClient
    virtual void newCuesAvailable(TextTrackLoader*) override;
    virtual void cueLoadingCompleted(TextTrackLoader*, bool loadingFailed) override;
    virtual void newRegionsAvailable(TextTrackLoader*) override;

    explicit LoadableTextTrack(HTMLTrackElement*);

    void loadTimerFired(Timer<LoadableTextTrack>*);

    RawPtrWillBeMember<HTMLTrackElement> m_trackElement;
    Timer<LoadableTextTrack> m_loadTimer;
    OwnPtrWillBeMember<TextTrackLoader> m_loader;
    KURL m_url;
    bool m_isDefault;
};
} // namespace blink

#endif
