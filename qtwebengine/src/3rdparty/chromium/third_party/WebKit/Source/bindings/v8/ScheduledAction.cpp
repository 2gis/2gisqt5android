/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/v8/ScheduledAction.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/TraceEvent.h"

namespace WebCore {

ScheduledAction::ScheduledAction(ScriptState* scriptState, v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[], v8::Isolate* isolate)
    : m_scriptState(scriptState)
    , m_function(isolate, function)
    , m_info(isolate)
    , m_code(String(), KURL(), TextPosition::belowRangePosition())
{
    m_info.ReserveCapacity(argc);
    for (int i = 0; i < argc; ++i)
        m_info.Append(argv[i]);
}

ScheduledAction::ScheduledAction(ScriptState* scriptState, const String& code, const KURL& url, v8::Isolate* isolate)
    : m_scriptState(scriptState)
    , m_info(isolate)
    , m_code(code, url)
{
}

ScheduledAction::~ScheduledAction()
{
}

void ScheduledAction::execute(ExecutionContext* context)
{
    if (context->isDocument()) {
        LocalFrame* frame = toDocument(context)->frame();
        if (!frame)
            return;
        if (!frame->script().canExecuteScripts(AboutToExecuteScript))
            return;
        execute(frame);
    } else {
        execute(toWorkerGlobalScope(context));
    }
}

void ScheduledAction::execute(LocalFrame* frame)
{
    if (m_scriptState->contextIsEmpty())
        return;

    TRACE_EVENT0("v8", "ScheduledAction::execute");
    ScriptState::Scope scope(m_scriptState.get());
    if (!m_function.isEmpty()) {
        Vector<v8::Handle<v8::Value> > info;
        createLocalHandlesForArgs(&info);
        frame->script().callFunction(m_function.newLocal(m_scriptState->isolate()), m_scriptState->context()->Global(), info.size(), info.data());
    } else {
        frame->script().executeScriptAndReturnValue(m_scriptState->context(), ScriptSourceCode(m_code));
    }

    // The frame might be invalid at this point because JavaScript could have released it.
}

void ScheduledAction::execute(WorkerGlobalScope* worker)
{
    ASSERT(worker->thread()->isCurrentThread());
    ASSERT(!m_scriptState->contextIsEmpty());
    if (!m_function.isEmpty()) {
        ScriptState::Scope scope(m_scriptState.get());
        Vector<v8::Handle<v8::Value> > info;
        createLocalHandlesForArgs(&info);
        V8ScriptRunner::callFunction(m_function.newLocal(m_scriptState->isolate()), worker, m_scriptState->context()->Global(), info.size(), info.data(), m_scriptState->isolate());
    } else {
        worker->script()->evaluate(m_code);
    }
}

void ScheduledAction::createLocalHandlesForArgs(Vector<v8::Handle<v8::Value> >* handles)
{
    handles->reserveCapacity(m_info.Size());
    for (size_t i = 0; i < m_info.Size(); ++i)
        handles->append(m_info.Get(i));
}

} // namespace WebCore
