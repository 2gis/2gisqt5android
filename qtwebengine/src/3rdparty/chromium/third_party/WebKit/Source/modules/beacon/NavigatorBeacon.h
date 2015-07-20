// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBeacon_h
#define NavigatorBeacon_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Blob;
class DOMArrayBufferView;
class DOMFormData;
class ExceptionState;
class ExecutionContext;
class KURL;

class NavigatorBeacon final : public NoBaseWillBeGarbageCollected<NavigatorBeacon>, public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorBeacon);
public:
    static NavigatorBeacon& from(Navigator&);

    static bool sendBeacon(ExecutionContext*, Navigator&, const String&, const String&, ExceptionState&);
    static bool sendBeacon(ExecutionContext*, Navigator&, const String&, PassRefPtr<DOMArrayBufferView>, ExceptionState&);
    static bool sendBeacon(ExecutionContext*, Navigator&, const String&, Blob*, ExceptionState&);
    static bool sendBeacon(ExecutionContext*, Navigator&, const String&, PassRefPtrWillBeRawPtr<DOMFormData>, ExceptionState&);

private:
    explicit NavigatorBeacon(Navigator&);

    static const char* supplementName();

    bool sendBeacon(ExecutionContext*, const String&, const String&, ExceptionState&);
    bool sendBeacon(ExecutionContext*, const String&, PassRefPtr<DOMArrayBufferView>, ExceptionState&);
    bool sendBeacon(ExecutionContext*, const String&, Blob*, ExceptionState&);
    bool sendBeacon(ExecutionContext*, const String&, PassRefPtrWillBeRawPtr<DOMFormData>, ExceptionState&);

    bool canSendBeacon(ExecutionContext*, const KURL&, ExceptionState&);
    int maxAllowance() const;
    bool beaconResult(ExecutionContext*, bool allowed, int sentBytes);

    int m_transmittedBytes;
    Navigator& m_navigator;
};

} // namespace blink

#endif // NavigatorBeacon_h
