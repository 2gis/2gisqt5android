/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorConsoleAgent_h
#define InspectorConsoleAgent_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptString.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/frame/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;
class DocumentLoader;
class LocalFrame;
class InspectorFrontend;
class InjectedScriptManager;
class InspectorTimelineAgent;
class ScriptProfile;
class ThreadableLoaderClient;
class XMLHttpRequest;

typedef String ErrorString;

class InspectorConsoleAgent : public InspectorBaseAgent<InspectorConsoleAgent>, public InspectorBackendDispatcher::ConsoleCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorConsoleAgent);
public:
    InspectorConsoleAgent(InspectorTimelineAgent*, InjectedScriptManager*);
    virtual ~InspectorConsoleAgent();
    virtual void trace(Visitor*) override;

    virtual void enable(ErrorString*) override final;
    virtual void disable(ErrorString*) override final;
    virtual void clearMessages(ErrorString*) override;
    bool enabled() { return m_enabled; }

    virtual void setFrontend(InspectorFrontend*) override final;
    virtual void clearFrontend() override final;
    virtual void restore() override final;

    void addMessageToConsole(ConsoleMessage*);
    void consoleMessagesCleared();

    void setTracingBasedTimeline(ErrorString*, bool enabled);
    void consoleTimeline(ExecutionContext*, const String& title, ScriptState*);
    void consoleTimelineEnd(ExecutionContext*, const String& title, ScriptState*);

    void didCommitLoad(LocalFrame*, DocumentLoader*);

    void didFinishXHRLoading(XMLHttpRequest*, ThreadableLoaderClient*, unsigned long requestIdentifier, ScriptString, const AtomicString& method, const String& url);
    void addProfileFinishedMessageToConsole(PassRefPtrWillBeRawPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    virtual void setMonitoringXHREnabled(ErrorString*, bool enabled) override;
    virtual void addInspectedNode(ErrorString*, int nodeId) = 0;
    virtual void addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId) override;

    virtual bool isWorkerAgent() = 0;

    virtual void setLastEvaluationResult(ErrorString*, const String& objectId) override;

protected:
    void sendConsoleMessageToFrontend(ConsoleMessage*, bool generatePreview);
    virtual ConsoleMessageStorage* messageStorage() = 0;

    RawPtrWillBeMember<InspectorTimelineAgent> m_timelineAgent;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    InspectorFrontend::Console* m_frontend;
    bool m_enabled;
private:
    static int s_enabledAgentCount;
};

} // namespace blink


#endif // !defined(InspectorConsoleAgent_h)
