/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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
#include "core/loader/DocumentThreadableLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/CrossOriginPreflightResultCache.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/SharedBuffer.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Assertions.h"

namespace blink {

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    // The loader will be deleted as soon as this function exits.
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, &client, LoadSynchronously, request, options, resourceLoaderOptions));
    ASSERT(loader->hasOneRef());
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, request, options, resourceLoaderOptions));
    if (!loader->resource())
        loader = nullptr;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document& document, ThreadableLoaderClient* client, BlockingBehavior blockingBehavior, const ResourceRequest& request, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    : m_client(client)
    , m_document(document)
    , m_options(options)
    , m_resourceLoaderOptions(resourceLoaderOptions)
    , m_forceDoNotAllowStoredCredentials(false)
    , m_securityOrigin(m_resourceLoaderOptions.securityOrigin)
    , m_sameOriginRequest(securityOrigin()->canRequest(request.url()))
    , m_simpleRequest(true)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_timeoutTimer(this, &DocumentThreadableLoader::didTimeout)
    , m_requestStartedSeconds(0.0)
{
    ASSERT(client);
    // Setting an outgoing referer is only supported in the async code path.
    ASSERT(m_async || request.httpReferrer().isEmpty());

    if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
        m_client->didFail(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are not supported."));
        return;
    }

    m_requestStartedSeconds = monotonicallyIncreasingTime();

    // Save any CORS simple headers on the request here. If this request redirects cross-origin, we cancel the old request
    // create a new one, and copy these headers.
    const HTTPHeaderMap& headerMap = request.httpHeaderFields();
    HTTPHeaderMap::const_iterator end = headerMap.end();
    for (HTTPHeaderMap::const_iterator it = headerMap.begin(); it != end; ++it) {
        if (FetchUtils::isSimpleHeader(it->key, it->value))
            m_simpleRequestHeaders.add(it->key, it->value);
    }

    // If the fetch request will be handled by the ServiceWorker, the
    // FetchRequestMode of the request must be FetchRequestModeCORS or
    // FetchRequestModeCORSWithForcedPreflight. Otherwise the ServiceWorker can
    // return a opaque response which is from the other origin site and the
    // script in the page can read the content.
    //
    // We assume that ServiceWorker is skipped for sync requests by content/
    // code.
    if (m_async && !request.skipServiceWorker() && m_document.fetcher()->isControlledByServiceWorker()) {
        ResourceRequest newRequest(request);
        // FetchRequestMode should be set by the caller. But the expected value
        // of FetchRequestMode is not speced yet except for XHR. So we set here.
        // FIXME: When we support fetch API in document, this value should not
        // be overridden here.
        if (options.preflightPolicy == ForcePreflight)
            newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORSWithForcedPreflight);
        else
            newRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);

        m_fallbackRequestForServiceWorker = adoptPtr(new ResourceRequest(request));
        m_fallbackRequestForServiceWorker->setSkipServiceWorker(true);

        loadRequest(newRequest, m_resourceLoaderOptions);
        return;
    }

    dispatchInitialRequest(request);
}

