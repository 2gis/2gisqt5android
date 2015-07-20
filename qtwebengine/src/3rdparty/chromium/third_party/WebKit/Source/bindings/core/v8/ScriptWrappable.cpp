// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptWrappable.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

#if COMPILER(MSVC)
__declspec(align(4))
#endif
struct SameSizeAsScriptWrappableBase { };

COMPILE_ASSERT(sizeof(ScriptWrappableBase) <= sizeof(SameSizeAsScriptWrappableBase), ScriptWrappableBase_should_stay_small);

struct SameSizeAsScriptWrappable : public ScriptWrappableBase {
    virtual ~SameSizeAsScriptWrappable() { }
    v8::Object* m_wrapper;
};

COMPILE_ASSERT(sizeof(ScriptWrappable) <= sizeof(SameSizeAsScriptWrappable), ScriptWrappable_should_stay_small);

namespace {

class ScriptWrappableBaseProtector final {
    WTF_MAKE_NONCOPYABLE(ScriptWrappableBaseProtector);
public:
    ScriptWrappableBaseProtector(ScriptWrappableBase* scriptWrappableBase, const WrapperTypeInfo* wrapperTypeInfo)
        : m_scriptWrappableBase(scriptWrappableBase), m_wrapperTypeInfo(wrapperTypeInfo)
    {
        m_wrapperTypeInfo->refObject(m_scriptWrappableBase);
    }
    ~ScriptWrappableBaseProtector()
    {
        m_wrapperTypeInfo->derefObject(m_scriptWrappableBase);
    }

private:
    ScriptWrappableBase* m_scriptWrappableBase;
    const WrapperTypeInfo* m_wrapperTypeInfo;
};

} // namespace

// ScriptWrappableBase

v8::Handle<v8::Object> ScriptWrappableBase::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo)
{
    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    ScriptWrappableBaseProtector protect(this, wrapperTypeInfo);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, wrapperTypeInfo, this, isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperTypeInfo->installConditionallyEnabledProperties(wrapper, isolate);
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

// ScriptWrappable

v8::Handle<v8::Object> ScriptWrappable::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();

    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    ScriptWrappableBaseProtector protect(this, wrapperTypeInfo);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, wrapperTypeInfo, toScriptWrappableBase(), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperTypeInfo->installConditionallyEnabledProperties(wrapper, isolate);
    return associateWithWrapper(wrapperTypeInfo, wrapper, isolate);
}

v8::Handle<v8::Object> ScriptWrappable::associateWithWrapper(const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

} // namespace blink
