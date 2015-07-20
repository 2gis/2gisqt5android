// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Presentation_h
#define Presentation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/presentation/PresentationSession.h"

namespace blink {

class ScriptState;

class Presentation final
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<Presentation>
    , public EventTargetWithInlineData
    , public ContextLifecycleObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<Presentation>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Presentation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Presentation* create(ExecutionContext*);
    virtual ~Presentation();

    // EventTarget implementation.
    virtual const AtomicString& interfaceName() const override;
    virtual ExecutionContext* executionContext() const override;

    virtual void trace(Visitor*) override;

    PresentationSession* session() const;

    ScriptPromise startSession(ScriptState*, const String& senderId, const String& presentationId);
    ScriptPromise joinSession(ScriptState*, const String& senderId, const String& presentationId);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(availablechange);

private:
    explicit Presentation(ExecutionContext*);

    Member<PresentationSession> m_session;
};

} // namespace blink

#endif // Presentation_h
