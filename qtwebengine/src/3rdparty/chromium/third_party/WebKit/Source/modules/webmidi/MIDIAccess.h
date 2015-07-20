/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef MIDIAccess_h
#define MIDIAccess_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class ExecutionContext;
class MIDIInput;
class MIDIInputMap;
class MIDIOutput;
class MIDIOutputMap;

class MIDIAccess final : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<MIDIAccess>, public ActiveDOMObject, public EventTargetWithInlineData, public MIDIAccessorClient {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<MIDIAccess>);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MIDIAccess);
public:
    static MIDIAccess* create(PassOwnPtr<MIDIAccessor> accessor, bool sysexEnabled, const Vector<MIDIAccessInitializer::PortDescriptor>& ports, ExecutionContext* executionContext)
    {
        MIDIAccess* access = new MIDIAccess(accessor, sysexEnabled, ports, executionContext);
        access->suspendIfNeeded();
        return access;
    }
    virtual ~MIDIAccess();

    MIDIInputMap* inputs() const;
    MIDIOutputMap* outputs() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    bool sysexEnabled() const { return m_sysexEnabled; }

    // EventTarget
    virtual const AtomicString& interfaceName() const override { return EventTargetNames::MIDIAccess; }
    virtual ExecutionContext* executionContext() const override { return ActiveDOMObject::executionContext(); }

    // ActiveDOMObject
    virtual void stop() override;

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive) override;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive) override;
    virtual void didSetInputPortState(unsigned portIndex, bool isActive) override;
    virtual void didSetOutputPortState(unsigned portIndex, bool isActive) override;
    virtual void didStartSession(bool success, const String& error, const String& message) override
    {
        // This method is for MIDIAccess initialization: MIDIAccessInitializer
        // has the implementation.
        ASSERT_NOT_REACHED();
    }
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) override;

    // |timeStampInMilliseconds| is in the same time coordinate system as performance.now().
    void sendMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStampInMilliseconds);

    virtual void trace(Visitor*) override;

private:
    MIDIAccess(PassOwnPtr<MIDIAccessor>, bool sysexEnabled, const Vector<MIDIAccessInitializer::PortDescriptor>&, ExecutionContext*);

    OwnPtr<MIDIAccessor> m_accessor;
    bool m_sysexEnabled;
    HeapVector<Member<MIDIInput> > m_inputs;
    HeapVector<Member<MIDIOutput> > m_outputs;
};

} // namespace blink

#endif // MIDIAccess_h
