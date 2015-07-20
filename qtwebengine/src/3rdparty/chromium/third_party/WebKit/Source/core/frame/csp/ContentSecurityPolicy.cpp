/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/frame/csp/ContentSecurityPolicy.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptController.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/SecurityPolicyViolationEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/CSPDirectiveList.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/CSPSourceList.h"
#include "core/frame/csp/MediaListDirective.h"
#include "core/frame/csp/SourceListDirective.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/PingLoader.h"
#include "platform/Crypto.h"
#include "platform/JSONValues.h"
#include "platform/ParsingUtilities.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/FormData.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/StringHasher.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {

// CSP 1.0 Directives
const char ContentSecurityPolicy::ConnectSrc[] = "connect-src";
const char ContentSecurityPolicy::DefaultSrc[] = "default-src";
const char ContentSecurityPolicy::FontSrc[] = "font-src";
const char ContentSecurityPolicy::FrameSrc[] = "frame-src";
const char ContentSecurityPolicy::ImgSrc[] = "img-src";
const char ContentSecurityPolicy::MediaSrc[] = "media-src";
const char ContentSecurityPolicy::ObjectSrc[] = "object-src";
const char ContentSecurityPolicy::ReportURI[] = "report-uri";
const char ContentSecurityPolicy::Sandbox[] = "sandbox";
const char ContentSecurityPolicy::ScriptSrc[] = "script-src";
const char ContentSecurityPolicy::StyleSrc[] = "style-src";

// CSP 1.1 Directives
const char ContentSecurityPolicy::BaseURI[] = "base-uri";
const char ContentSecurityPolicy::ChildSrc[] = "child-src";
const char ContentSecurityPolicy::FormAction[] = "form-action";
const char ContentSecurityPolicy::FrameAncestors[] = "frame-ancestors";
const char ContentSecurityPolicy::PluginTypes[] = "plugin-types";
const char ContentSecurityPolicy::ReflectedXSS[] = "reflected-xss";
const char ContentSecurityPolicy::Referrer[] = "referrer";

// Manifest Directives
// https://w3c.github.io/manifest/#content-security-policy
const char ContentSecurityPolicy::ManifestSrc[] = "manifest-src";

bool ContentSecurityPolicy::isDirectiveName(const String& name)
{
    return (equalIgnoringCase(name, ConnectSrc)
        || equalIgnoringCase(name, DefaultSrc)
        || equalIgnoringCase(name, FontSrc)
        || equalIgnoringCase(name, FrameSrc)
        || equalIgnoringCase(name, ImgSrc)
        || equalIgnoringCase(name, MediaSrc)
        || equalIgnoringCase(name, ObjectSrc)
        || equalIgnoringCase(name, ReportURI)
        || equalIgnoringCase(name, Sandbox)
        || equalIgnoringCase(name, ScriptSrc)
        || equalIgnoringCase(name, StyleSrc)
        || equalIgnoringCase(name, BaseURI)
        || equalIgnoringCase(name, ChildSrc)
        || equalIgnoringCase(name, FormAction)
        || equalIgnoringCase(name, FrameAncestors)
        || equalIgnoringCase(name, PluginTypes)
        || equalIgnoringCase(name, ReflectedXSS)
        || equalIgnoringCase(name, Referrer)
        || equalIgnoringCase(name, ManifestSrc)
    );
}

static UseCounter::Feature getUseCounterType(ContentSecurityPolicyHeaderType type)
{
    switch (type) {
    case ContentSecurityPolicyHeaderTypeEnforce:
        return UseCounter::ContentSecurityPolicy;
    case ContentSecurityPolicyHeaderTypeReport:
        return UseCounter::ContentSecurityPolicyReportOnly;
    }
    ASSERT_NOT_REACHED();
    return UseCounter::NumberOfFeatures;
}

static ReferrerPolicy mergeReferrerPolicies(ReferrerPolicy a, ReferrerPolicy b)
{
    if (a != b)
        return ReferrerPolicyNever;
    return a;
}