void DocumentThreadableLoader::dispatchInitialRequest(const ResourceRequest& request)
{
    if (m_sameOriginRequest || m_options.crossOriginRequestPolicy == AllowCrossOriginRequests) {
        loadRequest(request, m_resourceLoaderOptions);
        return;
    }

    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    makeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    // Cross-origin requests are only allowed certain registered schemes.
    // We would catch this when checking response headers later, but there
    // is no reason to send a request, preflighted or not, that's guaranteed
    // to be denied.
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(request.url().protocol())) {
        m_client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are only supported for protocol schemes: " + SchemeRegistry::listOfCORSEnabledURLSchemes() + "."));
        return;
    }

    // We use isSimpleOrForbiddenRequest() here since |request| may have been
    // modified in the process of loading (not from the user's input). For
    // example, referrer. We need to accept them. For security, we must reject
    // forbidden headers/methods at the point we accept user's input. Not here.
    if ((m_options.preflightPolicy == ConsiderPreflight && FetchUtils::isSimpleOrForbiddenRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight) {
        ResourceRequest crossOriginRequest(request);
        ResourceLoaderOptions crossOriginOptions(m_resourceLoaderOptions);
        updateRequestForAccessControl(crossOriginRequest, securityOrigin(), effectiveAllowCredentials());
        loadRequest(crossOriginRequest, crossOriginOptions);
    } else {
        m_simpleRequest = false;

        OwnPtr<ResourceRequest> crossOriginRequest = adoptPtr(new ResourceRequest(request));
        OwnPtr<ResourceLoaderOptions> crossOriginOptions = adoptPtr(new ResourceLoaderOptions(m_resourceLoaderOptions));
        // Do not set the Origin header for preflight requests.
        updateRequestForAccessControl(*crossOriginRequest, 0, effectiveAllowCredentials());
        m_actualRequest = crossOriginRequest.release();
        m_actualOptions = crossOriginOptions.release();

        bool shouldForcePreflight = InspectorInstrumentation::shouldForceCORSPreflight(&m_document);
        bool canSkipPreflight = CrossOriginPreflightResultCache::shared().canSkipPreflight(securityOrigin()->toString(), m_actualRequest->url(), effectiveAllowCredentials(), m_actualRequest->httpMethod(), m_actualRequest->httpHeaderFields());
        if (canSkipPreflight && !shouldForcePreflight) {
            loadActualRequest();
        } else {
            ResourceRequest preflightRequest = createAccessControlPreflightRequest(*m_actualRequest, securityOrigin());
            // Create a ResourceLoaderOptions for preflight.
            ResourceLoaderOptions preflightOptions = *m_actualOptions;
            preflightOptions.allowCredentials = DoNotAllowStoredCredentials;
            loadRequest(preflightRequest, preflightOptions);
        }
    }
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
}

void DocumentThreadableLoader::overrideTimeout(unsigned long timeoutMilliseconds)
{
    ASSERT(m_async);
    ASSERT(m_requestStartedSeconds > 0.0);
    m_timeoutTimer.stop();
    // At the time of this method's implementation, it is only ever called by
    // XMLHttpRequest, when the timeout attribute is set after sending the
    // request.
    //
    // The XHR request says to resolve the time relative to when the request
    // was initially sent, however other uses of this method may need to
    // behave differently, in which case this should be re-arranged somehow.
    if (timeoutMilliseconds) {
        double elapsedTime = monotonicallyIncreasingTime() - m_requestStartedSeconds;
        double nextFire = timeoutMilliseconds / 1000.0;
        double resolvedTime = std::max(nextFire - elapsedTime, 0.0);
        m_timeoutTimer.startOneShot(resolvedTime, FROM_HERE);
    }
}

void DocumentThreadableLoader::cancel()
{
    cancelWithError(ResourceError());
}

void DocumentThreadableLoader::cancelWithError(const ResourceError& error)
{
    RefPtr<DocumentThreadableLoader> protect(this);

    // Cancel can re-enter and m_resource might be null here as a result.
    if (m_client && resource()) {
        ResourceError errorForCallback = error;
        if (errorForCallback.isNull()) {
            // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use FrameLoaderClient::cancelledError() instead.
            errorForCallback = ResourceError(errorDomainBlinkInternal, 0, resource()->url().string(), "Load cancelled");
            errorForCallback.setIsCancellation(true);
        }
        m_client->didFail(errorForCallback);
    }
    clearResource();
    m_client = 0;
    m_requestStartedSeconds = 0.0;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (resource())
        resource()->setDefersLoading(value);
}

void DocumentThreadableLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    RefPtr<DocumentThreadableLoader> protect(this);

    // FIXME: Support redirect in Fetch API.
    if (resource->resourceRequest().requestContext() == blink::WebURLRequest::RequestContextFetch) {
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url())) {
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
        m_requestStartedSeconds = 0.0;
        return;
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url())) {
        if (m_client->isDocumentThreadableLoaderClient())
            static_cast<DocumentThreadableLoaderClient*>(m_client)->willFollowRedirect(request, redirectResponse);
        return;
    }

    // When using access control, only simple cross origin requests are allowed to redirect. The new request URL must have a supported
    // scheme and not contain the userinfo production. In addition, the redirect response must pass the access control check if the
    // original request was not same-origin.
    if (m_options.crossOriginRequestPolicy == UseAccessControl) {

        InspectorInstrumentation::didReceiveCORSRedirectResponse(m_document.frame(), resource->identifier(), m_document.frame()->loader().documentLoader(), redirectResponse, 0);

        bool allowRedirect = false;
        String accessControlErrorDescription;

        if (m_simpleRequest) {
            allowRedirect = CrossOriginAccessControl::isLegalRedirectLocation(request.url(), accessControlErrorDescription)
                && (m_sameOriginRequest || passesAccessControlCheck(redirectResponse, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription));
        } else {
            accessControlErrorDescription = "The request was redirected to '"+ request.url().string() + "', which is disallowed for cross-origin requests that require preflight.";
        }

        if (allowRedirect) {
            // FIXME: consider combining this with CORS redirect handling performed by
            // CrossOriginAccessControl::handleRedirect().
            clearResource();

            RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(redirectResponse.url());
            RefPtr<SecurityOrigin> requestOrigin = SecurityOrigin::create(request.url());
            // If the original request wasn't same-origin, then if the request URL origin is not same origin with the original URL origin,
            // set the source origin to a globally unique identifier. (If the original request was same-origin, the origin of the new request
            // should be the original URL origin.)
            if (!m_sameOriginRequest && !originalOrigin->isSameSchemeHostPort(requestOrigin.get()))
                m_securityOrigin = SecurityOrigin::createUnique();
            // Force any subsequent requests to use these checks.
            m_sameOriginRequest = false;

            // Since the request is no longer same-origin, if the user didn't request credentials in
            // the first place, update our state so we neither request them nor expect they must be allowed.
            if (m_resourceLoaderOptions.credentialsRequested == ClientDidNotRequestCredentials)
                m_forceDoNotAllowStoredCredentials = true;

            // Remove any headers that may have been added by the network layer that cause access control to fail.
            request.clearHTTPReferrer();
            request.clearHTTPOrigin();
            request.clearHTTPUserAgent();
            // Add any CORS simple request headers which we previously saved from the original request.
            HTTPHeaderMap::const_iterator end = m_simpleRequestHeaders.end();
            for (HTTPHeaderMap::const_iterator it = m_simpleRequestHeaders.begin(); it != end; ++it) {
                request.setHTTPHeaderField(it->key, it->value);
            }
            makeCrossOriginAccessRequest(request);
            return;
        }

        ResourceError error(errorDomainBlinkInternal, 0, redirectResponse.url().string(), accessControlErrorDescription);
        m_client->didFailAccessControlCheck(error);
    } else {
        m_client->didFailRedirectCheck();
    }
    request = ResourceRequest();
    m_requestStartedSeconds = 0.0;
}

