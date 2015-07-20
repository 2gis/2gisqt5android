// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/Body.h"
#include "modules/serviceworkers/FetchRequestData.h"
#include "modules/serviceworkers/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class RequestInit;
class WebServiceWorkerRequest;

class Request final : public Body {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~Request() { }
    static Request* create(ExecutionContext*, const String&, ExceptionState&);
    static Request* create(ExecutionContext*, const String&, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, ExceptionState&);
    static Request* create(ExecutionContext*, Request*, const Dictionary&, ExceptionState&);
    static Request* create(ExecutionContext*, FetchRequestData*);
    static Request* create(ExecutionContext*, const WebServiceWorkerRequest&);
    // The 'FetchRequestData' object is shared between requests, as it is
    // immutable to the user after Request creation. Headers are copied.
    static Request* create(const Request&);

    const FetchRequestData* request() { return m_request; }

    String method() const;
    String url() const;
    Headers* headers() const { return m_headers; }
    String referrer() const;
    String mode() const;
    String credentials() const;

    Request* clone() const;

    void populateWebServiceWorkerRequest(WebServiceWorkerRequest&) const;

    void setBodyBlobHandle(PassRefPtr<BlobDataHandle>);
    bool hasBody() const { return m_request->blobDataHandle(); }

    virtual void trace(Visitor*)  override;

private:
    explicit Request(const Request&);
    Request(ExecutionContext*, FetchRequestData*);
    Request(ExecutionContext*, const WebServiceWorkerRequest&);

    static Request* createRequestWithRequestData(ExecutionContext*, FetchRequestData*, const RequestInit&, WebURLRequest::FetchRequestMode, WebURLRequest::FetchCredentialsMode, ExceptionState&);
    void clearHeaderList();

    virtual PassRefPtr<BlobDataHandle> blobDataHandle() override;

    const Member<FetchRequestData> m_request;
    const Member<Headers> m_headers;
};

} // namespace blink

#endif // Request_h
