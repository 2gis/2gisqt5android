/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptDebugListener_h
#define ScriptDebugListener_h


#include "bindings/core/v8/ScriptState.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptValue;

enum CompileResult { CompileSuccess, CompileError };

class ScriptDebugListener {
public:
    class Script {
    public:
        Script()
            : startLine(0)
            , startColumn(0)
            , endLine(0)
            , endColumn(0)
            , isContentScript(false)
        {
        }

        String url;
        String sourceURL;
        String sourceMappingURL;
        String source;
        int startLine;
        int startColumn;
        int endLine;
        int endColumn;
        bool isContentScript;
    };

    enum SkipPauseRequest {
        NoSkip,
        Continue,
        StepInto,
        StepOut
    };

    virtual ~ScriptDebugListener() { }

    virtual void didParseSource(const String& scriptId, const Script&, CompileResult) = 0;
    virtual SkipPauseRequest didPause(ScriptState*, const ScriptValue& callFrames, const ScriptValue& exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection) = 0;
    virtual void didContinue() = 0;
    virtual bool v8AsyncTaskEventsEnabled() const = 0;
    virtual void didReceiveV8AsyncTaskEvent(ExecutionContext*, const String& eventType, const String& eventName, int id) = 0;
    virtual bool v8PromiseEventsEnabled() const = 0;
    virtual void didReceiveV8PromiseEvent(ScriptState*, v8::Handle<v8::Object> promise, v8::Handle<v8::Value> parentPromise, int status) = 0;
};

} // namespace blink


#endif // ScriptDebugListener_h
