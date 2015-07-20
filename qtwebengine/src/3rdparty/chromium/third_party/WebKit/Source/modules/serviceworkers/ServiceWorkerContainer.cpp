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
#include "modules/serviceworkers/ServiceWorkerContainer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/MessageEvent.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebServiceWorker.h"
#include "public/platform/WebServiceWorkerProvider.h"
#include "public/platform/WebServiceWorkerRegistration.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

// This wraps CallbackPromiseAdapter and resolves the promise with undefined
// when nullptr is given to onSuccess.
class GetRegistrationCallback : public WebServiceWorkerProvider::WebServiceWorkerGetRegistrationCallbacks {
public:
    explicit GetRegistrationCallback(PassRefPtr<ScriptPromiseResolver> resolver)
        : m_resolver(resolver)
        , m_adapter(m_resolver) { }
    virtual ~GetRegistrationCallback() { }
    virtual void onSuccess(WebServiceWorkerRegistration* registration) override
    {
        if (registration)
            m_adapter.onSuccess(registration);
        else if (m_resolver->executionContext() && !m_resolver->executionContext()->activeDOMObjectsAreStopped())
            m_resolver->resolve();
    }
    virtual void onError(WebServiceWorkerError* error) override { m_adapter.onError(error); }
private:
    RefPtr<ScriptPromiseResolver> m_resolver;
    CallbackPromiseAdapter<ServiceWorkerRegistration, ServiceWorkerError> m_adapter;
    WTF_MAKE_NONCOPYABLE(GetRegistrationCallback);
};

ServiceWorkerContainer* ServiceWorkerContainer::create(ExecutionContext* executionContext)
{
    return new ServiceWorkerContainer(executionContext);
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
    ASSERT(!m_provider);
}

void ServiceWorkerContainer::willBeDetachedFromFrame()
{
    if (m_provider) {
        m_provider->setClient(0);
        m_provider = nullptr;
    }
}

void ServiceWorkerContainer::trace(Visitor* visitor)
{
    visitor->trace(m_controller);
    visitor->trace(m_readyRegistration);
    visitor->trace(m_ready);
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(ScriptState* scriptState, const String& url, const RegistrationOptions& options)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_provider) {
        resolver->reject(DOMException::create(InvalidStateError, "No associated provider is available"));
        return promise;
    }

    // FIXME: This should use the container's execution context, not
    // the callers.
    ExecutionContext* executionContext = scriptState->executionContext();
    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    if (!documentOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL pageURL = KURL(KURL(), documentOrigin->toString());
    if (!pageURL.protocolIsInHTTPFamily()) {
        resolver->reject(DOMException::create(SecurityError, "The URL protocol of the current origin is not supported: " + pageURL.protocol()));
        return promise;
    }

    KURL patternURL = executionContext->completeURL(options.scope());
    patternURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(patternURL)) {
        resolver->reject(DOMException::create(SecurityError, "The scope must match the current origin."));
        return promise;
    }

    KURL scriptURL = executionContext->completeURL(url);
    scriptURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(scriptURL)) {
        resolver->reject(DOMException::create(SecurityError, "The origin of the script must match the current origin."));
        return promise;
    }

    m_provider->registerServiceWorker(patternURL, scriptURL, new CallbackPromiseAdapter<ServiceWorkerRegistration, ServiceWorkerError>(resolver));

    return promise;
}

class BooleanValue {
public:
    typedef bool WebType;
    static bool take(ScriptPromiseResolver* resolver, WebType* boolean)
    {
        return *boolean;
    }
    static void dispose(WebType* boolean) { }

private:
    BooleanValue();
};

ScriptPromise ServiceWorkerContainer::getRegistration(ScriptState* scriptState, const String& documentURL)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // FIXME: This should use the container's execution context, not
    // the callers.
    ExecutionContext* executionContext = scriptState->executionContext();
    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    String errorMessage;
    if (!documentOrigin->canAccessFeatureRequiringSecureOrigin(errorMessage)) {
        resolver->reject(DOMException::create(NotSupportedError, errorMessage));
        return promise;
    }

    KURL pageURL = KURL(KURL(), documentOrigin->toString());
    if (!pageURL.protocolIsInHTTPFamily()) {
        resolver->reject(DOMException::create(SecurityError, "The URL protocol of the current origin is not supported: " + pageURL.protocol()));
        return promise;
    }

    KURL completedURL = executionContext->completeURL(documentURL);
    completedURL.removeFragmentIdentifier();
    if (!documentOrigin->canRequest(completedURL)) {
        resolver->reject(DOMException::create(SecurityError, "The documentURL must match the current origin."));
        return promise;
    }
    m_provider->getRegistration(completedURL, new GetRegistrationCallback(resolver));

    return promise;
}

ServiceWorkerContainer::ReadyProperty* ServiceWorkerContainer::createReadyProperty()
{
    return new ReadyProperty(executionContext(), this, ReadyProperty::Ready);
}

ScriptPromise ServiceWorkerContainer::ready(ScriptState* callerState)
{
    if (!executionContext())
        return ScriptPromise();

    if (!callerState->world().isMainWorld()) {
        // FIXME: Support .ready from isolated worlds when
        // ScriptPromiseProperty can vend Promises in isolated worlds.
        return ScriptPromise::rejectWithDOMException(callerState, DOMException::create(NotSupportedError, "'ready' is only supported in pages."));
    }

    return m_ready->promise(callerState->world());
}

// If the WebServiceWorker is up for adoption (does not have a
// WebServiceWorkerProxy owner), rejects the adoption by deleting the
// WebServiceWorker.
static void deleteIfNoExistingOwner(WebServiceWorker* serviceWorker)
{
    if (serviceWorker && !serviceWorker->proxy())
        delete serviceWorker;
}

void ServiceWorkerContainer::setController(WebServiceWorker* serviceWorker)
{
    if (!executionContext()) {
        deleteIfNoExistingOwner(serviceWorker);
        return;
    }
    m_controller = ServiceWorker::from(executionContext(), serviceWorker);
}

void ServiceWorkerContainer::setReadyRegistration(WebServiceWorkerRegistration* registration)
{
    if (!executionContext()) {
        ServiceWorkerRegistration::dispose(registration);
        return;
    }

    ServiceWorkerRegistration* readyRegistration = ServiceWorkerRegistration::from(executionContext(), registration);
    ASSERT(readyRegistration->active());

    if (m_readyRegistration) {
        ASSERT(m_readyRegistration == readyRegistration);
        ASSERT(m_ready->state() == ReadyProperty::Resolved);
        return;
    }

    m_readyRegistration = readyRegistration;
    m_ready->resolve(readyRegistration);
}

void ServiceWorkerContainer::dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    if (!executionContext() || !executionContext()->executingWindow())
        return;

    OwnPtrWillBeRawPtr<MessagePortArray> ports = MessagePort::toMessagePortArray(executionContext(), webChannels);
    RefPtr<SerializedScriptValue> value = SerializedScriptValue::createFromWire(message);
    executionContext()->executingWindow()->dispatchEvent(MessageEvent::create(ports.release(), value));
}

ServiceWorkerContainer::ServiceWorkerContainer(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
    , m_provider(0)
{

    if (!executionContext)
        return;

    m_ready = createReadyProperty();

    if (ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(executionContext)) {
        m_provider = client->provider();
        if (m_provider)
            m_provider->setClient(this);
    }
}

} // namespace blink
