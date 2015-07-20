// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/WebRemoteFrameImpl.h"

#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebRect.h"
#include "public/web/WebDocument.h"
#include "public/web/WebPerformance.h"
#include "public/web/WebRange.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include <v8/include/v8.h>

namespace blink {

namespace {

// Helper class to bridge communication for a local frame with a remote parent.
// Currently, it serves two purposes:
// 1. Allows the local frame's loader to retrieve sandbox flags associated with
//    its owner element in another process.
// 2. Trigger a load event on its owner element once it finishes a load.
class RemoteBridgeFrameOwner : public NoBaseWillBeGarbageCollectedFinalized<RemoteBridgeFrameOwner>, public FrameOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RemoteBridgeFrameOwner);
public:
    static PassOwnPtrWillBeRawPtr<RemoteBridgeFrameOwner> create(PassRefPtrWillBeRawPtr<WebLocalFrameImpl> frame)
    {
        return adoptPtrWillBeNoop(new RemoteBridgeFrameOwner(frame));
    }

    virtual bool isLocal() const override;
    virtual SandboxFlags sandboxFlags() const override;
    virtual void dispatchLoad() override;

    virtual void trace(Visitor*);

private:
    explicit RemoteBridgeFrameOwner(PassRefPtrWillBeRawPtr<WebLocalFrameImpl>);

    RefPtrWillBeMember<WebLocalFrameImpl> m_frame;
};

RemoteBridgeFrameOwner::RemoteBridgeFrameOwner(PassRefPtrWillBeRawPtr<WebLocalFrameImpl> frame)
    : m_frame(frame)
{
}

void RemoteBridgeFrameOwner::trace(Visitor* visitor)
{
    visitor->trace(m_frame);
    FrameOwner::trace(visitor);
}

bool RemoteBridgeFrameOwner::isLocal() const
{
    return false;
}

SandboxFlags RemoteBridgeFrameOwner::sandboxFlags() const
{
    // FIXME: Implement. Most likely grab it from m_frame.
    return 0;
}

void RemoteBridgeFrameOwner::dispatchLoad()
{
    // FIXME: Implement. Most likely goes through m_frame->client().
}

} // namespace

bool PlaceholderFrameOwner::isLocal() const
{
    return false;
}

SandboxFlags PlaceholderFrameOwner::sandboxFlags() const
{
    return 0;
}

void PlaceholderFrameOwner::dispatchLoad()
{
}

WebRemoteFrame* WebRemoteFrame::create(WebRemoteFrameClient* client)
{
    WebRemoteFrameImpl* frame = new WebRemoteFrameImpl(client);
#if ENABLE(OILPAN)
    return frame;
#else
    return adoptRef(frame).leakRef();
#endif
}

WebRemoteFrameImpl::WebRemoteFrameImpl(WebRemoteFrameClient* client)
    : m_frameClient(this)
    , m_client(client)
#if ENABLE(OILPAN)
    , m_selfKeepAlive(this)
#endif
{
}

WebRemoteFrameImpl::~WebRemoteFrameImpl()
{
}

#if ENABLE(OILPAN)
void WebRemoteFrameImpl::trace(Visitor* visitor)
{
    visitor->trace(m_frame);
    visitor->trace(m_ownersForChildren);
    visitor->registerWeakMembers<WebFrame, &WebFrame::clearWeakFrames>(this);
    WebFrame::traceFrames(visitor, this);
}
#endif

bool WebRemoteFrameImpl::isWebLocalFrame() const
{
    return false;
}

WebLocalFrame* WebRemoteFrameImpl::toWebLocalFrame()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool WebRemoteFrameImpl::isWebRemoteFrame() const
{
    return true;
}

WebRemoteFrame* WebRemoteFrameImpl::toWebRemoteFrame()
{
    return this;
}

void WebRemoteFrameImpl::close()
{
#if ENABLE(OILPAN)
    m_selfKeepAlive.clear();
#else
    deref();
#endif
}

WebString WebRemoteFrameImpl::uniqueName() const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::assignedName() const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

void WebRemoteFrameImpl::setName(const WebString&)
{
    ASSERT_NOT_REACHED();
}