void DocumentThreadableLoader::dataSent(Resource* resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::dataDownloaded(Resource* resource, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(!m_actualRequest);
    ASSERT(m_async);

    m_client->didDownloadData(dataLength);
}

void DocumentThreadableLoader::responseReceived(Resource* resource, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    handleResponse(resource->identifier(), response, handle);
}

void DocumentThreadableLoader::handlePreflightResponse(const ResourceResponse& response)
{
    String accessControlErrorDescription;

    if (!passesAccessControlCheck(response, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription)) {
        handlePreflightFailure(response.url().string(), accessControlErrorDescription);
        return;
    }

    if (!passesPreflightStatusCheck(response, accessControlErrorDescription)) {
        handlePreflightFailure(response.url().string(), accessControlErrorDescription);
        return;
    }

    OwnPtr<CrossOriginPreflightResultCacheItem> preflightResult = adoptPtr(new CrossOriginPreflightResultCacheItem(effectiveAllowCredentials()));
    if (!preflightResult->parse(response, accessControlErrorDescription)
        || !preflightResult->allowsCrossOriginMethod(m_actualRequest->httpMethod(), accessControlErrorDescription)
        || !preflightResult->allowsCrossOriginHeaders(m_actualRequest->httpHeaderFields(), accessControlErrorDescription)) {
        handlePreflightFailure(response.url().string(), accessControlErrorDescription);
        return;
    }

    CrossOriginPreflightResultCache::shared().appendEntry(securityOrigin()->toString(), m_actualRequest->url(), preflightResult.release());
}

void DocumentThreadableLoader::reportResponseReceived(unsigned long identifier, const ResourceResponse& response)
{
    DocumentLoader* loader = m_document.frame()->loader().documentLoader();
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ResourceReceiveResponse", "data", InspectorReceiveResponseEvent::data(identifier, m_document.frame(), response));
    LocalFrame* frame = m_document.frame();
    InspectorInstrumentation::didReceiveResourceResponse(frame, identifier, loader, response, resource() ? resource()->loader() : 0);
    // It is essential that inspector gets resource response BEFORE console.
    frame->console().reportResourceResponseReceived(loader, identifier, response);
}

void DocumentThreadableLoader::handleResponse(unsigned long identifier, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    ASSERT(m_client);

    if (m_actualRequest) {
        reportResponseReceived(identifier, response);
        handlePreflightResponse(response);
        return;
    }

    if (response.wasFetchedViaServiceWorker()) {
        ASSERT(m_fallbackRequestForServiceWorker);
        if (response.wasFallbackRequiredByServiceWorker()) {
            loadFallbackRequestForServiceWorker();
            return;
        }
        m_fallbackRequestForServiceWorker = nullptr;
        m_client->didReceiveResponse(identifier, response, handle);
        return;
    }

    ASSERT(!m_fallbackRequestForServiceWorker);

    if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == UseAccessControl) {
        String accessControlErrorDescription;
        if (!passesAccessControlCheck(response, effectiveAllowCredentials(), securityOrigin(), accessControlErrorDescription)) {
            reportResponseReceived(identifier, response);
            m_client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, response.url().string(), accessControlErrorDescription));
            return;
        }
    }

    m_client->didReceiveResponse(identifier, response, handle);
}

void DocumentThreadableLoader::dataReceived(Resource* resource, const char* data, unsigned dataLength)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(m_async);

    handleReceivedData(data, dataLength);
}

void DocumentThreadableLoader::handleReceivedData(const char* data, unsigned dataLength)
{
    ASSERT(m_client);

    // Preflight data should be invisible to clients.
    if (m_actualRequest)
        return;

    ASSERT(!m_fallbackRequestForServiceWorker);

    m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_client);
    ASSERT(resource == this->resource());
    ASSERT(m_async);

    m_timeoutTimer.stop();

    if (resource->errorOccurred())
        m_client->didFail(resource->resourceError());
    else
        handleSuccessfulFinish(resource->identifier(), resource->loadFinishTime());
}

void DocumentThreadableLoader::handleSuccessfulFinish(unsigned long identifier, double finishTime)
{
    ASSERT(!m_fallbackRequestForServiceWorker);

    if (m_actualRequest) {
        ASSERT(!m_sameOriginRequest);
        ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
        loadActualRequest();
    } else {
        // FIXME: Should prevent timeout from being overridden after finished loading, without
        // resetting m_requestStartedSeconds to 0.0
        m_client->didFinishLoading(identifier, finishTime);
    }
}

void DocumentThreadableLoader::didTimeout(Timer<DocumentThreadableLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timeoutTimer);

    // Using values from net/base/net_error_list.h ERR_TIMED_OUT,
    // Same as existing FIXME above - this error should be coming from FrameLoaderClient to be identifiable.
    static const int timeoutError = -7;
    ResourceError error("net", timeoutError, resource()->url(), String());
    error.setIsTimeout(true);
    cancelWithError(error);
}

void DocumentThreadableLoader::loadFallbackRequestForServiceWorker()
{
    clearResource();
    OwnPtr<ResourceRequest> fallbackRequest(m_fallbackRequestForServiceWorker.release());
    dispatchInitialRequest(*fallbackRequest);
}

void DocumentThreadableLoader::loadActualRequest()
{
    OwnPtr<ResourceRequest> actualRequest;
    actualRequest.swap(m_actualRequest);
    OwnPtr<ResourceLoaderOptions> actualOptions;
    actualOptions.swap(m_actualOptions);

    actualRequest->setHTTPOrigin(securityOrigin()->toAtomicString());

    clearResource();

    loadRequest(*actualRequest, *actualOptions);
}