ContentSecurityPolicy::ContentSecurityPolicy()
    : m_executionContext(nullptr)
    , m_overrideInlineStyleAllowed(false)
    , m_scriptHashAlgorithmsUsed(ContentSecurityPolicyHashAlgorithmNone)
    , m_styleHashAlgorithmsUsed(ContentSecurityPolicyHashAlgorithmNone)
    , m_sandboxMask(0)
    , m_referrerPolicy(ReferrerPolicyDefault)
{
}

void ContentSecurityPolicy::bindToExecutionContext(ExecutionContext* executionContext)
{
    m_executionContext = executionContext;
    applyPolicySideEffectsToExecutionContext();
}

void ContentSecurityPolicy::applyPolicySideEffectsToExecutionContext()
{
    ASSERT(m_executionContext);
    // Ensure that 'self' processes correctly.
    m_selfProtocol = securityOrigin()->protocol();
    m_selfSource = adoptPtr(new CSPSource(this, m_selfProtocol, securityOrigin()->host(), securityOrigin()->port(), String(), CSPSource::NoWildcard, CSPSource::NoWildcard));

    // If we're in a Document, set the referrer policy and sandbox flags, then dump all the
    // parsing error messages, then poke at histograms.
    if (Document* document = this->document()) {
        document->enforceSandboxFlags(m_sandboxMask);
        if (didSetReferrerPolicy())
            document->setReferrerPolicy(m_referrerPolicy);

        for (const auto& consoleMessage : m_consoleMessages)
            m_executionContext->addConsoleMessage(consoleMessage);
        m_consoleMessages.clear();

        for (const auto& policy : m_policies)
            UseCounter::count(*document, getUseCounterType(policy->headerType()));
    }

    // We disable 'eval()' even in the case of report-only policies, and rely on the check in the
    // V8Initializer::codeGenerationCheckCallbackInMainThread callback to determine whether the
    // call should execute or not.
    if (!m_disableEvalErrorMessage.isNull())
        m_executionContext->disableEval(m_disableEvalErrorMessage);
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

Document* ContentSecurityPolicy::document() const
{
    return m_executionContext->isDocument() ? toDocument(m_executionContext) : nullptr;
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other)
{
    ASSERT(m_policies.isEmpty());
    for (const auto& policy : other->m_policies)
        addPolicyFromHeaderValue(policy->header(), policy->headerType(), policy->headerSource());
}

void ContentSecurityPolicy::didReceiveHeaders(const ContentSecurityPolicyResponseHeaders& headers)
{
    if (!headers.contentSecurityPolicy().isEmpty())
        addPolicyFromHeaderValue(headers.contentSecurityPolicy(), ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    if (!headers.contentSecurityPolicyReportOnly().isEmpty())
        addPolicyFromHeaderValue(headers.contentSecurityPolicyReportOnly(), ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceHTTP);
}

void ContentSecurityPolicy::didReceiveHeader(const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicyHeaderSource source)
{
    addPolicyFromHeaderValue(header, type, source);

    // This might be called after we've been bound to an execution context. For example, a <meta>
    // element might be injected after page load.
    if (m_executionContext)
        applyPolicySideEffectsToExecutionContext();
}

void ContentSecurityPolicy::addPolicyFromHeaderValue(const String& header, ContentSecurityPolicyHeaderType type, ContentSecurityPolicyHeaderSource source)
{
    // If this is a report-only header inside a <meta> element, bail out.
    if (source == ContentSecurityPolicyHeaderSourceMeta && type == ContentSecurityPolicyHeaderTypeReport && experimentalFeaturesEnabled()) {
        reportReportOnlyInMeta(header);
        return;
    }

    Vector<UChar> characters;
    header.appendTo(characters);

    const UChar* begin = characters.data();
    const UChar* end = begin + characters.size();

    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    const UChar* position = begin;
    while (position < end) {
        skipUntil<UChar>(position, end, ',');

        // header1,header2 OR header1
        //        ^                  ^
        OwnPtr<CSPDirectiveList> policy = CSPDirectiveList::create(this, begin, position, type, source);

        if (type != ContentSecurityPolicyHeaderTypeReport && policy->didSetReferrerPolicy()) {
            // FIXME: We need a 'ReferrerPolicyUnset' enum to avoid confusing code like this.
            m_referrerPolicy = didSetReferrerPolicy() ? mergeReferrerPolicies(m_referrerPolicy, policy->referrerPolicy()) : policy->referrerPolicy();
        }

        if (!policy->allowEval(0, SuppressReport) && m_disableEvalErrorMessage.isNull())
            m_disableEvalErrorMessage = policy->evalDisabledErrorMessage();

        m_policies.append(policy.release());

        // Skip the comma, and begin the next header from the current position.
        ASSERT(position == end || *position == ',');
        skipExactly<UChar>(position, end, ',');
        begin = position;
    }
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value)
{
    m_overrideInlineStyleAllowed = value;
}

void ContentSecurityPolicy::setOverrideURLForSelf(const KURL& url)
{
    // Create a temporary CSPSource so that 'self' expressions can be resolved before we bind to
    // an execution context (for 'frame-ancestor' resolution, for example). This CSPSource will
    // be overwritten when we bind this object to an execution context.
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
    m_selfProtocol = origin->protocol();
    m_selfSource = adoptPtr(new CSPSource(this, m_selfProtocol, origin->host(), origin->port(), String(), CSPSource::NoWildcard, CSPSource::NoWildcard));
}

const String& ContentSecurityPolicy::deprecatedHeader() const
{
    return m_policies.isEmpty() ? emptyString() : m_policies[0]->header();
}

ContentSecurityPolicyHeaderType ContentSecurityPolicy::deprecatedHeaderType() const
{
    return m_policies.isEmpty() ? ContentSecurityPolicyHeaderTypeEnforce : m_policies[0]->headerType();
}

template<bool (CSPDirectiveList::*allowed)(ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(ScriptState* scriptState, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithState(const CSPDirectiveListVector& policies, ScriptState* scriptState, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(scriptState, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const String&, const WTF::OrdinalNumber&, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithContext(const CSPDirectiveListVector& policies, const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(contextURL, contextLine, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const String&) const>
bool isAllowedByAllWithNonce(const CSPDirectiveListVector& policies, const String& nonce)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(nonce))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const CSPHashValue&) const>
bool isAllowedByAllWithHash(const CSPDirectiveListVector& policies, const CSPHashValue& hashValue)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(hashValue))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowFromURL)(const KURL&, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithURL(const CSPDirectiveListVector& policies, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
        return true;

    for (const auto& policy : policies) {
        if (!(policy.get()->*allowFromURL)(url, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(LocalFrame*, const KURL&, ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAllWithFrame(const CSPDirectiveListVector& policies, LocalFrame* frame, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus)
{
    for (const auto& policy : policies) {
        if (!(policy.get()->*allowed)(frame, url, reportingStatus))
            return false;
    }
    return true;
}

template<bool (CSPDirectiveList::*allowed)(const CSPHashValue&) const>
bool checkDigest(const String& source, uint8_t hashAlgorithmsUsed, const CSPDirectiveListVector& policies)
{
    // Any additions or subtractions from this struct should also modify the
    // respective entries in the kSupportedPrefixes array in
    // CSPSourceList::parseHash().
    static const struct {
        ContentSecurityPolicyHashAlgorithm cspHashAlgorithm;
        HashAlgorithm algorithm;
    } kAlgorithmMap[] = {
        { ContentSecurityPolicyHashAlgorithmSha1, HashAlgorithmSha1 },
        { ContentSecurityPolicyHashAlgorithmSha256, HashAlgorithmSha256 },
        { ContentSecurityPolicyHashAlgorithmSha384, HashAlgorithmSha384 },
        { ContentSecurityPolicyHashAlgorithmSha512, HashAlgorithmSha512 }
    };

    // Only bother normalizing the source/computing digests if there are any checks to be done.
    if (hashAlgorithmsUsed == ContentSecurityPolicyHashAlgorithmNone)
        return false;

    StringUTF8Adaptor normalizedSource(source, StringUTF8Adaptor::Normalize, WTF::EntitiesForUnencodables);

    for (const auto& algorithmMap : kAlgorithmMap) {
        DigestValue digest;
        if (algorithmMap.cspHashAlgorithm & hashAlgorithmsUsed) {
            bool digestSuccess = computeDigest(algorithmMap.algorithm, normalizedSource.data(), normalizedSource.length(), digest);
            if (digestSuccess && isAllowedByAllWithHash<allowed>(policies, CSPHashValue(algorithmMap.cspHashAlgorithm, digest)))
                return true;
        }
    }

    return false;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowJavaScriptURLs>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineEventHandlers>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineScript>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (m_overrideInlineStyleAllowed)
        return true;
    return isAllowedByAllWithContext<&CSPDirectiveList::allowInlineStyle>(m_policies, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowEval(ScriptState* scriptState, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithState<&CSPDirectiveList::allowEval>(m_policies, scriptState, reportingStatus);
}

String ContentSecurityPolicy::evalDisabledErrorMessage() const
{
    for (const auto& policy : m_policies) {
        if (!policy->allowEval(0, SuppressReport))
            return policy->evalDisabledErrorMessage();
    }
    return String();
}

bool ContentSecurityPolicy::allowPluginType(const String& type, const String& typeAttribute, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    for (const auto& policy : m_policies) {
        if (!policy->allowPluginType(type, typeAttribute, url, reportingStatus))
            return false;
    }
    return true;
}

bool ContentSecurityPolicy::allowScriptFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowScriptFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowScriptWithNonce(const String& nonce) const
{
    return isAllowedByAllWithNonce<&CSPDirectiveList::allowScriptNonce>(m_policies, nonce);
}

bool ContentSecurityPolicy::allowStyleWithNonce(const String& nonce) const
{
    return isAllowedByAllWithNonce<&CSPDirectiveList::allowStyleNonce>(m_policies, nonce);
}

bool ContentSecurityPolicy::allowScriptWithHash(const String& source) const
{
    return checkDigest<&CSPDirectiveList::allowScriptHash>(source, m_scriptHashAlgorithmsUsed, m_policies);
}

bool ContentSecurityPolicy::allowStyleWithHash(const String& source) const
{
    return checkDigest<&CSPDirectiveList::allowStyleHash>(source, m_styleHashAlgorithmsUsed, m_policies);
}

void ContentSecurityPolicy::usesScriptHashAlgorithms(uint8_t algorithms)
{
    m_scriptHashAlgorithmsUsed |= algorithms;
}

void ContentSecurityPolicy::usesStyleHashAlgorithms(uint8_t algorithms)
{
    m_styleHashAlgorithmsUsed |= algorithms;
}

bool ContentSecurityPolicy::allowObjectFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowObjectFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowChildFrameFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowImageFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowImageFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowStyleFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowStyleFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowFontFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowFontFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowMediaFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowMediaFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowConnectToSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowConnectToSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowFormAction(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowFormAction>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowBaseURI(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowBaseURI>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowAncestors(LocalFrame* frame, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithFrame<&CSPDirectiveList::allowAncestors>(m_policies, frame, url, reportingStatus);
}

bool ContentSecurityPolicy::allowChildContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowWorkerContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    // CSP 1.1 moves workers from 'script-src' to the new 'child-src'. Measure the impact of this backwards-incompatible change.
    if (Document* document = this->document()) {
        UseCounter::count(*document, UseCounter::WorkerSubjectToCSP);
        if (isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, SuppressReport) && !isAllowedByAllWithURL<&CSPDirectiveList::allowScriptFromSource>(m_policies, url, SuppressReport))
            UseCounter::count(*document, UseCounter::WorkerAllowedByChildBlockedByScript);
    }

    return isAllowedByAllWithURL<&CSPDirectiveList::allowChildContextFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::allowManifestFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return isAllowedByAllWithURL<&CSPDirectiveList::allowManifestFromSource>(m_policies, url, reportingStatus);
}

bool ContentSecurityPolicy::isActive() const
{
    return !m_policies.isEmpty();
}

ReflectedXSSDisposition ContentSecurityPolicy::reflectedXSSDisposition() const
{
    ReflectedXSSDisposition disposition = ReflectedXSSUnset;
    for (const auto& policy : m_policies) {
        if (policy->reflectedXSSDisposition() > disposition)
            disposition = std::max(disposition, policy->reflectedXSSDisposition());
    }
    return disposition;
}

ReferrerPolicy ContentSecurityPolicy::referrerPolicy() const
{
    ReferrerPolicy referrerPolicy = ReferrerPolicyDefault;
    bool first = true;
    for (const auto& policy : m_policies) {
        if (policy->didSetReferrerPolicy()) {
            if (first)
                referrerPolicy = policy->referrerPolicy();
            else
                referrerPolicy = mergeReferrerPolicies(referrerPolicy, policy->referrerPolicy());
        }
    }
    return referrerPolicy;
}

bool ContentSecurityPolicy::didSetReferrerPolicy() const
{
    for (const auto& policy : m_policies) {
        if (policy->didSetReferrerPolicy())
            return true;
    }
    return false;
}

SecurityOrigin* ContentSecurityPolicy::securityOrigin() const
{
    return m_executionContext->securityContext().securityOrigin();
}

const KURL ContentSecurityPolicy::url() const
{
    return m_executionContext->contextURL();
}

KURL ContentSecurityPolicy::completeURL(const String& url) const
{
    return m_executionContext->contextCompleteURL(url);
}

void ContentSecurityPolicy::enforceSandboxFlags(SandboxFlags mask)
{
    m_sandboxMask |= mask;
}

static String stripURLForUseInReport(Document* document, const KURL& url)
{
    if (!url.isValid())
        return String();
    if (!url.isHierarchical() || url.protocolIs("file"))
        return url.protocol();
    return document->securityOrigin()->canRequest(url) ? url.strippedForUseAsReferrer() : SecurityOrigin::create(url)->toString();
}

static void gatherSecurityPolicyViolationEventData(SecurityPolicyViolationEventInit& init, Document* document, const String& directiveText, const String& effectiveDirective, const KURL& blockedURL, const String& header)
{
    if (equalIgnoringCase(effectiveDirective, ContentSecurityPolicy::FrameAncestors)) {
        // If this load was blocked via 'frame-ancestors', then the URL of |document| has not yet
        // been initialized. In this case, we'll set both 'documentURI' and 'blockedURI' to the
        // blocked document's URL.
        init.documentURI = blockedURL.string();
        init.blockedURI = blockedURL.string();
    } else {
        init.documentURI = document->url().string();
        init.blockedURI = stripURLForUseInReport(document, blockedURL);
    }
    init.referrer = document->referrer();
    init.violatedDirective = directiveText;
    init.effectiveDirective = effectiveDirective;
    init.originalPolicy = header;
    init.sourceFile = String();
    init.lineNumber = 0;
    init.columnNumber = 0;
    init.statusCode = 0;

    if (!SecurityOrigin::isSecure(document->url()) && document->loader())
        init.statusCode = document->loader()->response().httpStatusCode();

    RefPtrWillBeRawPtr<ScriptCallStack> stack = createScriptCallStack(1, false);
    if (!stack)
        return;

    const ScriptCallFrame& callFrame = stack->at(0);

    if (callFrame.lineNumber()) {
        KURL source = KURL(ParsedURLString, callFrame.sourceURL());
        init.sourceFile = stripURLForUseInReport(document, source);
        init.lineNumber = callFrame.lineNumber();
        init.columnNumber = callFrame.columnNumber();
    }
}

void ContentSecurityPolicy::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const Vector<String>& reportEndpoints, const String& header, LocalFrame* contextFrame)
{
    ASSERT((m_executionContext && !contextFrame) || (equalIgnoringCase(effectiveDirective, ContentSecurityPolicy::FrameAncestors) && contextFrame));

    // FIXME: Support sending reports from worker.
    Document* document = contextFrame ? contextFrame->document() : this->document();
    if (!document)
        return;

    LocalFrame* frame = document->frame();
    if (!frame)
        return;

    SecurityPolicyViolationEventInit violationData;
    gatherSecurityPolicyViolationEventData(violationData, document, directiveText, effectiveDirective, blockedURL, header);

    frame->domWindow()->enqueueDocumentEvent(SecurityPolicyViolationEvent::create(EventTypeNames::securitypolicyviolation, violationData));

    if (reportEndpoints.isEmpty())
        return;

    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we supress the Referer in that
    // case), but the Referer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    RefPtr<JSONObject> cspReport = JSONObject::create();
    cspReport->setString("document-uri", violationData.documentURI);
    cspReport->setString("referrer", violationData.referrer);
    cspReport->setString("violated-directive", violationData.violatedDirective);
    cspReport->setString("effective-directive", violationData.effectiveDirective);
    cspReport->setString("original-policy", violationData.originalPolicy);
    cspReport->setString("blocked-uri", violationData.blockedURI);
    if (!violationData.sourceFile.isEmpty() && violationData.lineNumber) {
        cspReport->setString("source-file", violationData.sourceFile);
        cspReport->setNumber("line-number", violationData.lineNumber);
        cspReport->setNumber("column-number", violationData.columnNumber);
    }
    cspReport->setNumber("status-code", violationData.statusCode);

    RefPtr<JSONObject> reportObject = JSONObject::create();
    reportObject->setObject("csp-report", cspReport.release());
    String stringifiedReport = reportObject->toJSONString();

    if (!shouldSendViolationReport(stringifiedReport))
        return;

    RefPtr<FormData> report = FormData::create(stringifiedReport.utf8());

    for (const String& endpoint : reportEndpoints) {
        // If we have a context frame we're dealing with 'frame-ancestors' and we don't have our
        // own execution context. Use the frame's document to complete the endpoint URL, overriding
        // its URL with the blocked document's URL.
        ASSERT(!contextFrame || !m_executionContext);
        ASSERT(!contextFrame || equalIgnoringCase(effectiveDirective, FrameAncestors));
        KURL url = contextFrame ? frame->document()->completeURLWithOverride(endpoint, blockedURL) : completeURL(endpoint);
        PingLoader::sendViolationReport(frame, url, report, PingLoader::ContentSecurityPolicyViolationReport);
    }

    didSendViolationReport(stringifiedReport);
}

void ContentSecurityPolicy::reportInvalidReferrer(const String& invalidValue)
{
    logToConsole("The 'referrer' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Valid values are \"no-referrer\", \"no-referrer-when-downgrade\", \"origin\", and \"unsafe-url\". Note that \"origin-when-cross-origin\" is not yet supported.");
}

void ContentSecurityPolicy::reportReportOnlyInMeta(const String& header)
{
    logToConsole("The report-only Content Security Policy '" + header + "' was delivered via a <meta> element, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::reportMetaOutsideHead(const String& header)
{
    logToConsole("The Content Security Policy '" + header + "' was delivered via a <meta> element outside the document's <head>, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::reportInvalidInReportOnly(const String& name)
{
    logToConsole("The Content Security Policy directive '" + name + "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name)
{
    DEFINE_STATIC_LOCAL(String, allow, ("allow"));
    DEFINE_STATIC_LOCAL(String, options, ("options"));
    DEFINE_STATIC_LOCAL(String, policyURI, ("policy-uri"));
    DEFINE_STATIC_LOCAL(String, allowMessage, ("The 'allow' directive has been replaced with 'default-src'. Please use that directive instead, as 'allow' has no effect."));
    DEFINE_STATIC_LOCAL(String, optionsMessage, ("The 'options' directive has been replaced with 'unsafe-inline' and 'unsafe-eval' source expressions for the 'script-src' and 'style-src' directives. Please use those directives instead, as 'options' has no effect."));
    DEFINE_STATIC_LOCAL(String, policyURIMessage, ("The 'policy-uri' directive has been removed from the specification. Please specify a complete policy via the Content-Security-Policy header."));

    String message = "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
    MessageLevel level = ErrorMessageLevel;
    if (equalIgnoringCase(name, allow)) {
        message = allowMessage;
    } else if (equalIgnoringCase(name, options)) {
        message = optionsMessage;
    } else if (equalIgnoringCase(name, policyURI)) {
        message = policyURIMessage;
    } else if (isDirectiveName(name)) {
        message = "The Content-Security-Policy directive '" + name + "' is implemented behind a flag which is currently disabled.\n";
        level = InfoMessageLevel;
    }

    logToConsole(message, level);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression)
{
    String message = "The Content Security Policy directive '" + directiveName + "' contains '" + sourceExpression + "' as a source expression. Did you mean '" + directiveName + " ...; " + sourceExpression + "...' (note the semicolon)?";
    logToConsole(message);
}

void ContentSecurityPolicy::reportDuplicateDirective(const String& name)
{
    String message = "Ignoring duplicate Content-Security-Policy directive '" + name + "'.\n";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPluginTypes(const String& pluginType)
{
    String message;
    if (pluginType.isNull())
        message = "'plugin-types' Content Security Policy directive is empty; all plugins will be blocked.\n";
    else
        message = "Invalid plugin type in 'plugin-types' Content Security Policy directive: '" + pluginType + "'.\n";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSandboxFlags(const String& invalidFlags)
{
    logToConsole("Error while parsing the 'sandbox' Content Security Policy directive: " + invalidFlags);
}

void ContentSecurityPolicy::reportInvalidReflectedXSS(const String& invalidValue)
{
    logToConsole("The 'reflected-xss' Content Security Policy directive has the invalid value \"" + invalidValue + "\". Valid values are \"allow\", \"filter\", and \"block\".");
}

void ContentSecurityPolicy::reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value)
{
    String message = "The value for Content Security Policy directive '" + directiveName + "' contains an invalid character: '" + value + "'. Non-whitespace characters outside ASCII 0x21-0x7E must be percent-encoded, as described in RFC 3986, section 2.1: http://tools.ietf.org/html/rfc3986#section-2.1.";
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPathCharacter(const String& directiveName, const String& value, const char invalidChar)
{
    ASSERT(invalidChar == '#' || invalidChar == '?');

    String ignoring = "The fragment identifier, including the '#', will be ignored.";
    if (invalidChar == '?')
        ignoring = "The query component, including the '?', will be ignored.";
    String message = "The source list for Content Security Policy directive '" + directiveName + "' contains a source with an invalid path: '" + value + "'. " + ignoring;
    logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSourceExpression(const String& directiveName, const String& source)
{
    String message = "The source list for Content Security Policy directive '" + directiveName + "' contains an invalid source: '" + source + "'. It will be ignored.";
    if (equalIgnoringCase(source, "'none'"))
        message = message + " Note that 'none' has no effect unless it is the only expression in the source list.";
    logToConsole(message);
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy)
{
    logToConsole("The Content Security Policy '" + policy + "' was delivered in report-only mode, but does not specify a 'report-uri'; the policy will have no effect. Please either add a 'report-uri' directive, or deliver the policy via the 'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message, MessageLevel level)
{
    logToConsole(ConsoleMessage::create(SecurityMessageSource, level, message));
}

void ContentSecurityPolicy::logToConsole(PassRefPtrWillBeRawPtr<ConsoleMessage> consoleMessage, LocalFrame* frame)
{
    if (frame)
        frame->document()->addConsoleMessage(consoleMessage);
    else if (m_executionContext)
        m_executionContext->addConsoleMessage(consoleMessage);
    else
        m_consoleMessages.append(consoleMessage);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(const String& directiveText) const
{
    m_executionContext->reportBlockedScriptExecutionToInspector(directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const
{
    return RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled();
}

bool ContentSecurityPolicy::urlMatchesSelf(const KURL& url) const
{
    return m_selfSource->matches(url);
}

bool ContentSecurityPolicy::protocolMatchesSelf(const KURL& url) const
{
    if (equalIgnoringCase("http", m_selfProtocol))
        return url.protocolIsInHTTPFamily();
    return equalIgnoringCase(url.protocol(), m_selfProtocol);
}

bool ContentSecurityPolicy::shouldBypassMainWorld(ExecutionContext* context)
{
    if (context && context->isDocument()) {
        Document* document = toDocument(context);
        if (document->frame())
            return document->frame()->script().shouldBypassMainWorldCSP();
    }
    return false;
}

bool ContentSecurityPolicy::shouldSendViolationReport(const String& report) const
{
    // Collisions have no security impact, so we can save space by storing only the string's hash rather than the whole report.
    return !m_violationReportsSent.contains(report.impl()->hash());
}

void ContentSecurityPolicy::didSendViolationReport(const String& report)
{
    m_violationReportsSent.add(report.impl()->hash());
}

} // namespace blink