WebVector<WebIconURL> WebRemoteFrameImpl::iconURLs(int iconTypesMask) const
{
    ASSERT_NOT_REACHED();
    return WebVector<WebIconURL>();
}

void WebRemoteFrameImpl::setRemoteWebLayer(WebLayer* webLayer)
{
    if (!frame())
        return;

    frame()->setRemotePlatformLayer(webLayer);
}

void WebRemoteFrameImpl::setPermissionClient(WebPermissionClient*)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setCanHaveScrollbars(bool)
{
    ASSERT_NOT_REACHED();
}

WebSize WebRemoteFrameImpl::scrollOffset() const
{
    ASSERT_NOT_REACHED();
    return WebSize();
}

void WebRemoteFrameImpl::setScrollOffset(const WebSize&)
{
    ASSERT_NOT_REACHED();
}

WebSize WebRemoteFrameImpl::minimumScrollOffset() const
{
    ASSERT_NOT_REACHED();
    return WebSize();
}

WebSize WebRemoteFrameImpl::maximumScrollOffset() const
{
    ASSERT_NOT_REACHED();
    return WebSize();
}

WebSize WebRemoteFrameImpl::contentsSize() const
{
    ASSERT_NOT_REACHED();
    return WebSize();
}

bool WebRemoteFrameImpl::hasVisibleContent() const
{
    ASSERT_NOT_REACHED();
    return false;
}

WebRect WebRemoteFrameImpl::visibleContentRect() const
{
    ASSERT_NOT_REACHED();
    return WebRect();
}

bool WebRemoteFrameImpl::hasHorizontalScrollbar() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool WebRemoteFrameImpl::hasVerticalScrollbar() const
{
    ASSERT_NOT_REACHED();
    return false;
}

WebView* WebRemoteFrameImpl::view() const
{
    if (!frame())
        return 0;
    return WebViewImpl::fromPage(frame()->page());
}

void WebRemoteFrameImpl::removeChild(WebFrame* frame)
{
    WebFrame::removeChild(frame);
    m_ownersForChildren.remove(frame);
}

WebDocument WebRemoteFrameImpl::document() const
{
    return WebDocument();
}

WebPerformance WebRemoteFrameImpl::performance() const
{
    ASSERT_NOT_REACHED();
    return WebPerformance();
}

bool WebRemoteFrameImpl::dispatchBeforeUnloadEvent()
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::dispatchUnloadEvent()
{
    ASSERT_NOT_REACHED();
}

