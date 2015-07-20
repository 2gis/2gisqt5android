// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchRequestData_h
#define FetchRequestData_h

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BlobDataHandle;
class ExecutionContext;
class FetchHeaderList;
class SecurityOrigin;
class WebServiceWorkerRequest;

class FetchRequestData final : public GarbageCollectedFinalized<FetchRequestData> {
    WTF_MAKE_NONCOPYABLE(FetchRequestData);
public:
    enum Context { ChildContext, ConnectContext, DownloadContext, FontContext, FormContext, ImageContext, ManifestContext, MediaContext, NavigateContext, ObjectContext, PingContext, PopupContext, PrefetchContext, ScriptContext, ServiceWorkerContext, SharedWorkerContext, StyleContext, WorkerContext, NullContext };
    enum Tainting { BasicTainting, CORSTainting, OpaqueTainting };

    class Referrer final {
    public:
        Referrer() : m_type(ClientReferrer) { }
        bool isNone() const { return m_type == NoneReferrer; }
        bool isClient() const { return m_type == ClientReferrer; }
        bool isURL() const { return m_type == URLReferrer; }
        void setNone()
        {
            m_referrer = blink::Referrer();
            m_type = NoneReferrer;
        }
        void setClient(const blink::Referrer& referrer)
        {
            m_referrer = referrer;
            m_type = ClientReferrer;
        }
        void setURL(const blink::Referrer& referrer)
        {
            m_referrer = referrer;
            m_type = URLReferrer;
        }
        blink::Referrer referrer() const { return m_referrer; }
    private:
        enum Type { NoneReferrer, ClientReferrer, URLReferrer };
        Type m_type;
        blink::Referrer m_referrer;
    };

    static FetchRequestData* create(ExecutionContext*);
    static FetchRequestData* create(const blink::WebServiceWorkerRequest&);
    FetchRequestData* createRestrictedCopy(ExecutionContext*, PassRefPtr<SecurityOrigin>) const;
    FetchRequestData* createCopy() const;
    ~FetchRequestData();

    void setMethod(AtomicString method) { m_method = method; }
    const AtomicString method() const { return m_method; }
    void setURL(const KURL& url) { m_url = url; }
    const KURL& url() const { return m_url; }
    bool unsafeRequestFlag() const { return m_unsafeRequestFlag; }
    PassRefPtr<SecurityOrigin> origin() { return m_origin; }
    bool sameOriginDataURLFlag() { return m_sameOriginDataURLFlag; }
    const Referrer& referrer() const { return m_referrer; }
    void setMode(WebURLRequest::FetchRequestMode mode) { m_mode = mode; }
    WebURLRequest::FetchRequestMode mode() const { return m_mode; }
    void setCredentials(WebURLRequest::FetchCredentialsMode credentials) { m_credentials = credentials; }
    WebURLRequest::FetchCredentialsMode credentials() const { return m_credentials; }
    void setResponseTainting(Tainting tainting) { m_responseTainting = tainting; }
    Tainting tainting() const { return m_responseTainting; }
    FetchHeaderList* headerList() { return m_headerList.get(); }
    PassRefPtr<BlobDataHandle> blobDataHandle() const { return m_blobDataHandle; }
    void setBlobDataHandle(PassRefPtr<BlobDataHandle> blobHandle) { m_blobDataHandle = blobHandle; }

    void trace(Visitor*);

private:
    FetchRequestData();

    static FetchRequestData* create();

    AtomicString m_method;
    KURL m_url;
    Member<FetchHeaderList> m_headerList;
    RefPtr<BlobDataHandle> m_blobDataHandle;
    bool m_unsafeRequestFlag;
    // FIXME: Support m_skipServiceWorkerFlag;
    Context m_context;
    RefPtr<SecurityOrigin> m_origin;
    // FIXME: Support m_forceOriginHeaderFlag;
    bool m_sameOriginDataURLFlag;
    Referrer m_referrer;
    // FIXME: Support m_authenticationFlag;
    // FIXME: Support m_synchronousFlag;
    WebURLRequest::FetchRequestMode m_mode;
    WebURLRequest::FetchCredentialsMode m_credentials;
    // FIXME: Support m_useURLCredentialsFlag;
    // FIXME: Support m_manualRedirectFlag;
    // FIXME: Support m_redirectCount;
    Tainting m_responseTainting;
};

} // namespace blink

#endif // FetchRequestData_h
