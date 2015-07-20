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

#ifndef AsyncCallStackTracker_h
#define AsyncCallStackTracker_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class Event;
class EventListener;
class EventTarget;
class ExecutionContext;
class ExecutionContextTask;
class MutationObserver;
class XMLHttpRequest;

class AsyncCallStackTracker final : public NoBaseWillBeGarbageCollectedFinalized<AsyncCallStackTracker> {
    WTF_MAKE_NONCOPYABLE(AsyncCallStackTracker);
public:
    class AsyncCallStack final : public RefCountedWillBeGarbageCollectedFinalized<AsyncCallStack> {
    public:
        AsyncCallStack(const String&, const ScriptValue&);
        ~AsyncCallStack();
        void trace(Visitor*) { }
        String description() const { return m_description; }
        ScriptValue callFrames() const { return m_callFrames; }
    private:
        String m_description;
        ScriptValue m_callFrames;
    };

    typedef WillBeHeapDeque<RefPtrWillBeMember<AsyncCallStack>, 4> AsyncCallStackVector;

    class AsyncCallChain final : public RefCountedWillBeGarbageCollected<AsyncCallChain> {
    public:
        AsyncCallChain() { }
        AsyncCallChain(const AsyncCallChain& t) : m_callStacks(t.m_callStacks) { }
        AsyncCallStackVector callStacks() const { return m_callStacks; }
        void trace(Visitor*);
    private:
        friend class AsyncCallStackTracker;
        AsyncCallStackVector m_callStacks;
    };

    class ExecutionContextData final : public NoBaseWillBeGarbageCollectedFinalized<ExecutionContextData>, public ContextLifecycleObserver {
        WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
    public:
        ExecutionContextData(AsyncCallStackTracker* tracker, ExecutionContext* executionContext)
            : ContextLifecycleObserver(executionContext)
            , m_circularSequentialID(0)
            , m_tracker(tracker)
        {
        }

        virtual void contextDestroyed() override;

        int circularSequentialID();

        void trace(Visitor*);

    private:
        int m_circularSequentialID;

    public:
        RawPtrWillBeMember<AsyncCallStackTracker> m_tracker;
        HashSet<int> m_intervalTimerIds;
        WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_timerCallChains;
        WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_animationFrameCallChains;
        WillBeHeapHashMap<RawPtrWillBeMember<Event>, RefPtrWillBeMember<AsyncCallChain> > m_eventCallChains;
        WillBeHeapHashMap<RawPtrWillBeMember<EventTarget>, RefPtrWillBeMember<AsyncCallChain> > m_xhrCallChains;
        WillBeHeapHashMap<RawPtrWillBeMember<MutationObserver>, RefPtrWillBeMember<AsyncCallChain> > m_mutationObserverCallChains;
        WillBeHeapHashMap<ExecutionContextTask*, RefPtrWillBeMember<AsyncCallChain> > m_executionContextTaskCallChains;
        WillBeHeapHashMap<String, RefPtrWillBeMember<AsyncCallChain> > m_v8AsyncTaskCallChains;
        WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_asyncOperationCallChains;
    };

    AsyncCallStackTracker();

    bool isEnabled() const { return m_maxAsyncCallStackDepth; }
    void setAsyncCallStackDepth(int);
    const AsyncCallChain* currentAsyncCallChain() const;

    void didInstallTimer(ExecutionContext*, int timerId, bool singleShot, const ScriptValue& callFrames);
    void didRemoveTimer(ExecutionContext*, int timerId);
    void willFireTimer(ExecutionContext*, int timerId);

    void didRequestAnimationFrame(ExecutionContext*, int callbackId, const ScriptValue& callFrames);
    void didCancelAnimationFrame(ExecutionContext*, int callbackId);
    void willFireAnimationFrame(ExecutionContext*, int callbackId);

    void didEnqueueEvent(EventTarget*, Event*, const ScriptValue& callFrames);
    void didRemoveEvent(EventTarget*, Event*);
    void willHandleEvent(EventTarget*, Event*, EventListener*, bool useCapture);

    void willLoadXHR(XMLHttpRequest*, const ScriptValue& callFrames);
    void didLoadXHR(XMLHttpRequest*);

    void didEnqueueMutationRecord(ExecutionContext*, MutationObserver*, const ScriptValue& callFrames);
    bool hasEnqueuedMutationRecord(ExecutionContext*, MutationObserver*);
    void didClearAllMutationRecords(ExecutionContext*, MutationObserver*);
    void willDeliverMutationRecords(ExecutionContext*, MutationObserver*);

    void didPostExecutionContextTask(ExecutionContext*, ExecutionContextTask*, const ScriptValue& callFrames);
    void didKillAllExecutionContextTasks(ExecutionContext*);
    void willPerformExecutionContextTask(ExecutionContext*, ExecutionContextTask*);

    void didEnqueueV8AsyncTask(ExecutionContext*, const String& eventName, int id, const ScriptValue& callFrames);
    void willHandleV8AsyncTask(ExecutionContext*, const String& eventName, int id);

    int traceAsyncOperationStarting(ExecutionContext*, const String& operationName, const ScriptValue& callFrames);
    void traceAsyncOperationCompleted(ExecutionContext*, int operationId);
    void traceAsyncCallbackStarting(ExecutionContext*, int operationId);

    void didFireAsyncCall();
    void clear();

    void trace(Visitor*);

private:
    void willHandleXHREvent(XMLHttpRequest*, Event*);

    PassRefPtrWillBeRawPtr<AsyncCallChain> createAsyncCallChain(const String& description, const ScriptValue& callFrames);
    void setCurrentAsyncCallChain(ExecutionContext*, PassRefPtrWillBeRawPtr<AsyncCallChain>);
    void clearCurrentAsyncCallChain();
    static void ensureMaxAsyncCallChainDepth(AsyncCallChain*, unsigned);
    bool validateCallFrames(const ScriptValue& callFrames);

    ExecutionContextData* createContextDataIfNeeded(ExecutionContext*);

    unsigned m_maxAsyncCallStackDepth;
    RefPtrWillBeMember<AsyncCallChain> m_currentAsyncCallChain;
    unsigned m_nestedAsyncCallCount;
    typedef WillBeHeapHashMap<RawPtrWillBeMember<ExecutionContext>, OwnPtrWillBeMember<ExecutionContextData> > ExecutionContextDataMap;
    ExecutionContextDataMap m_executionContextDataMap;
};

} // namespace blink

#endif // !defined(AsyncCallStackTracker_h)
