// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClient_h
#define ServiceWorkerClient_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class ServiceWorkerClient final : public GarbageCollected<ServiceWorkerClient>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorkerClient* create(unsigned id);

    // ServiceWorkerClient.idl
    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    void trace(Visitor*) { }

private:
    explicit ServiceWorkerClient(unsigned id);

    unsigned m_id;
};

} // namespace blink

#endif // ServiceWorkerClient_h
