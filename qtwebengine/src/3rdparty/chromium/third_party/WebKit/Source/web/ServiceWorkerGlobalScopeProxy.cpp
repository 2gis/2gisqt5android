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

#include "config.h"
#include "web/ServiceWorkerGlobalScopeProxy.h"

#include "bindings/core/v8/WorkerScriptController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/geofencing/CircularGeofencingRegion.h"
#include "modules/geofencing/GeofencingEvent.h"
#include "modules/push_messaging/PushEvent.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/FetchEvent.h"
#include "modules/serviceworkers/InstallEvent.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/web/WebSerializedScriptValue.h"
#include "public/web/WebServiceWorkerContextClient.h"
#include "web/WebEmbeddedWorkerImpl.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

PassOwnPtr<ServiceWorkerGlobalScopeProxy> ServiceWorkerGlobalScopeProxy::create(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
{
    return adoptPtr(new ServiceWorkerGlobalScopeProxy(embeddedWorker, document, client));
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy()
{
}

void ServiceWorkerGlobalScopeProxy::dispatchInstallEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    WaitUntilObserver* observer = WaitUntilObserver::create(m_workerGlobalScope, WaitUntilObserver::Install, eventID);
    RefPtrWillBeRawPtr<Event> event(InstallEvent::create(EventTypeNames::install, EventInit(), observer));
    m_workerGlobalScope->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchActivateEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    WaitUntilObserver* observer = WaitUntilObserver::create(m_workerGlobalScope, WaitUntilObserver::Activate, eventID);
    RefPtrWillBeRawPtr<Event> event(ExtendableEvent::create(EventTypeNames::activate, EventInit(), observer));
    m_workerGlobalScope->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest)
{
    ASSERT(m_workerGlobalScope);
    RespondWithObserver* observer = RespondWithObserver::create(m_workerGlobalScope, eventID, webRequest.mode(), webRequest.frameType());
    if (!RuntimeEnabledFeatures::serviceWorkerOnFetchEnabled()) {
        observer->didDispatchEvent();
        return;
    }

    Request* request = Request::create(m_workerGlobalScope, webRequest);
    RefPtrWillBeRawPtr<FetchEvent> fetchEvent(FetchEvent::create(observer, request));
    fetchEvent->setIsReload(webRequest.isReload());
    m_workerGlobalScope->dispatchEvent(fetchEvent.release());
    observer->didDispatchEvent();
}

void ServiceWorkerGlobalScopeProxy::dispatchGeofencingEvent(int eventID, WebGeofencingEventType eventType, const WebString& regionID, const WebCircularGeofencingRegion& region)
{
    ASSERT(m_workerGlobalScope);
    const AtomicString& type = eventType == WebGeofencingEventTypeEnter ? EventTypeNames::geofenceenter : EventTypeNames::geofenceleave;
    m_workerGlobalScope->dispatchEvent(GeofencingEvent::create(type, regionID, CircularGeofencingRegion::create(regionID, region)));
}

void ServiceWorkerGlobalScopeProxy::dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    ASSERT(m_workerGlobalScope);

    OwnPtrWillBeRawPtr<MessagePortArray> ports = MessagePort::toMessagePortArray(m_workerGlobalScope, webChannels);
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    m_workerGlobalScope->dispatchEvent(MessageEvent::create(ports.release(), value));
}

void ServiceWorkerGlobalScopeProxy::dispatchPushEvent(int eventID, const WebString& data)
{
    ASSERT(m_workerGlobalScope);
    m_workerGlobalScope->dispatchEvent(PushEvent::create(EventTypeNames::push, data));
}

void ServiceWorkerGlobalScopeProxy::dispatchSyncEvent(int eventID)
{
    ASSERT(m_workerGlobalScope);
    if (RuntimeEnabledFeatures::backgroundSyncEnabled())
        m_workerGlobalScope->dispatchEvent(Event::create(EventTypeNames::sync));
    ServiceWorkerGlobalScopeClient::from(m_workerGlobalScope)->didHandleSyncEvent(eventID);
}

void ServiceWorkerGlobalScopeProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_client.reportException(errorMessage, lineNumber, columnNumber, sourceURL);
}

void ServiceWorkerGlobalScopeProxy::reportConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage> consoleMessage)
{
    m_client.reportConsoleMessage(consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), consoleMessage->lineNumber(), consoleMessage->url());
}

void ServiceWorkerGlobalScopeProxy::postMessageToPageInspector(const String& message)
{
    m_document.postInspectorTask(createCrossThreadTask(&WebEmbeddedWorkerImpl::postMessageToPageInspector, &m_embeddedWorker, message));
}

void ServiceWorkerGlobalScopeProxy::didEvaluateWorkerScript(bool success)
{
    m_client.didEvaluateWorkerScript(success);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeStarted(WorkerGlobalScope* workerGlobalScope)
{
    ASSERT(!m_workerGlobalScope);
    m_workerGlobalScope = static_cast<ServiceWorkerGlobalScope*>(workerGlobalScope);
    m_client.workerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeClosed()
{
    m_document.postTask(createCrossThreadTask(&WebEmbeddedWorkerImpl::terminateWorkerContext, &m_embeddedWorker));
}

void ServiceWorkerGlobalScopeProxy::willDestroyWorkerGlobalScope()
{
    m_workerGlobalScope = 0;
    m_client.willDestroyWorkerContext();
}

void ServiceWorkerGlobalScopeProxy::workerThreadTerminated()
{
    m_client.workerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
    : m_embeddedWorker(embeddedWorker)
    , m_document(document)
    , m_client(client)
    , m_workerGlobalScope(0)
{
}

} // namespace blink