NPObject* WebRemoteFrameImpl::windowObject() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void WebRemoteFrameImpl::bindToWindowObject(const WebString& name, NPObject*)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::bindToWindowObject(const WebString& name, NPObject*, void*)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::executeScript(const WebScriptSource&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::executeScriptInIsolatedWorld(
    int worldID, const WebScriptSource* sources, unsigned numSources,
    int extensionGroup)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::addMessageToConsole(const WebConsoleMessage&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::collectGarbage()
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::checkIfRunInsecureContent(const WebURL&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

v8::Handle<v8::Value> WebRemoteFrameImpl::executeScriptAndReturnValue(
    const WebScriptSource&)
{
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Value>();
}

void WebRemoteFrameImpl::executeScriptInIsolatedWorld(
    int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
    int extensionGroup, WebVector<v8::Local<v8::Value> >* results)
{
    ASSERT_NOT_REACHED();
}

v8::Handle<v8::Value> WebRemoteFrameImpl::callFunctionEvenIfScriptDisabled(
    v8::Handle<v8::Function>,
    v8::Handle<v8::Value>,
    int argc,
    v8::Handle<v8::Value> argv[])
{
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Value>();
}

v8::Local<v8::Context> WebRemoteFrameImpl::mainWorldScriptContext() const
{
    ASSERT_NOT_REACHED();
    return v8::Local<v8::Context>();
}

void WebRemoteFrameImpl::reload(bool ignoreCache)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::loadRequest(const WebURLRequest&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::loadHistoryItem(const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::loadData(
    const WebData&, const WebString& mimeType, const WebString& textEncoding,
    const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::loadHTMLString(
    const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
    bool replace)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::stopLoading()
{
    ASSERT_NOT_REACHED();
}

WebDataSource* WebRemoteFrameImpl::provisionalDataSource() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

WebDataSource* WebRemoteFrameImpl::dataSource() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void WebRemoteFrameImpl::enableViewSourceMode(bool enable)
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::isViewSourceModeEnabled() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::setReferrerForRequest(WebURLRequest&, const WebURL& referrer)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::dispatchWillSendRequest(WebURLRequest&)
{
    ASSERT_NOT_REACHED();
}

WebURLLoader* WebRemoteFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions&)
{
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned WebRemoteFrameImpl::unloadListenerCount() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void WebRemoteFrameImpl::replaceSelection(const WebString&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::insertText(const WebString&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setMarkedText(const WebString&, unsigned location, unsigned length)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::unmarkText()
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::hasMarkedText() const
{
    ASSERT_NOT_REACHED();
    return false;
}

WebRange WebRemoteFrameImpl::markedRange() const
{
    ASSERT_NOT_REACHED();
    return WebRange();
}

bool WebRemoteFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

size_t WebRemoteFrameImpl::characterIndexForPoint(const WebPoint&) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool WebRemoteFrameImpl::executeCommand(const WebString&, const WebNode&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool WebRemoteFrameImpl::executeCommand(const WebString&, const WebString& value, const WebNode&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool WebRemoteFrameImpl::isCommandEnabled(const WebString&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::enableContinuousSpellChecking(bool)
{
}

bool WebRemoteFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return false;
}

void WebRemoteFrameImpl::requestTextChecking(const WebElement&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::replaceMisspelledRange(const WebString&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::removeSpellingMarkers()
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::hasSelection() const
{
    ASSERT_NOT_REACHED();
    return false;
}

WebRange WebRemoteFrameImpl::selectionRange() const
{
    ASSERT_NOT_REACHED();
    return WebRange();
}

WebString WebRemoteFrameImpl::selectionAsText() const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::selectionAsMarkup() const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

bool WebRemoteFrameImpl::selectWordAroundCaret()
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::selectRange(const WebRange&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::moveRangeSelection(const WebPoint& base, const WebPoint& extent)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::moveCaretSelection(const WebPoint&)
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::setEditableSelectionOffsets(int start, int end)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool WebRemoteFrameImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::extendSelectionAndDelete(int before, int after)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::setCaretVisible(bool)
{
    ASSERT_NOT_REACHED();
}

int WebRemoteFrameImpl::printBegin(const WebPrintParams&, const WebNode& constrainToNode)
{
    ASSERT_NOT_REACHED();
    return 0;
}

float WebRemoteFrameImpl::printPage(int pageToPrint, WebCanvas*)
{
    ASSERT_NOT_REACHED();
    return 0.0;
}

float WebRemoteFrameImpl::getPrintPageShrink(int page)
{
    ASSERT_NOT_REACHED();
    return 0.0;
}

void WebRemoteFrameImpl::printEnd()
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::isPrintScalingDisabledForPlugin(const WebNode&)
{
    ASSERT_NOT_REACHED();
    return false;
}

int WebRemoteFrameImpl::getPrintCopiesForPlugin(const WebNode&)
{
    ASSERT_NOT_REACHED();
    return 1;
}

bool WebRemoteFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool WebRemoteFrameImpl::isPageBoxVisible(int pageIndex)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::pageSizeAndMarginsInPixels(
    int pageIndex,
    WebSize& pageSize,
    int& marginTop,
    int& marginRight,
    int& marginBottom,
    int& marginLeft)
{
    ASSERT_NOT_REACHED();
}

WebString WebRemoteFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    ASSERT_NOT_REACHED();
    return WebString();
}

void WebRemoteFrameImpl::printPagesWithBoundaries(WebCanvas*, const WebSize&)
{
    ASSERT_NOT_REACHED();
}

bool WebRemoteFrameImpl::find(
    int identifier, const WebString& searchText, const WebFindOptions&,
    bool wrapWithinFrame, WebRect* selectionRect)
{
    ASSERT_NOT_REACHED();
    return false;
}

void WebRemoteFrameImpl::stopFinding(bool clearSelection)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::scopeStringMatches(
    int identifier, const WebString& searchText, const WebFindOptions&,
    bool reset)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::cancelPendingScopingEffort()
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::increaseMatchCount(int count, int identifier)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::resetMatchCount()
{
    ASSERT_NOT_REACHED();
}

int WebRemoteFrameImpl::findMatchMarkersVersion() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

WebFloatRect WebRemoteFrameImpl::activeFindMatchRect()
{
    ASSERT_NOT_REACHED();
    return WebFloatRect();
}

void WebRemoteFrameImpl::findMatchRects(WebVector<WebFloatRect>&)
{
    ASSERT_NOT_REACHED();
}

int WebRemoteFrameImpl::selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect)
{
    ASSERT_NOT_REACHED();
    return 0;
}

void WebRemoteFrameImpl::setTickmarks(const WebVector<WebRect>&)
{
    ASSERT_NOT_REACHED();
}

void WebRemoteFrameImpl::dispatchMessageEventWithOriginCheck(
    const WebSecurityOrigin& intendedTargetOrigin,
    const WebDOMEvent&)
{
    ASSERT_NOT_REACHED();
}

WebString WebRemoteFrameImpl::contentAsText(size_t maxChars) const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::contentAsMarkup() const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::renderTreeAsText(RenderAsTextControls toShow) const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::markerTextForListItem(const WebElement&) const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebRect WebRemoteFrameImpl::selectionBoundsRect() const
{
    ASSERT_NOT_REACHED();
    return WebRect();
}

bool WebRemoteFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    ASSERT_NOT_REACHED();
    return false;
}

WebString WebRemoteFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    ASSERT_NOT_REACHED();
    return WebString();
}

WebLocalFrame* WebRemoteFrameImpl::createLocalChild(const WebString& name, WebFrameClient* client)
{
    WebLocalFrameImpl* child = toWebLocalFrameImpl(WebLocalFrame::create(client));
    WillBeHeapHashMap<WebFrame*, OwnPtrWillBeMember<FrameOwner> >::AddResult result =
        m_ownersForChildren.add(child, RemoteBridgeFrameOwner::create(child));
    appendChild(child);
    // FIXME: currently this calls LocalFrame::init() on the created LocalFrame, which may
    // result in the browser observing two navigations to about:blank (one from the initial
    // frame creation, and one from swapping it into the remote process). FrameLoader might
    // need a special initialization function for this case to avoid that duplicate navigation.
    child->initializeCoreFrame(frame()->host(), result.storedValue->value.get(), name, nullAtom);
    // Partially related with the above FIXME--the init() call may trigger JS dispatch. However,
    // if the parent is remote, it should never be detached synchronously...
    ASSERT(child->frame());
    return child;
}

void WebRemoteFrameImpl::initializeCoreFrame(FrameHost* host, FrameOwner* owner, const AtomicString& name)
{
    setCoreFrame(RemoteFrame::create(&m_frameClient, host, owner));
    m_frame->tree().setName(name, nullAtom);
}

WebRemoteFrame* WebRemoteFrameImpl::createRemoteChild(const WebString& name, WebRemoteFrameClient* client)
{
    WebRemoteFrameImpl* child = toWebRemoteFrameImpl(WebRemoteFrame::create(client));
    WillBeHeapHashMap<WebFrame*, OwnPtrWillBeMember<FrameOwner> >::AddResult result =
        m_ownersForChildren.add(child, adoptPtrWillBeNoop(new PlaceholderFrameOwner));
    appendChild(child);
    child->initializeCoreFrame(frame()->host(), result.storedValue->value.get(), name);
    return child;
}

void WebRemoteFrameImpl::setCoreFrame(PassRefPtrWillBeRawPtr<RemoteFrame> frame)
{
    m_frame = frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::fromFrame(RemoteFrame& frame)
{
    if (!frame.client())
        return 0;
    return static_cast<RemoteFrameClientImpl*>(frame.client())->webFrame();
}

void WebRemoteFrameImpl::initializeFromFrame(WebLocalFrame* source) const
{
    ASSERT(source);
    WebLocalFrameImpl* localFrameImpl = toWebLocalFrameImpl(source);
    client()->initializeChildFrame(
        localFrameImpl->frame()->view()->frameRect(),
        localFrameImpl->frame()->view()->visibleContentScaleFactor());
}

} // namespace blink
