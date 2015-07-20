// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchRequestData.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/serviceworkers/FetchHeaderList.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

FetchRequestData* FetchRequestData::create()
{
    return new FetchRequestData();
}

FetchRequestData* FetchRequestData::create(ExecutionContext* context)
{
    FetchRequestData* request = FetchRequestData::create();
    if (context->isDocument())
        request->m_referrer.setClient(blink::Referrer(context->url().strippedForUseAsReferrer(), toDocument(context)->referrerPolicy()));
    else
        request->m_referrer.setClient(blink::Referrer(context->url().strippedForUseAsReferrer(), ReferrerPolicyDefault));
    return request;
}

FetchRequestData* FetchRequestData::create(const WebServiceWorkerRequest& webRequest)
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = webRequest.url();
    request->m_method = webRequest.method();
    for (HTTPHeaderMap::const_iterator it = webRequest.headers().begin(); it != webRequest.headers().end(); ++it)
        request->m_headerList->append(it->key, it->value);
    request->m_blobDataHandle = webRequest.blobDataHandle();
    request->m_referrer.setURL(webRequest.referrer());
    request->setMode(webRequest.mode());
    request->setCredentials(webRequest.credentialsMode());
    return request;
}

FetchRequestData* FetchRequestData::createRestrictedCopy(ExecutionContext* context, PassRefPtr<SecurityOrigin> origin) const
{
    // "To make a restricted copy of a request |request|, run these steps:
    // 1. Let |r| be a new request whose url is |request|'s url, method is
    // |request|'s method, header list is a copy of |request|'s header list,
    // body is a tee of |request|'s body, client is entry settings object's
    // global object, origin is entry settings object's origin, referrer is
    // |client|, context is |connect|, mode is |request|'s mode, and credentials
    //  mode is |request|'s credentials mode."
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = m_url;
    request->m_method = m_method;
    request->m_headerList = m_headerList->createCopy();
    request->m_blobDataHandle = m_blobDataHandle;
    request->m_origin = origin;
    if (context->isDocument())
        request->m_referrer.setClient(blink::Referrer(context->url().strippedForUseAsReferrer(), toDocument(context)->referrerPolicy()));
    else
        request->m_referrer.setClient(blink::Referrer(context->url().strippedForUseAsReferrer(), ReferrerPolicyDefault));
    request->m_context = ConnectContext;
    request->m_mode = m_mode;
    request->m_credentials = m_credentials;
    // "2. Return |r|."
    return request;
}

FetchRequestData* FetchRequestData::createCopy() const
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = m_url;
    request->m_method = m_method;
    request->m_headerList = m_headerList->createCopy();
    request->m_unsafeRequestFlag = m_unsafeRequestFlag;
    request->m_blobDataHandle = m_blobDataHandle;
    request->m_origin = m_origin;
    request->m_sameOriginDataURLFlag = m_sameOriginDataURLFlag;
    request->m_context = m_context;
    request->m_referrer = m_referrer;
    request->m_mode = m_mode;
    request->m_credentials = m_credentials;
    request->m_responseTainting = m_responseTainting;
    return request;
}

FetchRequestData::~FetchRequestData()
{
}

FetchRequestData::FetchRequestData()
    : m_method("GET")
    , m_headerList(FetchHeaderList::create())
    , m_unsafeRequestFlag(false)
    , m_context(NullContext)
    , m_sameOriginDataURLFlag(false)
    , m_mode(WebURLRequest::FetchRequestModeNoCORS)
    , m_credentials(WebURLRequest::FetchCredentialsModeOmit)
    , m_responseTainting(BasicTainting)
{
}

void FetchRequestData::trace(Visitor* visitor)
{
    visitor->trace(m_headerList);
}

} // namespace blink