void DocumentThreadableLoader::handlePreflightFailure(const String& url, const String& errorDescription)
{
    ResourceError error(errorDomainBlinkInternal, 0, url, errorDescription);

    // Prevent handleSuccessfulFinish() from bypassing access check.
    m_actualRequest = nullptr;

    // FIXME: Should prevent timeout from being overridden after preflight failure, without
    // resetting m_requestStartedSeconds to 0.0
    m_client->didFailAccessControlCheck(error);
}

void DocumentThreadableLoader::loadRequest(const ResourceRequest& request, ResourceLoaderOptions resourceLoaderOptions)
{
    // Any credential should have been removed from the cross-site requests.
    const KURL& requestURL = request.url();
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    // Update resourceLoaderOptions with enforced values.
    if (m_forceDoNotAllowStoredCredentials)
        resourceLoaderOptions.allowCredentials = DoNotAllowStoredCredentials;
    resourceLoaderOptions.securityOrigin = m_securityOrigin;
    if (m_async) {
        if (m_actualRequest)
            resourceLoaderOptions.dataBufferingPolicy = BufferData;

        if (m_options.timeoutMilliseconds > 0)
            m_timeoutTimer.startOneShot(m_options.timeoutMilliseconds / 1000.0, FROM_HERE);

        FetchRequest newRequest(request, m_options.initiator, resourceLoaderOptions);
        if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
            newRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
        ASSERT(!resource());
        if (request.requestContext() == blink::WebURLRequest::RequestContextVideo || request.requestContext() == blink::WebURLRequest::RequestContextAudio)
            setResource(m_document.fetcher()->fetchMedia(newRequest));
        else
            setResource(m_document.fetcher()->fetchRawResource(newRequest));
        if (resource() && resource()->loader()) {
            unsigned long identifier = resource()->identifier();
            InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(&m_document, identifier, m_client);
        }
        return;
    }

    FetchRequest fetchRequest(request, m_options.initiator, resourceLoaderOptions);
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        fetchRequest.setOriginRestriction(FetchRequest::NoOriginRestriction);
    ResourcePtr<Resource> resource = m_document.fetcher()->fetchSynchronously(fetchRequest);
    ResourceResponse response = resource ? resource->response() : ResourceResponse();
    unsigned long identifier = resource ? resource->identifier() : std::numeric_limits<unsigned long>::max();
    ResourceError error = resource ? resource->resourceError() : ResourceError();

    InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(&m_document, identifier, m_client);

    if (!resource) {
        m_client->didFail(error);
        return;
    }

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !requestURL.isLocalFile() && response.httpStatusCode() <= 0) {
        m_client->didFail(error);
        return;
    }

    // FIXME: A synchronous request does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    if (requestURL != response.url() && (!isAllowedByContentSecurityPolicy(response.url()) || !isAllowedRedirect(response.url()))) {
        m_client->didFailRedirectCheck();
        return;
    }

    handleResponse(identifier, response, nullptr);

    SharedBuffer* data = resource->resourceBuffer();
    if (data)
        handleReceivedData(data->data(), data->size());

    handleSuccessfulFinish(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url) const
{
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        return true;

    return m_sameOriginRequest && securityOrigin()->canRequest(url);
}

bool DocumentThreadableLoader::isAllowedByContentSecurityPolicy(const KURL& url) const
{
    if (m_options.contentSecurityPolicyEnforcement != EnforceConnectSrcDirective)
        return true;
    return m_document.contentSecurityPolicy()->allowConnectToSource(url);
}

StoredCredentials DocumentThreadableLoader::effectiveAllowCredentials() const
{
    if (m_forceDoNotAllowStoredCredentials)
        return DoNotAllowStoredCredentials;
    return m_resourceLoaderOptions.allowCredentials;
}

SecurityOrigin* DocumentThreadableLoader::securityOrigin() const
{
    return m_securityOrigin ? m_securityOrigin.get() : m_document.securityOrigin();
}

} // namespace blink
