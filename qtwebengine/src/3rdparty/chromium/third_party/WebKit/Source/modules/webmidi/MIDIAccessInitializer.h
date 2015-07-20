// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIAccessInitializer_h
#define MIDIAccessInitializer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIPort.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class MIDIOptions;
class ScriptState;

class MIDIAccessInitializer : public ScriptPromiseResolver, public MIDIAccessorClient {
public:
    struct PortDescriptor {
        String id;
        String manufacturer;
        String name;
        MIDIPort::MIDIPortTypeCode type;
        String version;
        bool isActive;

        PortDescriptor(const String& id, const String& manufacturer, const String& name, MIDIPort::MIDIPortTypeCode type, const String& version, bool isActive)
            : id(id)
            , manufacturer(manufacturer)
            , name(name)
            , type(type)
            , version(version)
            , isActive(isActive) { }
    };

    static ScriptPromise start(ScriptState* scriptState, const MIDIOptions& options)
    {
        RefPtr<MIDIAccessInitializer> p = adoptRef(new MIDIAccessInitializer(scriptState, options));
        p->keepAliveWhilePending();
        p->suspendIfNeeded();
        return p->start();
    }

    virtual ~MIDIAccessInitializer();

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive) override;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version, bool isActive) override;
    virtual void didSetInputPortState(unsigned portIndex, bool isActive) override;
    virtual void didSetOutputPortState(unsigned portIndex, bool isActive) override;
    virtual void didStartSession(bool success, const String& error, const String& message) override;
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) override { }

    void resolveSysexPermission(bool allowed);
    SecurityOrigin* securityOrigin() const;

private:
    ScriptPromise start();

    MIDIAccessInitializer(ScriptState*, const MIDIOptions&);

    ExecutionContext* executionContext() const;

    OwnPtr<MIDIAccessor> m_accessor;
    bool m_requestSysex;
    Vector<PortDescriptor> m_portDescriptors;
};

} // namespace blink


#endif // MIDIAccessInitializer_h
