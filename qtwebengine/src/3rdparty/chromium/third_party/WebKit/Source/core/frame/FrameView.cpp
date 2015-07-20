/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/frame/FrameView.h"

#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/FontFaceSet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/Fullscreen.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/events/OverflowEvent.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoadPriorityOptimizer.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/FramePainter.h"
#include "core/rendering/RenderCounter.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderListBox.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/CompositedSelectionBound.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/HostWindow.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerDebugInfo.h"
#include "platform/scroll/ScrollAnimator.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/TextStream.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"

namespace blink {

using namespace HTMLNames;

double FrameView::s_currentFrameTimeStamp = 0.0;

// The maximum number of updateWidgets iterations that should be done before returning.
static const unsigned maxUpdateWidgetsIterations = 2;
static const double resourcePriorityUpdateDelayAfterScroll = 0.250;

FrameView::FrameView(LocalFrame* frame)
    : m_frame(frame)
    , m_canHaveScrollbars(true)
    , m_slowRepaintObjectCount(0)
    , m_hasPendingLayout(false)
    , m_layoutSubtreeRoot(0)
    , m_inSynchronousPostLayout(false)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_updateWidgetsTimer(this, &FrameView::updateWidgetsTimerFired)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType(MediaTypeNames::screen)
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_safeToPropagateScrollToParent(true)
    , m_isTrackingPaintInvalidations(false)
    , m_scrollCorner(nullptr)
    , m_visibleContentScaleFactor(1)
    , m_inputEventsScaleFactorForEmulation(1)
    , m_layoutSizeFixedToFrameSize(true)
    , m_didScrollTimer(this, &FrameView::didScrollTimerFired)
    , m_topControlsViewportAdjustment(0)
    , m_needsUpdateWidgetPositions(false)
#if ENABLE(OILPAN) && ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
    , m_horizontalScrollbarMode(ScrollbarAuto)
    , m_verticalScrollbarMode(ScrollbarAuto)
    , m_horizontalScrollbarLock(false)
    , m_verticalScrollbarLock(false)
    , m_scrollbarsAvoidingResizer(0)
    , m_scrollbarsSuppressed(false)
    , m_inUpdateScrollbars(false)
    , m_shouldDrawPanScrollIcon(false)
    , m_clipsRepaints(true)
{
    ASSERT(m_frame);
    init();

    if (!m_frame->isMainFrame())
        return;

    ScrollableArea::setVerticalScrollElasticity(ScrollElasticityAllowed);
    ScrollableArea::setHorizontalScrollElasticity(ScrollElasticityAllowed);
}

PassRefPtrWillBeRawPtr<FrameView> FrameView::create(LocalFrame* frame)
{
    RefPtrWillBeRawPtr<FrameView> view = adoptRefWillBeNoop(new FrameView(frame));
    view->show();
    return view.release();
}

PassRefPtrWillBeRawPtr<FrameView> FrameView::create(LocalFrame* frame, const IntSize& initialSize)
{
    RefPtrWillBeRawPtr<FrameView> view = adoptRefWillBeNoop(new FrameView(frame));
    view->Widget::setFrameRect(IntRect(view->location(), initialSize));
    view->setLayoutSizeInternal(initialSize);

    view->show();
    return view.release();
}

FrameView::~FrameView()
{
#if ENABLE(OILPAN)
    ASSERT(m_hasBeenDisposed);
#else
    // Verify that the LocalFrame has a different FrameView or
    // that it is being detached and destructed.
    ASSERT(frame().view() != this || !renderView());
    dispose();
#endif
}

void FrameView::dispose()
{
    if (m_postLayoutTasksTimer.isActive())
        m_postLayoutTasksTimer.stop();

    if (m_didScrollTimer.isActive())
        m_didScrollTimer.stop();

    removeFromAXObjectCache();

    // Custom scrollbars should already be destroyed at this point
    ASSERT(!horizontalScrollbar() || !horizontalScrollbar()->isCustomScrollbar());
    ASSERT(!verticalScrollbar() || !verticalScrollbar()->isCustomScrollbar());

    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);

    ASSERT(!m_scrollCorner);

    // FIXME: Do we need to do something here for OOPI?
    HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
    if (ownerElement && ownerElement->ownedWidget() == this)
        ownerElement->setWidget(nullptr);

    disposeAutoSizeInfo();
#if ENABLE(OILPAN) && ENABLE(ASSERT)
    m_hasBeenDisposed = true;
#endif
}

void FrameView::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_partUpdateSet);
    visitor->trace(m_parts);
    visitor->trace(m_frame);
    visitor->trace(m_nodeToDraw);
    visitor->trace(m_maintainScrollPositionAnchor);
    visitor->trace(m_scrollCorner);
    visitor->trace(m_autoSizeInfo);
    visitor->trace(m_horizontalScrollbar);
    visitor->trace(m_verticalScrollbar);
    visitor->trace(m_children);
#endif
    Widget::trace(visitor);
}

void FrameView::reset()
{
    m_hasPendingLayout = false;
    m_layoutSubtreeRoot = 0;
    m_doFullPaintInvalidation = false;
    m_layoutSchedulingEnabled = true;
    m_inPerformLayout = false;
    m_canInvalidatePaintDuringPerformLayout = false;
    m_inSynchronousPostLayout = false;
    m_layoutCount = 0;
    m_nestedLayoutCount = 0;
    m_postLayoutTasksTimer.stop();
    m_updateWidgetsTimer.stop();
    m_firstLayout = true;
    m_firstLayoutCallbackPending = false;
    m_wasScrolledByUser = false;
    m_safeToPropagateScrollToParent = true;
    m_lastViewportSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_isTrackingPaintInvalidations = false;
    m_trackedPaintInvalidationRects.clear();
    m_lastPaintTime = 0;
    m_paintBehavior = PaintBehaviorNormal;
    m_isPainting = false;
    m_visuallyNonEmptyCharacterCount = 0;
    m_visuallyNonEmptyPixelCount = 0;
    m_isVisuallyNonEmpty = false;
    m_firstVisuallyNonEmptyLayoutCallbackPending = true;
    m_maintainScrollPositionAnchor = nullptr;
    m_viewportConstrainedObjects.clear();
}

void FrameView::removeFromAXObjectCache()
{
    if (AXObjectCache* cache = axObjectCache()) {
        cache->remove(this);
        cache->childrenChanged(m_frame->pagePopupOwner());
    }
}

void FrameView::init()
{
    reset();

    m_size = LayoutSize();

    // Propagate the marginwidth/height and scrolling modes to the view.
    // FIXME: Do we need to do this for OOPI?
    Element* ownerElement = m_frame->deprecatedLocalOwner();
    if (ownerElement && (isHTMLFrameElement(*ownerElement) || isHTMLIFrameElement(*ownerElement))) {
        HTMLFrameElementBase* frameElt = toHTMLFrameElementBase(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
    }
}

void FrameView::prepareForDetach()
{
    RELEASE_ASSERT(!isInPerformLayout());

    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        scrollAnimator->cancelAnimations();
    cancelProgrammaticScrollAnimation();

    detachCustomScrollbars();
    // When the view is no longer associated with a frame, it needs to be removed from the ax object cache
    // right now, otherwise it won't be able to reach the topDocument()'s axObject cache later.
    removeFromAXObjectCache();

    if (m_frame->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_frame->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

#if ENABLE(OILPAN)
    // FIXME: once/if dust settles, do this always (non-Oilpan)?
    //
    // FIXME: Oilpan: is this safe to dispose() if there are FrameView protections on the stack?
    dispose();
#endif
}

void FrameView::detachCustomScrollbars()
{
    Scrollbar* horizontalBar = horizontalScrollbar();
    if (horizontalBar && horizontalBar->isCustomScrollbar())
        setHasHorizontalScrollbar(false);

    Scrollbar* verticalBar = verticalScrollbar();
    if (verticalBar && verticalBar->isCustomScrollbar())
        setHasVerticalScrollbar(false);

    if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = nullptr;
    }
}

void FrameView::recalculateCustomScrollbarStyle()
{
    bool didStyleChange = false;
    if (m_horizontalScrollbar && m_horizontalScrollbar->isCustomScrollbar()) {
        m_horizontalScrollbar->styleChanged();
        didStyleChange = true;
    }
    if (m_verticalScrollbar && m_verticalScrollbar->isCustomScrollbar()) {
        m_verticalScrollbar->styleChanged();
        didStyleChange = true;
    }
    if (didStyleChange) {
        updateScrollbarGeometry();
        updateScrollCorner();
        positionScrollbarLayers();
    }
}

void FrameView::recalculateScrollbarOverlayStyle()
{
    ScrollbarOverlayStyle oldOverlayStyle = scrollbarOverlayStyle();
    ScrollbarOverlayStyle overlayStyle = ScrollbarOverlayStyleDefault;

    Color backgroundColor = documentBackgroundColor();
    // Reduce the background color from RGB to a lightness value
    // and determine which scrollbar style to use based on a lightness
    // heuristic.
    double hue, saturation, lightness;
    backgroundColor.getHSL(hue, saturation, lightness);
    if (lightness <= .5)
        overlayStyle = ScrollbarOverlayStyleLight;

    if (oldOverlayStyle != overlayStyle)
        setScrollbarOverlayStyle(overlayStyle);
}

void FrameView::clear()
{
    reset();
    setScrollbarsSuppressed(true);
}

bool FrameView::didFirstLayout() const
{
    return !m_firstLayout;
}

void FrameView::invalidateRect(const IntRect& rect)
{
    // For querying RenderLayer::compositingState() when invalidating scrollbars.
    // FIXME: do all scrollbar invalidations after layout of all frames is complete. It's currently not recursively true.
    DisableCompositingQueryAsserts disabler;
    if (!parent()) {
        if (HostWindow* window = hostWindow())
            window->invalidateContentsAndRootView(rect);
        return;
    }

    RenderPart* renderer = m_frame->ownerRenderer();
    if (!renderer)
        return;

    IntRect paintInvalidationRect = rect;
    paintInvalidationRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                     renderer->borderTop() + renderer->paddingTop());
    renderer->invalidatePaintRectangle(paintInvalidationRect);
}

void FrameView::setFrameRect(const IntRect& newRect)
{
    IntRect oldRect = frameRect();
    if (newRect == oldRect)
        return;

    // Autosized font sizes depend on the width of the viewing area.
    bool autosizerNeedsUpdating = false;
    if (newRect.width() != oldRect.width() && m_frame->isMainFrame() && m_frame->settings()->textAutosizingEnabled())
        autosizerNeedsUpdating = true;

    Widget::setFrameRect(newRect);

    updateScrollbars(scrollOffsetDouble());
    frameRectsChanged();

    updateScrollableAreaSet();

    if (autosizerNeedsUpdating) {
        // This needs to be after the call to Widget::setFrameRect, because it reads the new width.
        if (TextAutosizer* textAutosizer = m_frame->document()->textAutosizer())
            textAutosizer->updatePageInfoInAllFrames();
    }

    if (RenderView* renderView = this->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor()->frameViewDidChangeSize();
    }

    viewportConstrainedVisibleContentSizeChanged(newRect.width() != oldRect.width(), newRect.height() != oldRect.height());

    if (oldRect.size() != newRect.size()
        && m_frame->isMainFrame()
        && m_frame->settings()->pinchVirtualViewportEnabled())
        page()->frameHost().pinchViewport().mainFrameDidChangeSize();
}

Page* FrameView::page() const
{
    return frame().page();
}

RenderView* FrameView::renderView() const
{
    return frame().contentRenderer();
}

void FrameView::setCanHaveScrollbars(bool canHaveScrollbars)
{
    m_canHaveScrollbars = canHaveScrollbars;

    ScrollbarMode newHorizontalMode;
    ScrollbarMode newVerticalMode;

    scrollbarModes(newHorizontalMode, newVerticalMode);

    if (canHaveScrollbars && newVerticalMode == ScrollbarAlwaysOff)
        newVerticalMode = ScrollbarAuto;
    else if (!canHaveScrollbars)
        newVerticalMode = ScrollbarAlwaysOff;

    if (canHaveScrollbars && newHorizontalMode == ScrollbarAlwaysOff)
        newHorizontalMode = ScrollbarAuto;
    else if (!canHaveScrollbars)
        newHorizontalMode = ScrollbarAlwaysOff;

    setScrollbarModes(newHorizontalMode, newVerticalMode);
}

bool FrameView::shouldUseCustomScrollbars(Element*& customScrollbarElement, LocalFrame*& customScrollbarFrame)
{
    customScrollbarElement = 0;
    customScrollbarFrame = 0;

    if (Settings* settings = m_frame->settings()) {
        if (!settings->allowCustomScrollbarInMainFrame() && m_frame->isMainFrame())
            return false;
    }

    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    Document* doc = m_frame->document();

    // Try the <body> element first as a scrollbar source.
    Element* body = doc ? doc->body() : 0;
    if (body && body->renderer() && body->renderer()->style()->hasPseudoStyle(SCROLLBAR)) {
        customScrollbarElement = body;
        return true;
    }

    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc ? doc->documentElement() : 0;
    if (docElement && docElement->renderer() && docElement->renderer()->style()->hasPseudoStyle(SCROLLBAR)) {
        customScrollbarElement = docElement;
        return true;
    }

    // If we have an owning ipage/LocalFrame element, then it can set the custom scrollbar also.
    RenderPart* frameRenderer = m_frame->ownerRenderer();
    if (frameRenderer && frameRenderer->style()->hasPseudoStyle(SCROLLBAR)) {
        customScrollbarFrame = m_frame.get();
        return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<Scrollbar> FrameView::createScrollbar(ScrollbarOrientation orientation)
{
    Element* customScrollbarElement = 0;
    LocalFrame* customScrollbarFrame = 0;
    if (shouldUseCustomScrollbars(customScrollbarElement, customScrollbarFrame))
        return RenderScrollbar::createCustomScrollbar(this, orientation, customScrollbarElement, customScrollbarFrame);

    // Nobody set a custom style, so we just use a native scrollbar.
    return Scrollbar::create(this, orientation, RegularScrollbar);
}

void FrameView::setContentsSize(const IntSize& size)
{
    if (size == contentsSize())
        return;

    m_contentsSize = size;
    updateScrollbars(scrollOffsetDouble());
    updateOverhangAreas();
    ScrollableArea::contentsResized();

    Page* page = frame().page();
    if (!page)
        return;

    updateScrollableAreaSet();

    page->chrome().contentsSizeChanged(m_frame.get(), size);
}

IntPoint FrameView::clampOffsetAtScale(const IntPoint& offset, float scale) const
{
    IntPoint maxScrollExtent(contentsSize().width() - scrollOrigin().x(), contentsSize().height() - scrollOrigin().y());
    FloatSize scaledSize = unscaledVisibleContentSize();
    if (scale)
        scaledSize.scale(1 / scale);

    IntPoint clampedOffset = offset;
    clampedOffset = clampedOffset.shrunkTo(maxScrollExtent - expandedIntSize(scaledSize));
    clampedOffset = clampedOffset.expandedTo(-scrollOrigin());

    return clampedOffset;
}

void FrameView::adjustViewSize()
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return;

    ASSERT(m_frame->view() == this);

    const IntRect rect = renderView->documentRect();
    const IntSize& size = rect.size();
    setScrollOrigin(IntPoint(-rect.x(), -rect.y()), !m_frame->document()->printing(), size == contentsSize());

    setContentsSize(size);
}

void FrameView::applyOverflowToViewportAndSetRenderer(RenderObject* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.

    EOverflow overflowX = o->style()->overflowX();
    EOverflow overflowY = o->style()->overflowY();

    if (o->isSVGRoot()) {
        // Don't allow overflow to affect <img> and css backgrounds
        if (toRenderSVGRoot(o)->isEmbeddedThroughSVGImage())
            return;

        // FIXME: evaluate if we can allow overflow for these cases too.
        // Overflow is always hidden when stand-alone SVG documents are embedded.
        if (toRenderSVGRoot(o)->isEmbeddedThroughFrameContainingSVGDocument()) {
            overflowX = OHIDDEN;
            overflowY = OHIDDEN;
        }
    }

    bool ignoreOverflowHidden = false;
    if (m_frame->settings()->ignoreMainFrameOverflowHiddenQuirk() && m_frame->isMainFrame())
        ignoreOverflowHidden = true;

    switch (overflowX) {
        case OHIDDEN:
            if (!ignoreOverflowHidden)
                hMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            hMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            hMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }

     switch (overflowY) {
        case OHIDDEN:
            if (!ignoreOverflowHidden)
                vMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            vMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            vMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }

    m_viewportRenderer = o;
}

void FrameView::calculateScrollbarModesForLayoutAndSetViewportRenderer(ScrollbarMode& hMode, ScrollbarMode& vMode, ScrollbarModesCalculationStrategy strategy)
{
    m_viewportRenderer = 0;

    // FIXME: How do we handle this for OOPI?
    const HTMLFrameOwnerElement* owner = m_frame->deprecatedLocalOwner();
    if (owner && (owner->scrollingMode() == ScrollbarAlwaysOff)) {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
        return;
    }

    if (m_canHaveScrollbars || strategy == RulesFromWebContentOnly) {
        hMode = ScrollbarAuto;
        vMode = ScrollbarAuto;
    } else {
        hMode = ScrollbarAlwaysOff;
        vMode = ScrollbarAlwaysOff;
    }

    if (!isSubtreeLayout()) {
        Document* document = m_frame->document();
        Node* body = document->body();
        if (isHTMLFrameSetElement(body) && body->renderer()) {
            vMode = ScrollbarAlwaysOff;
            hMode = ScrollbarAlwaysOff;
        } else if (Element* viewportElement = document->viewportDefiningElement()) {
            if (RenderObject* viewportRenderer = viewportElement->renderer()) {
                if (viewportRenderer->style())
                    applyOverflowToViewportAndSetRenderer(viewportRenderer, hMode, vMode);
            }
        }
    }
}

void FrameView::updateAcceleratedCompositingSettings()
{
    if (RenderView* renderView = this->renderView())
        renderView->compositor()->updateAcceleratedCompositingSettings();
}

void FrameView::recalcOverflowAfterStyleChange()
{
    RenderView* renderView = this->renderView();
    RELEASE_ASSERT(renderView);
    if (!renderView->needsOverflowRecalcAfterStyleChange())
        return;

    renderView->recalcOverflowAfterStyleChange();

    IntRect documentRect = renderView->documentRect();
    if (scrollOrigin() == -documentRect.location() && contentsSize() == documentRect.size())
        return;

    if (needsLayout())
        return;

    InUpdateScrollbarsScope inUpdateScrollbarsScope(this);

    bool shouldHaveHorizontalScrollbar = false;
    bool shouldHaveVerticalScrollbar = false;
    computeScrollbarExistence(shouldHaveHorizontalScrollbar, shouldHaveVerticalScrollbar, documentRect.size());

    bool hasHorizontalScrollbar = horizontalScrollbar();
    bool hasVerticalScrollbar = verticalScrollbar();
    if (hasHorizontalScrollbar != shouldHaveHorizontalScrollbar
        || hasVerticalScrollbar != shouldHaveVerticalScrollbar) {
        setNeedsLayout();
        return;
    }

    adjustViewSize();
    updateScrollbarGeometry();
}

bool FrameView::usesCompositedScrolling() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;
    if (m_frame->settings() && m_frame->settings()->preferCompositingToLCDTextEnabled())
        return renderView->compositor()->inCompositingMode();
    return false;
}

GraphicsLayer* FrameView::layerForScrolling() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
}

GraphicsLayer* FrameView::layerForHorizontalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForHorizontalScrollbar();
}

GraphicsLayer* FrameView::layerForVerticalScrollbar() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForVerticalScrollbar();
}

GraphicsLayer* FrameView::layerForScrollCorner() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;
    return renderView->compositor()->layerForScrollCorner();
}

bool FrameView::isEnclosedInCompositingLayer() const
{
    // FIXME: It's a bug that compositing state isn't always up to date when this is called. crbug.com/366314
    DisableCompositingQueryAsserts disabler;

    RenderObject* frameOwnerRenderer = m_frame->ownerRenderer();
    return frameOwnerRenderer && frameOwnerRenderer->enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries();
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : m_layoutSubtreeRoot;
}

inline void FrameView::forceLayoutParentViewIfNeeded()
{
    RenderPart* ownerRenderer = m_frame->ownerRenderer();
    if (!ownerRenderer || !ownerRenderer->frame())
        return;

    RenderBox* contentBox = embeddedContentBox();
    if (!contentBox)
        return;

    RenderSVGRoot* svgRoot = toRenderSVGRoot(contentBox);
    if (svgRoot->everHadLayout() && !svgRoot->needsLayout())
        return;

    // If the embedded SVG document appears the first time, the ownerRenderer has already finished
    // layout without knowing about the existence of the embedded SVG document, because RenderReplaced
    // embeddedContentBox() returns 0, as long as the embedded document isn't loaded yet. Before
    // bothering to lay out the SVG document, mark the ownerRenderer needing layout and ask its
    // FrameView for a layout. After that the RenderEmbeddedObject (ownerRenderer) carries the
    // correct size, which RenderSVGRoot::computeReplacedLogicalWidth/Height rely on, when laying
    // out for the first time, or when the RenderSVGRoot size has changed dynamically (eg. via <script>).
    RefPtrWillBeRawPtr<FrameView> frameView = ownerRenderer->frame()->view();

    // Mark the owner renderer as needing layout.
    ownerRenderer->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();

    // Synchronously enter layout, to layout the view containing the host object/embed/iframe.
    ASSERT(frameView);
    frameView->layout();
}

void FrameView::performPreLayoutTasks()
{
    TRACE_EVENT0("blink", "FrameView::performPreLayoutTasks");
    lifecycle().advanceTo(DocumentLifecycle::InPreLayout);

    // Don't schedule more layouts, we're in one.
    TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

    if (!m_nestedLayoutCount && !m_inSynchronousPostLayout && m_postLayoutTasksTimer.isActive()) {
        // This is a new top-level layout. If there are any remaining tasks from the previous layout, finish them now.
        m_inSynchronousPostLayout = true;
        performPostLayoutTasks();
        m_inSynchronousPostLayout = false;
    }

    Document* document = m_frame->document();
    if (wasViewportResized())
        document->notifyResizeForViewportUnits();

    // Viewport-dependent media queries may cause us to need completely different style information.
    if (!document->styleResolver() || document->styleResolver()->mediaQueryAffectedByViewportChange()) {
        document->styleResolverChanged();
        document->mediaQueryAffectingValueChanged();

        // FIXME: This instrumentation event is not strictly accurate since cached media query results
        //        do not persist across StyleResolver rebuilds.
        InspectorInstrumentation::mediaQueryResultChanged(document);
    } else {
        document->evaluateMediaQueryList();
    }

    document->updateRenderTreeIfNeeded();
    lifecycle().advanceTo(DocumentLifecycle::StyleClean);
}

void FrameView::performLayout(RenderObject* rootForThisLayout, bool inSubtreeLayout)
{
    TRACE_EVENT0("blink", "FrameView::performLayout");

    ScriptForbiddenScope forbidScript;

    ASSERT(!isInPerformLayout());
    lifecycle().advanceTo(DocumentLifecycle::InPerformLayout);

    TemporaryChange<bool> changeInPerformLayout(m_inPerformLayout, true);

    // performLayout is the actual guts of layout().
    // FIXME: The 300 other lines in layout() probably belong in other helper functions
    // so that a single human could understand what layout() is actually doing.

    LayoutState layoutState(*rootForThisLayout);

    forceLayoutParentViewIfNeeded();

    // FIXME (crbug.com/256657): Do not do two layouts for text autosizing.
    rootForThisLayout->layout();
    gatherDebugLayoutRects(rootForThisLayout);

    ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->updateAllImageResourcePriorities();

    lifecycle().advanceTo(DocumentLifecycle::AfterPerformLayout);
}

void FrameView::scheduleOrPerformPostLayoutTasks()
{
    if (m_postLayoutTasksTimer.isActive())
        return;

    if (!m_inSynchronousPostLayout) {
        m_inSynchronousPostLayout = true;
        // Calls resumeScheduledEvents()
        performPostLayoutTasks();
        m_inSynchronousPostLayout = false;
    }

    if (!m_postLayoutTasksTimer.isActive() && (needsLayout() || m_inSynchronousPostLayout)) {
        // If we need layout or are already in a synchronous call to postLayoutTasks(),
        // defer widget updates and event dispatch until after we return. postLayoutTasks()
        // can make us need to update again, and we can get stuck in a nasty cycle unless
        // we call it through the timer here.
        m_postLayoutTasksTimer.startOneShot(0, FROM_HERE);
        if (needsLayout())
            layout();
    }
}

void FrameView::layout(bool allowSubtree)
{
    // We should never layout a Document which is not in a LocalFrame.
    ASSERT(m_frame);
    ASSERT(m_frame->view() == this);
    ASSERT(m_frame->page());

    ScriptForbiddenScope forbidScript;

    if (isInPerformLayout() || !m_frame->document()->isActive())
        return;

    TRACE_EVENT0("blink", "FrameView::layout");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "Layout");

    // Protect the view from being deleted during layout (in recalcStyle)
    RefPtrWillBeRawPtr<FrameView> protector(this);

    // Every scroll that happens during layout is programmatic.
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);

    if (m_autoSizeInfo)
        m_autoSizeInfo->autoSizeIfNeeded();

    m_hasPendingLayout = false;
    DocumentLifecycle::Scope lifecycleScope(lifecycle(), DocumentLifecycle::LayoutClean);

    RELEASE_ASSERT(!isPainting());

    TRACE_EVENT_BEGIN1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Layout", "beginData", InspectorLayoutEvent::beginData(this));
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willLayout(m_frame.get());

    if (!allowSubtree && isSubtreeLayout()) {
        m_layoutSubtreeRoot->markContainingBlocksForLayout(false);
        m_layoutSubtreeRoot = 0;
    }

    performPreLayoutTasks();

#if !ENABLE(OILPAN)
    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit,
    // so there's no point to continuing to layout
    if (protector->hasOneRef())
        return;
#endif

    Document* document = m_frame->document();
    bool inSubtreeLayout = isSubtreeLayout();
    RenderObject* rootForThisLayout = inSubtreeLayout ? m_layoutSubtreeRoot : document->renderView();
    if (!rootForThisLayout) {
        // FIXME: Do we need to set m_size here?
        ASSERT_NOT_REACHED();
        return;
    }

    FontCachePurgePreventer fontCachePurgePreventer;
    RenderLayer* layer;
    {
        TemporaryChange<bool> changeSchedulingEnabled(m_layoutSchedulingEnabled, false);

        m_nestedLayoutCount++;
        if (!inSubtreeLayout) {
            Document* document = m_frame->document();
            Node* body = document->body();
            if (body && body->renderer()) {
                if (isHTMLFrameSetElement(*body)) {
                    body->renderer()->setChildNeedsLayout();
                } else if (isHTMLBodyElement(*body)) {
                    if (!m_firstLayout && m_size.height() != layoutSize().height() && body->renderer()->enclosingBox()->stretchesToViewport())
                        body->renderer()->setChildNeedsLayout();
                }
            }
        }
        updateCounters();

        ScrollbarMode hMode;
        ScrollbarMode vMode;
        calculateScrollbarModesForLayoutAndSetViewportRenderer(hMode, vMode);

        if (!inSubtreeLayout) {
            // Now set our scrollbar state for the layout.
            ScrollbarMode currentHMode = horizontalScrollbarMode();
            ScrollbarMode currentVMode = verticalScrollbarMode();

            if (m_firstLayout) {
                setScrollbarsSuppressed(true);

                m_doFullPaintInvalidation = true;
                m_firstLayout = false;
                m_firstLayoutCallbackPending = true;
                m_lastViewportSize = layoutSize(IncludeScrollbars);
                m_lastZoomFactor = rootForThisLayout->style()->zoom();

                // Set the initial vMode to AlwaysOn if we're auto.
                if (vMode == ScrollbarAuto)
                    setVerticalScrollbarMode(ScrollbarAlwaysOn); // This causes a vertical scrollbar to appear.
                // Set the initial hMode to AlwaysOff if we're auto.
                if (hMode == ScrollbarAuto)
                    setHorizontalScrollbarMode(ScrollbarAlwaysOff); // This causes a horizontal scrollbar to disappear.

                setScrollbarModes(hMode, vMode);
                setScrollbarsSuppressed(false, true);
            } else if (hMode != currentHMode || vMode != currentVMode) {
                setScrollbarModes(hMode, vMode);
            }

            LayoutSize oldSize = m_size;

            m_size = LayoutSize(layoutSize().width(), layoutSize().height());

            if (oldSize != m_size && !m_firstLayout) {
                RenderBox* rootRenderer = document->documentElement() ? document->documentElement()->renderBox() : 0;
                RenderBox* bodyRenderer = rootRenderer && document->body() ? document->body()->renderBox() : 0;
                if (bodyRenderer && bodyRenderer->stretchesToViewport())
                    bodyRenderer->setChildNeedsLayout();
                else if (rootRenderer && rootRenderer->stretchesToViewport())
                    rootRenderer->setChildNeedsLayout();
            }

            // We need to set m_doFullPaintInvalidation before triggering layout as RenderObject::checkForPaintInvalidation
            // checks the boolean to disable local paint invalidations.
            m_doFullPaintInvalidation |= renderView()->shouldDoFullPaintInvalidationForNextLayout();
        }

        layer = rootForThisLayout->enclosingLayer();

        performLayout(rootForThisLayout, inSubtreeLayout);

        m_layoutSubtreeRoot = 0;
        // We need to ensure that we mark up all renderers up to the RenderView
        // for paint invalidation. This simplifies our code as we just always
        // do a full tree walk.
        if (RenderObject* container = rootForThisLayout->container())
            container->setMayNeedPaintInvalidation(true);
    } // Reset m_layoutSchedulingEnabled to its previous value.

    if (!inSubtreeLayout && !toRenderView(rootForThisLayout)->document().printing())
        adjustViewSize();

    layer->updateLayerPositionsAfterLayout();

    renderView()->compositor()->didLayout();

    m_layoutCount++;

    if (AXObjectCache* cache = rootForThisLayout->document().axObjectCache()) {
        const KURL& url = rootForThisLayout->document().url();
        if (url.isValid() && !url.isAboutBlankURL())
            cache->handleLayoutComplete(rootForThisLayout);
    }
    updateAnnotatedRegions();

    ASSERT(!rootForThisLayout->needsLayout());

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutSize().width() < contentsWidth(), layoutSize().height() < contentsHeight());

    scheduleOrPerformPostLayoutTasks();

    TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Layout", "endData", InspectorLayoutEvent::endData(rootForThisLayout));
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didLayout(cookie, rootForThisLayout);

    m_nestedLayoutCount--;
    if (m_nestedLayoutCount)
        return;

#if ENABLE(ASSERT)
    // Post-layout assert that nobody was re-marked as needing layout during layout.
    document->renderView()->assertSubtreeIsLaidOut();
#endif

    // FIXME: It should be not possible to remove the FrameView from the frame/page during layout
    // however m_inPerformLayout is not set for most of this function, so none of our RELEASE_ASSERTS
    // in LocalFrame/Page will fire. One of the post-layout tasks is disconnecting the LocalFrame from
    // the page in fast/frames/crash-remove-iframe-during-object-beforeload-2.html
    // necessitating this check here.
    // ASSERT(frame()->page());
    if (frame().page())
        frame().page()->chrome().client().layoutUpdated(m_frame.get());
}

// The plan is to move to compositor-queried paint invalidation, in which case this
// method would setNeedsRedraw on the GraphicsLayers with invalidations and
// let the compositor pick which to actually draw.
// See http://crbug.com/306706
void FrameView::invalidateTreeIfNeeded()
{
    ASSERT(renderView());
    RenderView& rootForPaintInvalidation = *renderView();
    ASSERT(!rootForPaintInvalidation.needsLayout());

    TRACE_EVENT1("blink", "FrameView::invalidateTree", "root", rootForPaintInvalidation.debugName().ascii());

    PaintInvalidationState rootPaintInvalidationState(rootForPaintInvalidation);

    if (m_doFullPaintInvalidation)
        renderView()->compositor()->fullyInvalidatePaint();

    rootForPaintInvalidation.invalidateTreeIfNeeded(rootPaintInvalidationState);

    // Invalidate the paint of the frameviews scrollbars if needed
    if (hasVerticalBarDamage())
        invalidateRect(verticalBarDamage());
    if (hasHorizontalBarDamage())
        invalidateRect(horizontalBarDamage());
    resetScrollbarDamage();


#ifndef NDEBUG
    renderView()->assertSubtreeClearedPaintInvalidationState();
#endif

    if (m_frame->selection().isCaretBoundsDirty())
        m_frame->selection().invalidateCaretRect();
}

DocumentLifecycle& FrameView::lifecycle() const
{
    return m_frame->document()->lifecycle();
}

void FrameView::gatherDebugLayoutRects(RenderObject* layoutRoot)
{
    bool isTracing;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"), &isTracing);
    if (!isTracing)
        return;
    if (!layoutRoot->enclosingLayer()->hasCompositedLayerMapping())
        return;
    // For access to compositedLayerMapping().
    DisableCompositingQueryAsserts disabler;
    GraphicsLayer* graphicsLayer = layoutRoot->enclosingLayer()->compositedLayerMapping()->mainGraphicsLayer();
    if (!graphicsLayer)
        return;

    GraphicsLayerDebugInfo& debugInfo = graphicsLayer->debugInfo();

    debugInfo.currentLayoutRects().clear();
    for (RenderObject* renderer = layoutRoot; renderer; renderer = renderer->nextInPreOrder()) {
        if (renderer->layoutDidGetCalledSinceLastFrame()) {
            FloatQuad quad = renderer->localToAbsoluteQuad(FloatQuad(renderer->previousPaintInvalidationRect()));
            LayoutRect rect = quad.enclosingBoundingBox();
            debugInfo.currentLayoutRects().append(rect);
        }
    }
}

RenderBox* FrameView::embeddedContentBox() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return 0;

    RenderObject* firstChild = renderView->firstChild();
    if (!firstChild || !firstChild->isBox())
        return 0;

    // Curently only embedded SVG documents participate in the size-negotiation logic.
    if (firstChild->isSVGRoot())
        return toRenderBox(firstChild);

    return 0;
}


void FrameView::addPart(RenderPart* object)
{
    m_parts.add(object);
}

void FrameView::removePart(RenderPart* object)
{
    m_parts.remove(object);
}

void FrameView::updateWidgetPositions()
{
    WillBeHeapVector<RefPtrWillBeMember<RenderPart> > parts;
    copyToVector(m_parts, parts);

    // Script or plugins could detach the frame so abort processing if that happens.

    for (size_t i = 0; i < parts.size() && renderView(); ++i)
        parts[i]->updateWidgetPosition();

    for (size_t i = 0; i < parts.size() && renderView(); ++i)
        parts[i]->widgetPositionsUpdated();
}

void FrameView::addPartToUpdate(RenderEmbeddedObject& object)
{
    ASSERT(isInPerformLayout());
    // Tell the DOM element that it needs a widget update.
    Node* node = object.node();
    ASSERT(node);
    if (isHTMLObjectElement(*node) || isHTMLEmbedElement(*node))
        toHTMLPlugInElement(node)->setNeedsWidgetUpdate(true);

    m_partUpdateSet.add(&object);
}

void FrameView::setMediaType(const AtomicString& mediaType)
{
    ASSERT(m_frame->document());
    m_frame->document()->mediaQueryAffectingValueChanged();
    m_mediaType = mediaType;
}

AtomicString FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType;
    InspectorInstrumentation::applyEmulatedMedia(m_frame.get(), &overrideType);
    if (!overrideType.isNull())
        return AtomicString(overrideType);
    return m_mediaType;
}

void FrameView::adjustMediaTypeForPrinting(bool printing)
{
    if (printing) {
        if (m_mediaTypeWhenNotPrinting.isNull())
            m_mediaTypeWhenNotPrinting = mediaType();
            setMediaType(MediaTypeNames::print);
    } else {
        if (!m_mediaTypeWhenNotPrinting.isNull())
            setMediaType(m_mediaTypeWhenNotPrinting);
        m_mediaTypeWhenNotPrinting = nullAtom;
    }
}

bool FrameView::contentsInCompositedLayer() const
{
    RenderView* renderView = this->renderView();
    return renderView && renderView->compositingState() == PaintsIntoOwnBacking;
}

void FrameView::addSlowRepaintObject()
{
    if (!m_slowRepaintObjectCount++) {
        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(m_slowRepaintObjectCount > 0);
    m_slowRepaintObjectCount--;
    if (!m_slowRepaintObjectCount) {
        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewHasSlowRepaintObjectsDidChange(this);
        }
    }
}

void FrameView::addViewportConstrainedObject(RenderObject* object)
{
    if (!m_viewportConstrainedObjects)
        m_viewportConstrainedObjects = adoptPtr(new ViewportConstrainedObjectSet);

    if (!m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->add(object);

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
        }
    }
}

void FrameView::removeViewportConstrainedObject(RenderObject* object)
{
    if (m_viewportConstrainedObjects && m_viewportConstrainedObjects->contains(object)) {
        m_viewportConstrainedObjects->remove(object);

        if (Page* page = m_frame->page()) {
            if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewFixedObjectsDidChange(this);
        }
    }
}

LayoutRect FrameView::viewportConstrainedVisibleContentRect() const
{
    LayoutRect viewportRect = visibleContentRect();
    // Ignore overhang. No-op when not using rubber banding.
    viewportRect.setLocation(clampScrollPosition(scrollPosition()));
    return viewportRect;
}

void FrameView::viewportConstrainedVisibleContentSizeChanged(bool widthChanged, bool heightChanged)
{
    if (!hasViewportConstrainedObjects())
        return;

    // If viewport is not enabled, frameRect change will cause layout size change and then layout.
    // Otherwise, viewport constrained objects need their layout flags set separately to ensure
    // they are positioned correctly. In the virtual-viewport pinch mode frame rect changes wont
    // necessarily cause a layout size change so only take this early-out if we're in old-style
    // pinch.
    if (m_frame->settings()
        && !m_frame->settings()->viewportEnabled()
        && !m_frame->settings()->pinchVirtualViewportEnabled())
        return;

    for (const auto& viewportConstrainedObject : *m_viewportConstrainedObjects) {
        RenderObject* renderer = viewportConstrainedObject;
        RenderStyle* style = renderer->style();
        if (widthChanged) {
            if (style->width().isFixed() && (style->left().isAuto() || style->right().isAuto()))
                renderer->setNeedsPositionedMovementLayout();
            else
                renderer->setNeedsLayoutAndFullPaintInvalidation();
        }
        if (heightChanged) {
            if (style->height().isFixed() && (style->top().isAuto() || style->bottom().isAuto()))
                renderer->setNeedsPositionedMovementLayout();
            else
                renderer->setNeedsLayoutAndFullPaintInvalidation();
        }
    }
}

IntSize FrameView::scrollOffsetForFixedPosition() const
{
    return toIntSize(clampScrollPosition(scrollPosition()));
}

IntPoint FrameView::lastKnownMousePosition() const
{
    return m_frame->eventHandler().lastKnownMousePosition();
}

bool FrameView::shouldSetCursor() const
{
    Page* page = frame().page();
    return page && page->visibilityState() != PageVisibilityStateHidden && page->focusController().isActive() && page->settings().deviceSupportsMouse();
}

void FrameView::scrollContentsIfNeededRecursive()
{
    scrollContentsIfNeeded();

    for (Frame* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->isLocalFrame())
            continue;
        if (FrameView* view = toLocalFrame(child)->view())
            view->scrollContentsIfNeededRecursive();
    }
}

// FIXME: If we had a flag to force invalidations in a whole subtree, we could get rid of this function (crbug.com/410097).
static void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants(const RenderLayer* layer)
{
    layer->renderer()->setShouldDoFullPaintInvalidation();

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling()) {
        // Don't include paint invalidation rects for composited child layers; they will paint themselves and have a different origin.
        if (child->isPaintInvalidationContainer())
            continue;

        setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants(child);
    }
}

bool FrameView::scrollContentsFastPath(const IntSize& scrollDelta)
{
    if (!contentsInCompositedLayer() || hasSlowRepaintObjects())
        return false;

    if (!m_viewportConstrainedObjects || m_viewportConstrainedObjects->isEmpty()) {
        InspectorInstrumentation::didScroll(page());
        return true;
    }

    for (const auto& viewportConstrainedObject : *m_viewportConstrainedObjects) {
        RenderObject* renderer = viewportConstrainedObject;
        ASSERT(renderer->style()->hasViewportConstrainedPosition());
        ASSERT(renderer->hasLayer());
        RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();

        if (layer->isPaintInvalidationContainer())
            continue;

        if (layer->subtreeIsInvisible())
            continue;

        // If the fixed layer has a blur/drop-shadow filter applied on at least one of its parents, we cannot
        // scroll using the fast path, otherwise the outsets of the filter will be moved around the page.
        if (layer->hasAncestorWithFilterOutsets())
            return false;

        setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants(layer);
    }

    InspectorInstrumentation::didScroll(page());
    return true;
}

void FrameView::scrollContentsSlowPath(const IntRect& updateRect)
{
    if (contentsInCompositedLayer()) {
        IntRect updateRect = visibleContentRect();
        ASSERT(renderView());
        renderView()->invalidatePaintRectangle(updateRect);
    }
    if (RenderPart* frameRenderer = m_frame->ownerRenderer()) {
        if (isEnclosedInCompositingLayer()) {
            LayoutRect rect(frameRenderer->borderLeft() + frameRenderer->paddingLeft(),
                            frameRenderer->borderTop() + frameRenderer->paddingTop(),
                            visibleWidth(), visibleHeight());
            frameRenderer->invalidatePaintRectangle(rect);
            return;
        }
    }

    hostWindow()->invalidateContentsForSlowScroll(updateRect);
}

void FrameView::restoreScrollbar()
{
    setScrollbarsSuppressed(false);
}

bool FrameView::scrollToFragment(const KURL& url)
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH If CSS target was set previously, we want to set it to 0, recalc
    // and possibly paint invalidation because :target pseudo class may have been
    // set (see bug 11321).
    if (!url.hasFragmentIdentifier() && !m_frame->document()->cssTarget())
        return false;

    String fragmentIdentifier = url.fragmentIdentifier();
    if (scrollToAnchor(fragmentIdentifier))
        return true;

    // Try again after decoding the ref, based on the document's encoding.
    if (m_frame->document()->encoding().isValid())
        return scrollToAnchor(decodeURLEscapeSequences(fragmentIdentifier, m_frame->document()->encoding()));

    return false;
}

bool FrameView::scrollToAnchor(const String& name)
{
    ASSERT(m_frame->document());

    if (!m_frame->document()->isRenderingReady()) {
        m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorNode = m_frame->document()->findAnchor(name);

    // Setting to null will clear the current target.
    m_frame->document()->setCSSTarget(anchorNode);

    if (m_frame->document()->isSVGDocument()) {
        if (SVGSVGElement* svg = SVGDocumentExtensions::rootElement(*m_frame->document())) {
            svg->setupInitialView(name, anchorNode);
            if (!anchorNode)
                return true;
        }
    }

    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    maintainScrollPositionAtAnchor(anchorNode ? static_cast<Node*>(anchorNode) : m_frame->document());

    // If the anchor accepts keyboard focus, move focus there to aid users relying on keyboard navigation.
    // If anchorNode is not focusable, setFocusedElement() will still clear focus, which matches the behavior of other browsers.
    if (anchorNode)
        m_frame->document()->setFocusedElement(anchorNode);

    return true;
}

void FrameView::maintainScrollPositionAtAnchor(Node* anchorNode)
{
    m_maintainScrollPositionAnchor = anchorNode;
    if (!m_maintainScrollPositionAnchor)
        return;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    m_frame->document()->updateRenderTreeIfNeeded();
    // Only do a layout if changes have occurred that make it necessary.
    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout())
        layout();
    else
        scrollToAnchor();
}

void FrameView::scrollElementToRect(Element* element, const IntRect& rect)
{
    // FIXME(http://crbug.com/371896) - This method shouldn't be manually doing
    // coordinate transformations to the PinchViewport.
    IntRect targetRect(rect);

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    bool pinchVirtualViewportEnabled = m_frame->settings()->pinchVirtualViewportEnabled();

    // FIXME: This needs an isMainFrame() check as well. crbug.com/378776.
    if (pinchVirtualViewportEnabled) {
        PinchViewport& pinchViewport = m_frame->page()->frameHost().pinchViewport();

        IntSize pinchViewportSize = expandedIntSize(pinchViewport.visibleRect().size());
        targetRect.moveBy(ceiledIntPoint(pinchViewport.visibleRect().location()));
        targetRect.setSize(pinchViewportSize.shrunkTo(targetRect.size()));
    }

    LayoutRect bounds = element->boundingBox();
    int centeringOffsetX = (targetRect.width() - bounds.width()) / 2;
    int centeringOffsetY = (targetRect.height() - bounds.height()) / 2;

    IntPoint targetOffset(
        bounds.x() - centeringOffsetX - targetRect.x(),
        bounds.y() - centeringOffsetY - targetRect.y());

    setScrollPosition(DoublePoint(targetOffset));

    if (pinchVirtualViewportEnabled) {
        IntPoint remainder = IntPoint(targetOffset - scrollPosition());
        m_frame->page()->frameHost().pinchViewport().move(remainder);
    }
}

void FrameView::setScrollPosition(const DoublePoint& scrollPoint, ScrollBehavior scrollBehavior)
{
    cancelProgrammaticScrollAnimation();
    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, true);
    m_maintainScrollPositionAnchor = nullptr;

    DoublePoint newScrollPosition = adjustScrollPositionWithinRange(scrollPoint);
    if (newScrollPosition == scrollPositionDouble())
        return;

    if (scrollBehavior == ScrollBehaviorAuto) {
        RenderObject* renderer = m_frame->document()->documentElement() ? m_frame->document()->documentElement()->renderer() : 0;
        if (renderer)
            scrollBehavior = renderer->style()->scrollBehavior();
        else
            scrollBehavior = ScrollBehaviorInstant;
    }

    if (scrollBehavior == ScrollBehaviorInstant) {
        DoubleSize newOffset(newScrollPosition.x(), newScrollPosition.y());
        updateScrollbars(newOffset);
    } else {
        programmaticallyScrollSmoothlyToOffset(toFloatPoint(newScrollPosition));
    }
}

void FrameView::setScrollPositionNonProgrammatically(const IntPoint& scrollPoint)
{
    IntPoint newScrollPosition = adjustScrollPositionWithinRange(scrollPoint);

    if (newScrollPosition == scrollPosition())
        return;

    TemporaryChange<bool> changeInProgrammaticScroll(m_inProgrammaticScroll, false);
    notifyScrollPositionChanged(newScrollPosition);
}

IntSize FrameView::layoutSize(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return scrollbarInclusion == ExcludeScrollbars ? excludeScrollbars(m_layoutSize) : m_layoutSize;
}

void FrameView::setLayoutSize(const IntSize& size)
{
    ASSERT(!layoutSizeFixedToFrameSize());

    setLayoutSizeInternal(size);
}

void FrameView::scrollPositionChanged()
{
    setWasScrolledByUser(true);

    Document* document = m_frame->document();
    document->enqueueScrollEventForNode(document);

    m_frame->eventHandler().dispatchFakeMouseMoveEventSoon();

    if (RenderView* renderView = document->renderView()) {
        if (renderView->usesCompositing())
            renderView->compositor()->frameViewDidScroll();
    }

    if (m_didScrollTimer.isActive())
        m_didScrollTimer.stop();
    m_didScrollTimer.startOneShot(resourcePriorityUpdateDelayAfterScroll, FROM_HERE);

    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
        cache->handleScrollPositionChanged(this);

    frame().loader().saveScrollState();
}

void FrameView::didScrollTimerFired(Timer<FrameView>*)
{
    if (m_frame->document() && m_frame->document()->renderView()) {
        ResourceLoadPriorityOptimizer::resourceLoadPriorityOptimizer()->updateAllImageResourcePriorities();
    }
}

void FrameView::updateLayersAndCompositingAfterScrollIfNeeded()
{
    // Nothing to do after scrolling if there are no fixed position elements.
    if (!hasViewportConstrainedObjects())
        return;

    RefPtrWillBeRawPtr<FrameView> protect(this);

    // If there fixed position elements, scrolling may cause compositing layers to change.
    // Update widget and layer positions after scrolling, but only if we're not inside of
    // layout.
    if (!m_nestedLayoutCount) {
        updateWidgetPositions();
        if (RenderView* renderView = this->renderView())
            renderView->layer()->setNeedsCompositingInputsUpdate();
    }
}

bool FrameView::computeCompositedSelectionBounds(LocalFrame& frame, CompositedSelectionBound& start, CompositedSelectionBound& end)
{
    const VisibleSelection &selection = frame.selection().selection();
    if (!selection.isCaretOrRange())
        return false;

    VisiblePosition visibleStart(selection.visibleStart());
    VisiblePosition visibleEnd(selection.visibleEnd());

    RenderedPosition renderedStart(visibleStart);
    RenderedPosition renderedEnd(visibleEnd);

    renderedStart.positionInGraphicsLayerBacking(start);
    if (!start.layer)
        return false;

    renderedEnd.positionInGraphicsLayerBacking(end);
    if (!end.layer)
        return false;

    if (selection.isCaret()) {
        start.type = end.type = CompositedSelectionBound::Caret;
        return true;
    }

    TextDirection startDir = visibleStart.deepEquivalent().primaryDirection();
    TextDirection endDir = visibleEnd.deepEquivalent().primaryDirection();
    start.type = startDir == RTL ? CompositedSelectionBound::SelectionRight : CompositedSelectionBound::SelectionLeft;
    end.type = endDir == RTL ? CompositedSelectionBound::SelectionLeft : CompositedSelectionBound::SelectionRight;
    return true;
}

void FrameView::updateCompositedSelectionBoundsIfNeeded()
{
    if (!RuntimeEnabledFeatures::compositedSelectionUpdateEnabled())
        return;

    Page* page = frame().page();
    ASSERT(page);

    CompositedSelectionBound start, end;
    LocalFrame* frame = toLocalFrame(page->focusController().focusedOrMainFrame());
    if (!frame || !computeCompositedSelectionBounds(*frame, start, end)) {
        page->chrome().client().clearCompositedSelectionBounds();
        return;
    }

    page->chrome().client().updateCompositedSelectionBounds(start, end);
}

bool FrameView::isRubberBandInProgress() const
{
    if (scrollbarsSuppressed())
        return false;

    // If the main thread updates the scroll position for this FrameView, we should return
    // ScrollAnimator::isRubberBandInProgress().
    if (ScrollAnimator* scrollAnimator = existingScrollAnimator())
        return scrollAnimator->isRubberBandInProgress();

    return false;
}

HostWindow* FrameView::hostWindow() const
{
    Page* page = frame().page();
    if (!page)
        return 0;
    return &page->chrome();
}

void FrameView::contentRectangleForPaintInvalidation(const IntRect& rect)
{
    ASSERT(paintInvalidationIsAllowed());
    ASSERT(!m_frame->ownerRenderer());

    if (m_isTrackingPaintInvalidations) {
        IntRect paintInvalidationRect = rect;
        paintInvalidationRect.move(-scrollOffset());
        m_trackedPaintInvalidationRects.append(paintInvalidationRect);
        // FIXME: http://crbug.com/368518. Eventually, invalidateContentRectangleForPaint
        // is going away entirely once all layout tests are FCM. In the short
        // term, no code should be tracking non-composited FrameView paint invalidations.
        RELEASE_ASSERT_NOT_REACHED();
    }

    IntRect paintRect = rect;
    if (clipsPaintInvalidations())
        paintRect.intersect(visibleContentRect());
    if (paintRect.isEmpty())
        return;

    if (HostWindow* window = hostWindow())
        window->invalidateContentsAndRootView(contentsToWindow(paintRect));
}

void FrameView::contentsResized()
{
    if (m_frame->isMainFrame() && m_frame->document()) {
        if (TextAutosizer* textAutosizer = m_frame->document()->textAutosizer())
            textAutosizer->updatePageInfoInAllFrames();
    }

    ScrollableArea::contentsResized();
    setNeedsLayout();
}

void FrameView::scrollbarExistenceDidChange()
{
    // We check to make sure the view is attached to a frame() as this method can
    // be triggered before the view is attached by LocalFrame::createView(...) setting
    // various values such as setScrollBarModes(...) for example.  An ASSERT is
    // triggered when a view is layout before being attached to a frame().
    if (!frame().view())
        return;

    // Note that simply having overlay scrollbars is not sufficient to be
    // certain that scrollbars' presence does not impact layout. This should
    // also check if custom scrollbars (as reported by shouldUseCustomScrollbars)
    // are in use as well.
    // http://crbug.com/269692
    bool useOverlayScrollbars = ScrollbarTheme::theme()->usesOverlayScrollbars();

    // FIXME: this call to layout() could be called within FrameView::layout(), but before performLayout(),
    // causing double-layout. See also crbug.com/429242.
    if (!useOverlayScrollbars && needsLayout())
        layout();

    if (renderView() && renderView()->usesCompositing()) {
        renderView()->compositor()->frameViewScrollbarsExistenceDidChange();

        if (!useOverlayScrollbars)
            renderView()->compositor()->frameViewDidChangeSize();
    }
}

void FrameView::handleLoadCompleted()
{
    // Once loading has completed, allow autoSize one last opportunity to
    // reduce the size of the frame.
    if (m_autoSizeInfo)
        m_autoSizeInfo->autoSizeIfNeeded();
}

void FrameView::scheduleRelayout()
{
    ASSERT(m_frame->view() == this);

    if (isSubtreeLayout()) {
        m_layoutSubtreeRoot->markContainingBlocksForLayout(false);
        m_layoutSubtreeRoot = 0;
    }
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!m_frame->document()->shouldScheduleLayout())
        return;
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "InvalidateLayout", "frame", m_frame.get());
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didInvalidateLayout(m_frame.get());

    if (m_hasPendingLayout)
        return;
    m_hasPendingLayout = true;

    page()->animator().scheduleVisualUpdate();
    lifecycle().ensureStateAtMost(DocumentLifecycle::StyleClean);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* relayoutRoot)
{
    ASSERT(m_frame->view() == this);

    // FIXME: Should this call shouldScheduleLayout instead?
    if (!m_frame->document()->isActive())
        return;

    RenderView* renderView = this->renderView();
    if (renderView && renderView->needsLayout()) {
        if (relayoutRoot)
            relayoutRoot->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending() || !m_layoutSchedulingEnabled) {
        if (m_layoutSubtreeRoot != relayoutRoot) {
            if (isObjectAncestorContainerOf(m_layoutSubtreeRoot, relayoutRoot)) {
                // Keep the current root
                relayoutRoot->markContainingBlocksForLayout(false, m_layoutSubtreeRoot);
                ASSERT(!m_layoutSubtreeRoot->container() || !m_layoutSubtreeRoot->container()->needsLayout());
            } else if (isSubtreeLayout() && isObjectAncestorContainerOf(relayoutRoot, m_layoutSubtreeRoot)) {
                // Re-root at relayoutRoot
                m_layoutSubtreeRoot->markContainingBlocksForLayout(false, relayoutRoot);
                m_layoutSubtreeRoot = relayoutRoot;
                ASSERT(!m_layoutSubtreeRoot->container() || !m_layoutSubtreeRoot->container()->needsLayout());
            } else {
                // Just do a full relayout
                if (isSubtreeLayout())
                    m_layoutSubtreeRoot->markContainingBlocksForLayout(false);
                m_layoutSubtreeRoot = 0;
                relayoutRoot->markContainingBlocksForLayout(false);
            }
        }
    } else if (m_layoutSchedulingEnabled) {
        m_layoutSubtreeRoot = relayoutRoot;
        ASSERT(!m_layoutSubtreeRoot->container() || !m_layoutSubtreeRoot->container()->needsLayout());
        m_hasPendingLayout = true;

        page()->animator().scheduleVisualUpdate();
        lifecycle().ensureStateAtMost(DocumentLifecycle::StyleClean);
    }
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "InvalidateLayout", "frame", m_frame.get());
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
    // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
    InspectorInstrumentation::didInvalidateLayout(m_frame.get());
}

bool FrameView::layoutPending() const
{
    // FIXME: This should check Document::lifecycle instead.
    return m_hasPendingLayout;
}

bool FrameView::isInPerformLayout() const
{
    ASSERT(m_inPerformLayout == (lifecycle().state() == DocumentLifecycle::InPerformLayout));
    return m_inPerformLayout;
}

bool FrameView::needsLayout() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.

    RenderView* renderView = this->renderView();
    return layoutPending()
        || (renderView && renderView->needsLayout())
        || isSubtreeLayout();
}

void FrameView::setNeedsLayout()
{
    if (RenderView* renderView = this->renderView())
        renderView->setNeedsLayout();
}

bool FrameView::isTransparent() const
{
    return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;
    DisableCompositingQueryAsserts disabler;
    if (renderView() && renderView()->layer()->hasCompositedLayerMapping())
        renderView()->layer()->compositedLayerMapping()->updateContentsOpaque();
}

bool FrameView::hasOpaqueBackground() const
{
    return !m_isTransparent && !m_baseBackgroundColor.hasAlpha();
}

Color FrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(const Color& backgroundColor)
{
    m_baseBackgroundColor = backgroundColor;

    if (renderView() && renderView()->layer()->hasCompositedLayerMapping()) {
        CompositedLayerMapping* compositedLayerMapping = renderView()->layer()->compositedLayerMapping();
        compositedLayerMapping->updateContentsOpaque();
        if (compositedLayerMapping->mainGraphicsLayer())
            compositedLayerMapping->mainGraphicsLayer()->setNeedsDisplay();
    }
    recalculateScrollbarOverlayStyle();
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor, bool transparent)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree().traverseNext(m_frame.get())) {
        if (!frame->isLocalFrame())
            continue;
        if (FrameView* view = toLocalFrame(frame)->view()) {
            view->setTransparent(transparent);
            view->setBaseBackgroundColor(backgroundColor);
        }
    }
}

void FrameView::scrollToAnchor()
{
    RefPtrWillBeRawPtr<Node> anchorNode = m_maintainScrollPositionAnchor;
    if (!anchorNode)
        return;

    if (!anchorNode->renderer())
        return;

    LayoutRect rect;
    if (anchorNode != m_frame->document())
        rect = anchorNode->boundingBox();

    RefPtrWillBeRawPtr<LocalFrame> boundaryFrame = m_frame->document()->findUnsafeParentScrollPropagationBoundary();

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(false);

    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    anchorNode->renderer()->scrollRectToVisible(rect, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(true);

    if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
        cache->handleScrolledToAnchor(anchorNode.get());

    // scrollRectToVisible can call into setScrollPosition(), which resets m_maintainScrollPositionAnchor.
    m_maintainScrollPositionAnchor = anchorNode;
}

bool FrameView::updateWidgets()
{
    // This is always called from updateWidgetsTimerFired.
    // m_updateWidgetsTimer should only be scheduled if we have widgets to update.
    // Thus I believe we can stop checking isEmpty here, and just ASSERT isEmpty:
    // FIXME: This assert has been temporarily removed due to https://crbug.com/430344
    if (m_nestedLayoutCount > 1 || m_partUpdateSet.isEmpty())
        return true;

    // Need to swap because script will run inside the below loop and invalidate the iterator.
    EmbeddedObjectSet objects;
    objects.swap(m_partUpdateSet);

    for (const auto& embeddedObject : objects) {
        RenderEmbeddedObject& object = *embeddedObject;
        HTMLPlugInElement* element = toHTMLPlugInElement(object.node());

        // The object may have already been destroyed (thus node cleared),
        // but FrameView holds a manual ref, so it won't have been deleted.
        if (!element)
            continue;

        // No need to update if it's already crashed or known to be missing.
        if (object.showsUnavailablePluginIndicator())
            continue;

        if (element->needsWidgetUpdate())
            element->updateWidget();
        object.updateWidgetPosition();

        // Prevent plugins from causing infinite updates of themselves.
        // FIXME: Do we really need to prevent this?
        m_partUpdateSet.remove(&object);
    }

    return m_partUpdateSet.isEmpty();
}

void FrameView::updateWidgetsTimerFired(Timer<FrameView>*)
{
    ASSERT(!isInPerformLayout());
    RefPtrWillBeRawPtr<FrameView> protect(this);
    m_updateWidgetsTimer.stop();
    for (unsigned i = 0; i < maxUpdateWidgetsIterations; ++i) {
        if (updateWidgets())
            return;
    }
}

void FrameView::flushAnyPendingPostLayoutTasks()
{
    ASSERT(!isInPerformLayout());
    if (m_postLayoutTasksTimer.isActive())
        performPostLayoutTasks();
    if (m_updateWidgetsTimer.isActive())
        updateWidgetsTimerFired(0);
}

void FrameView::scheduleUpdateWidgetsIfNecessary()
{
    ASSERT(!isInPerformLayout());
    if (m_updateWidgetsTimer.isActive() || m_partUpdateSet.isEmpty())
        return;
    m_updateWidgetsTimer.startOneShot(0, FROM_HERE);
}

void FrameView::performPostLayoutTasks()
{
    // FIXME: We can reach here, even when the page is not active!
    // http/tests/inspector/elements/html-link-import.html and many other
    // tests hit that case.
    // We should ASSERT(isActive()); or at least return early if we can!
    ASSERT(!isInPerformLayout()); // Always before or after performLayout(), part of the highest-level layout() call.
    TRACE_EVENT0("blink", "FrameView::performPostLayoutTasks");
    RefPtrWillBeRawPtr<FrameView> protect(this);

    m_postLayoutTasksTimer.stop();

    m_frame->selection().setCaretRectNeedsUpdate();

    {
        // Hits in compositing/overflow/do-not-repaint-if-scrolling-composited-layers.html
        DisableCompositingQueryAsserts disabler;
        m_frame->selection().updateAppearance();
    }

    ASSERT(m_frame->document());
    if (m_nestedLayoutCount <= 1) {
        if (m_firstLayoutCallbackPending)
            m_firstLayoutCallbackPending = false;

        // Ensure that we always send this eventually.
        if (!m_frame->document()->parsing() && m_frame->loader().stateMachine()->committedFirstRealDocumentLoad())
            m_isVisuallyNonEmpty = true;

        // If the layout was done with pending sheets, we are not in fact visually non-empty yet.
        if (m_isVisuallyNonEmpty && !m_frame->document()->didLayoutWithPendingStylesheets() && m_firstVisuallyNonEmptyLayoutCallbackPending) {
            m_firstVisuallyNonEmptyLayoutCallbackPending = false;
            // FIXME: This callback is probably not needed, but is currently used
            // by android for setting the background color.
            m_frame->loader().client()->dispatchDidFirstVisuallyNonEmptyLayout();
        }
    }

    FontFaceSet::didLayout(*m_frame->document());

    updateWidgetPositions();

    // Plugins could have torn down the page inside updateWidgetPositions().
    if (!renderView())
        return;

    scheduleUpdateWidgetsIfNecessary();

    if (Page* page = m_frame->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->notifyLayoutUpdated();
    }

    scrollToAnchor();

    sendResizeEventIfNeeded();
}

bool FrameView::wasViewportResized()
{
    ASSERT(m_frame);
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;
    ASSERT(renderView->style());
    return (layoutSize(IncludeScrollbars) != m_lastViewportSize || renderView->style()->zoom() != m_lastZoomFactor);
}

void FrameView::sendResizeEventIfNeeded()
{
    ASSERT(m_frame);

    RenderView* renderView = this->renderView();
    if (!renderView || renderView->document().printing())
        return;

    if (!wasViewportResized())
        return;

    m_lastViewportSize = layoutSize(IncludeScrollbars);
    m_lastZoomFactor = renderView->style()->zoom();

    m_frame->document()->enqueueResizeEvent();

    if (m_frame->isMainFrame())
        InspectorInstrumentation::didResizeMainFrame(m_frame->page());
}

void FrameView::postLayoutTimerFired(Timer<FrameView>*)
{
    performPostLayoutTasks();
}

void FrameView::updateCounters()
{
    RenderView* view = renderView();
    if (!view->hasRenderCounters())
        return;

    for (RenderObject* renderer = view; renderer; renderer = renderer->nextInPreOrder()) {
        if (!renderer->isCounter())
            continue;

        toRenderCounter(renderer)->updateCounter();
    }
}

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (!m_viewportRenderer)
        return;

    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        return;
    }

    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);

    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;

        RefPtrWillBeRawPtr<OverflowEvent> event = OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow, verticalOverflowChanged, verticalOverflow);
        event->setTarget(m_viewportRenderer->node());
        m_frame->document()->enqueueAnimationFrameEvent(event.release());
    }

}

IntRect FrameView::windowClipRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    ASSERT(m_frame->view() == this);

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(scrollbarInclusion));
    if (!m_frame->deprecatedLocalOwner())
        return clipRect;

    // Take our owner element and get its clip rect.
    // FIXME: Do we need to do this for remote frames?
    HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
    FrameView* parentView = ownerElement->document().view();
    if (parentView)
        clipRect.intersect(parentView->windowClipRectForFrameOwner(ownerElement));
    return clipRect;
}

IntRect FrameView::windowClipRectForFrameOwner(const HTMLFrameOwnerElement* ownerElement) const
{
    // The renderer can sometimes be null when style="display:none" interacts
    // with external content and plugins.
    if (!ownerElement->renderer())
        return windowClipRect();

    // If we have no layer, just return our window clip rect.
    const RenderLayer* enclosingLayer = ownerElement->renderer()->enclosingLayer();
    if (!enclosingLayer)
        return windowClipRect();

    // FIXME: childrenClipRect relies on compositingState, which is not necessarily up to date.
    // https://code.google.com/p/chromium/issues/detail?id=343769
    DisableCompositingQueryAsserts disabler;

    // Apply the clip from the layer.
    IntRect clipRect = contentsToWindow(pixelSnappedIntRect(enclosingLayer->clipper().childrenClipRect()));
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame().page();
    return page && page->focusController().isActive();
}

void FrameView::scrollTo(const DoublePoint& newPosition)
{
    DoublePoint oldPosition = m_scrollPosition;
    DoubleSize scrollDelta = newPosition - oldPosition;
    if (scrollDelta.isZero())
        return;

    if (m_frame->settings() && m_frame->settings()->rootLayerScrolls()) {
        // Don't scroll the FrameView!
        ASSERT_NOT_REACHED();
    }

    m_scrollPosition = newPosition;

    if (!scrollbarsSuppressed())
        m_pendingScrollDelta += scrollDelta;

    updateLayersAndCompositingAfterScrollIfNeeded();
    scrollPositionChanged();
    frame().loader().client()->didChangeScrollOffset();
}

void FrameView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.moveBy(scrollbar->location());

    if (isInPerformLayout())
        addScrollbarDamage(scrollbar, rect);
    else
        invalidateRect(dirtyRect);
}

void FrameView::getTickmarks(Vector<IntRect>& tickmarks) const
{
    if (!m_tickmarks.isEmpty())
        tickmarks = m_tickmarks;
    else
        tickmarks = frame().document()->markers().renderedRectsForMarkers(DocumentMarker::TextMatch);
}

IntRect FrameView::windowResizerRect() const
{
    Page* page = frame().page();
    if (!page)
        return IntRect();
    return page->chrome().windowResizerRect();
}

void FrameView::setVisibleContentScaleFactor(float visibleContentScaleFactor)
{
    if (m_visibleContentScaleFactor == visibleContentScaleFactor)
        return;

    m_visibleContentScaleFactor = visibleContentScaleFactor;
    updateScrollbars(scrollOffsetDouble());
}

void FrameView::setInputEventsTransformForEmulation(const IntSize& offset, float contentScaleFactor)
{
    m_inputEventsOffsetForEmulation = offset;
    m_inputEventsScaleFactorForEmulation = contentScaleFactor;
}

IntSize FrameView::inputEventsOffsetForEmulation() const
{
    return m_inputEventsOffsetForEmulation;
}

float FrameView::inputEventsScaleFactor() const
{
    float pageScale = m_frame->settings()->pinchVirtualViewportEnabled()
        ? m_frame->page()->frameHost().pinchViewport().scale()
        : visibleContentScaleFactor();
    return pageScale * m_inputEventsScaleFactorForEmulation;
}

bool FrameView::scrollbarsCanBeActive() const
{
    if (m_frame->view() != this)
        return false;

    return !!m_frame->document();
}

IntRect FrameView::scrollableAreaBoundingBox() const
{
    RenderPart* ownerRenderer = frame().ownerRenderer();
    if (!ownerRenderer)
        return frameRect();

    return ownerRenderer->absoluteContentQuad().enclosingBoundingBox();
}


bool FrameView::isScrollable()
{
    return scrollingReasons() == Scrollable;
}

FrameView::ScrollingReasons FrameView::scrollingReasons()
{
    // Check for:
    // 1) If there an actual overflow.
    // 2) display:none or visibility:hidden set to self or inherited.
    // 3) overflow{-x,-y}: hidden;
    // 4) scrolling: no;

    // Covers #1
    IntSize contentsSize = this->contentsSize();
    IntSize visibleContentSize = visibleContentRect().size();
    if ((contentsSize.height() <= visibleContentSize.height() && contentsSize.width() <= visibleContentSize.width()))
        return NotScrollableNoOverflow;

    // Covers #2.
    // FIXME: Do we need to fix this for OOPI?
    HTMLFrameOwnerElement* owner = m_frame->deprecatedLocalOwner();
    if (owner && (!owner->renderer() || !owner->renderer()->visibleToHitTesting()))
        return NotScrollableNotVisible;

    // Cover #3 and #4.
    ScrollbarMode horizontalMode;
    ScrollbarMode verticalMode;
    calculateScrollbarModesForLayoutAndSetViewportRenderer(horizontalMode, verticalMode, RulesFromWebContentOnly);
    if (horizontalMode == ScrollbarAlwaysOff && verticalMode == ScrollbarAlwaysOff)
        return NotScrollableExplicitlyDisabled;

    return Scrollable;
}

void FrameView::updateScrollableAreaSet()
{
    // That ensures that only inner frames are cached.
    FrameView* parentFrameView = this->parentFrameView();
    if (!parentFrameView)
        return;

    if (!isScrollable()) {
        parentFrameView->removeScrollableArea(this);
        return;
    }

    parentFrameView->addScrollableArea(this);
}

bool FrameView::shouldSuspendScrollAnimations() const
{
    return m_frame->loader().state() != FrameStateComplete;
}

void FrameView::scrollbarStyleChanged()
{
    // FIXME: Why does this only apply to the main frame?
    if (!m_frame->isMainFrame())
        return;
    adjustScrollbarOpacity();
    contentsResized();
    updateScrollbars(scrollOffsetDouble());
    positionScrollbarLayers();
}

void FrameView::notifyPageThatContentAreaWillPaint() const
{
    Page* page = m_frame->page();
    if (!page)
        return;

    contentAreaWillPaint();

    if (!m_scrollableAreas)
        return;

    for (const auto& scrollableArea : *m_scrollableAreas) {
        if (!scrollableArea->scrollbarsCanBeActive())
            continue;

        scrollableArea->contentAreaWillPaint();
    }
}

bool FrameView::scrollAnimatorEnabled() const
{
    return m_frame->settings() && m_frame->settings()->scrollAnimatorEnabled();
}

void FrameView::updateAnnotatedRegions()
{
    Document* document = m_frame->document();
    if (!document->hasAnnotatedRegions())
        return;
    Vector<AnnotatedRegionValue> newRegions;
    document->renderBox()->collectAnnotatedRegions(newRegions);
    if (newRegions == document->annotatedRegions())
        return;
    document->setAnnotatedRegions(newRegions);
    if (Page* page = m_frame->page())
        page->chrome().client().annotatedRegionsChanged();
}

void FrameView::updateScrollCorner()
{
    RefPtr<RenderStyle> cornerStyle;
    IntRect cornerRect = scrollCornerRect();
    Document* doc = m_frame->document();

    if (doc && !cornerRect.isEmpty()) {
        // Try the <body> element first as a scroll corner source.
        if (Element* body = doc->body()) {
            if (RenderObject* renderer = body->renderer())
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
        }

        if (!cornerStyle) {
            // If the <body> didn't have a custom style, then the root element might.
            if (Element* docElement = doc->documentElement()) {
                if (RenderObject* renderer = docElement->renderer())
                    cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
            }
        }

        if (!cornerStyle) {
            // If we have an owning ipage/LocalFrame element, then it can set the custom scrollbar also.
            if (RenderPart* renderer = m_frame->ownerRenderer())
                cornerStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), renderer->style());
        }
    }

    if (cornerStyle) {
        if (!m_scrollCorner)
            m_scrollCorner = RenderScrollbarPart::createAnonymous(doc);
        m_scrollCorner->setStyle(cornerStyle.release());
        invalidateScrollCorner(cornerRect);
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = nullptr;
    }
}

Color FrameView::documentBackgroundColor() const
{
    // <https://bugs.webkit.org/show_bug.cgi?id=59540> We blend the background color of
    // the document and the body against the base background color of the frame view.
    // Background images are unfortunately impractical to include.

    Color result = baseBackgroundColor();
    if (!frame().document())
        return result;

    Element* htmlElement = frame().document()->documentElement();
    Element* bodyElement = frame().document()->body();

    // We take the aggregate of the base background color
    // the <html> background color, and the <body>
    // background color to find the document color. The
    // addition of the base background color is not
    // technically part of the document background, but it
    // otherwise poses problems when the aggregate is not
    // fully opaque.
    if (htmlElement && htmlElement->renderer())
        result = result.blend(htmlElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor));
    if (bodyElement && bodyElement->renderer())
        result = result.blend(bodyElement->renderer()->style()->visitedDependentColor(CSSPropertyBackgroundColor));

    return result;
}

bool FrameView::hasCustomScrollbars() const
{
    const ChildrenWidgetSet* viewChildren = children();
    for (const RefPtrWillBeMember<Widget>& child : *viewChildren) {
        Widget* widget = child.get();
        if (widget->isFrameView()) {
            if (toFrameView(widget)->hasCustomScrollbars())
                return true;
        } else if (widget->isScrollbar()) {
            if (toScrollbar(widget)->isCustomScrollbar())
                return true;
        }
    }

    return false;
}

FrameView* FrameView::parentFrameView() const
{
    if (!parent())
        return 0;

    Frame* parentFrame = m_frame->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame())
        return toLocalFrame(parentFrame)->view();

    return 0;
}

bool FrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (m_inProgrammaticScroll)
        return;
    m_maintainScrollPositionAnchor = nullptr;
    m_wasScrolledByUser = wasScrolledByUser;
}

void FrameView::setPaintBehavior(PaintBehavior behavior)
{
    m_paintBehavior = behavior;
}

PaintBehavior FrameView::paintBehavior() const
{
    return m_paintBehavior;
}

bool FrameView::isPainting() const
{
    return m_isPainting;
}

void FrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void FrameView::updateWidgetPositionsIfNeeded()
{
    if (!m_needsUpdateWidgetPositions)
        return;

    m_needsUpdateWidgetPositions = false;

    updateWidgetPositions();
}

void FrameView::updateLayoutAndStyleForPainting()
{
    // Updating layout can run script, which can tear down the FrameView.
    RefPtrWillBeRawPtr<FrameView> protector(this);

    updateLayoutAndStyleIfNeededRecursive();

    updateWidgetPositionsIfNeeded();

    RenderView* view = renderView();
    if (view) {
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateLayerTree", "frame", m_frame.get());
        // FIXME(361045): remove InspectorInstrumentation calls once DevTools Timeline migrates to tracing.
        InspectorInstrumentation::willUpdateLayerTree(m_frame.get());

        view->compositor()->updateIfNeededRecursive();

        if (view->compositor()->inCompositingMode() && m_frame->isLocalRoot())
            m_frame->page()->scrollingCoordinator()->updateAfterCompositingChangeIfNeeded();

        updateCompositedSelectionBoundsIfNeeded();

        InspectorInstrumentation::didUpdateLayerTree(m_frame.get());
    }

    scrollContentsIfNeededRecursive();

    if (view)
        invalidateTreeIfNeededRecursive();

    ASSERT(lifecycle().state() == DocumentLifecycle::PaintInvalidationClean);
}

void FrameView::updateLayoutAndStyleIfNeededRecursive()
{
    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.

    m_frame->document()->updateRenderTreeIfNeeded();

    if (needsLayout())
        layout();

    // FIXME: Calling layout() shouldn't trigger scripe execution or have any
    // observable effects on the frame tree but we're not quite there yet.
    WillBeHeapVector<RefPtrWillBeMember<FrameView> > frameViews;
    for (Frame* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->isLocalFrame())
            continue;
        if (FrameView* view = toLocalFrame(child)->view())
            frameViews.append(view);
    }

    for (const auto& frameView : frameViews)
        frameView->updateLayoutAndStyleIfNeededRecursive();

    // When an <iframe> gets composited, it triggers an extra style recalc in its containing FrameView.
    // To avoid pushing an invalid tree for display, we have to check for this case and do another
    // style recalc. The extra style recalc needs to happen after our child <iframes> were updated.
    // FIXME: We shouldn't be triggering an extra style recalc in the first place.
    if (m_frame->document()->hasSVGFilterElementsRequiringLayerUpdate()) {
        m_frame->document()->updateRenderTreeIfNeeded();

        if (needsLayout())
            layout();
    }

    // These asserts ensure that parent frames are clean, when child frames finished updating layout and style.
    ASSERT(!needsLayout());
    ASSERT(!m_frame->document()->hasSVGFilterElementsRequiringLayerUpdate());
#if ENABLE(ASSERT)
    m_frame->document()->renderView()->assertRendererLaidOut();
#endif

}

void FrameView::invalidateTreeIfNeededRecursive()
{
    // FIXME: We should be more aggressive at cutting tree traversals.
    lifecycle().advanceTo(DocumentLifecycle::InPaintInvalidation);
    invalidateTreeIfNeeded();
    lifecycle().advanceTo(DocumentLifecycle::PaintInvalidationClean);

    for (Frame* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->isLocalFrame())
            continue;

        toLocalFrame(child)->view()->invalidateTreeIfNeededRecursive();
    }

    m_doFullPaintInvalidation = false;
}

void FrameView::enableAutoSizeMode(const IntSize& minSize, const IntSize& maxSize)
{
    if (!m_autoSizeInfo)
        m_autoSizeInfo = FrameViewAutoSizeInfo::create(this);

    m_autoSizeInfo->configureAutoSizeMode(minSize, maxSize);
    setLayoutSizeFixedToFrameSize(true);
    setNeedsLayout();
    scheduleRelayout();
}

void FrameView::disposeAutoSizeInfo()
{
    if (!m_autoSizeInfo)
        return;

    setLayoutSizeFixedToFrameSize(false);
    setNeedsLayout();
    scheduleRelayout();

    // Since autosize mode forces the scrollbar mode, change them to being auto.
    setVerticalScrollbarLock(false);
    setHorizontalScrollbarLock(false);
    setScrollbarModes(ScrollbarAuto, ScrollbarAuto);
    m_autoSizeInfo.clear();
}

void FrameView::forceLayout(bool allowSubtree)
{
    layout(allowSubtree);
}

void FrameView::forceLayoutForPagination(const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkFactor)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    if (RenderView* renderView = this->renderView()) {
        float pageLogicalWidth = renderView->style()->isHorizontalWritingMode() ? pageSize.width() : pageSize.height();
        float pageLogicalHeight = renderView->style()->isHorizontalWritingMode() ? pageSize.height() : pageSize.width();

        LayoutUnit flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
        LayoutUnit flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
        renderView->setLogicalWidth(flooredPageLogicalWidth);
        renderView->setPageLogicalHeight(flooredPageLogicalHeight);
        renderView->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        forceLayout();

        // If we don't fit in the given page width, we'll lay out again. If we don't fit in the
        // page width when shrunk, we will lay out at maximum shrink and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        bool horizontalWritingMode = renderView->style()->isHorizontalWritingMode();
        const LayoutRect& documentRect = renderView->documentRect();
        LayoutUnit docLogicalWidth = horizontalWritingMode ? documentRect.width() : documentRect.height();
        if (docLogicalWidth > pageLogicalWidth) {
            FloatSize expectedPageSize(std::min<float>(documentRect.width().toFloat(), pageSize.width() * maximumShrinkFactor), std::min<float>(documentRect.height().toFloat(), pageSize.height() * maximumShrinkFactor));
            FloatSize maxPageSize = m_frame->resizePageRectsKeepingRatio(FloatSize(originalPageSize.width(), originalPageSize.height()), expectedPageSize);
            pageLogicalWidth = horizontalWritingMode ? maxPageSize.width() : maxPageSize.height();
            pageLogicalHeight = horizontalWritingMode ? maxPageSize.height() : maxPageSize.width();

            flooredPageLogicalWidth = static_cast<LayoutUnit>(pageLogicalWidth);
            flooredPageLogicalHeight = static_cast<LayoutUnit>(pageLogicalHeight);
            renderView->setLogicalWidth(flooredPageLogicalWidth);
            renderView->setPageLogicalHeight(flooredPageLogicalHeight);
            renderView->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
            forceLayout();

            const LayoutRect& updatedDocumentRect = renderView->documentRect();
            LayoutUnit docLogicalHeight = horizontalWritingMode ? updatedDocumentRect.height() : updatedDocumentRect.width();
            LayoutUnit docLogicalTop = horizontalWritingMode ? updatedDocumentRect.y() : updatedDocumentRect.x();
            LayoutUnit docLogicalRight = horizontalWritingMode ? updatedDocumentRect.maxX() : updatedDocumentRect.maxY();
            LayoutUnit clippedLogicalLeft = 0;
            if (!renderView->style()->isLeftToRightDirection())
                clippedLogicalLeft = docLogicalRight - pageLogicalWidth;
            LayoutRect overflow(clippedLogicalLeft, docLogicalTop, pageLogicalWidth, docLogicalHeight);

            if (!horizontalWritingMode)
                overflow = overflow.transposedRect();
            renderView->clearLayoutOverflow();
            renderView->addLayoutOverflow(overflow); // This is how we clip in case we overflow again.
        }
    }

    adjustViewSize();
}

IntRect FrameView::convertFromRenderer(const RenderObject& renderer, const IntRect& rendererRect) const
{
    IntRect rect = pixelSnappedIntRect(enclosingLayoutRect(renderer.localToAbsoluteQuad(FloatRect(rendererRect)).boundingBox()));

    // Convert from page ("absolute") to FrameView coordinates.
    rect.moveBy(-scrollPosition());

    return rect;
}

IntRect FrameView::convertToRenderer(const RenderObject& renderer, const IntRect& viewRect) const
{
    IntRect rect = viewRect;

    // Convert from FrameView coords into page ("absolute") coordinates.
    rect.moveBy(scrollPosition());

    // FIXME: we don't have a way to map an absolute rect down to a local quad, so just
    // move the rect for now.
    rect.setLocation(roundedIntPoint(renderer.absoluteToLocal(rect.location(), UseTransforms)));
    return rect;
}

IntPoint FrameView::convertFromRenderer(const RenderObject& renderer, const IntPoint& rendererPoint) const
{
    IntPoint point = roundedIntPoint(renderer.localToAbsolute(rendererPoint, UseTransforms));

    // Convert from page ("absolute") to FrameView coordinates.
    point.moveBy(-scrollPosition());
    return point;
}

IntPoint FrameView::convertToRenderer(const RenderObject& renderer, const IntPoint& viewPoint) const
{
    IntPoint point = viewPoint;

    // Convert from FrameView coords into page ("absolute") coordinates.
    point += IntSize(scrollX(), scrollY());

    return roundedIntPoint(renderer.absoluteToLocal(point, UseTransforms));
}

IntRect FrameView::convertToContainingView(const IntRect& localRect) const
{
    if (const FrameView* parentView = toFrameView(parent())) {
        // Get our renderer in the parent view
        RenderPart* renderer = m_frame->ownerRenderer();
        if (!renderer)
            return localRect;

        IntRect rect(localRect);
        // Add borders and padding??
        rect.move(renderer->borderLeft() + renderer->paddingLeft(),
            renderer->borderTop() + renderer->paddingTop());
        return parentView->convertFromRenderer(*renderer, rect);
    }

    return localRect;
}

IntRect FrameView::convertFromContainingView(const IntRect& parentRect) const
{
    if (const FrameView* parentView = toFrameView(parent())) {
        // Get our renderer in the parent view
        RenderPart* renderer = m_frame->ownerRenderer();
        if (!renderer)
            return parentRect;

        IntRect rect = parentView->convertToRenderer(*renderer, parentRect);
        // Subtract borders and padding
        rect.move(-renderer->borderLeft() - renderer->paddingLeft(),
            -renderer->borderTop() - renderer->paddingTop());
        return rect;
    }

    return parentRect;
}

IntPoint FrameView::convertToContainingView(const IntPoint& localPoint) const
{
    if (const FrameView* parentView = toFrameView(parent())) {
        // Get our renderer in the parent view
        RenderPart* renderer = m_frame->ownerRenderer();
        if (!renderer)
            return localPoint;

        IntPoint point(localPoint);

        // Add borders and padding
        point.move(renderer->borderLeft() + renderer->paddingLeft(),
            renderer->borderTop() + renderer->paddingTop());
        return parentView->convertFromRenderer(*renderer, point);
    }

    return localPoint;
}

IntPoint FrameView::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const FrameView* parentView = toFrameView(parent())) {
        // Get our renderer in the parent view
        RenderPart* renderer = m_frame->ownerRenderer();
        if (!renderer)
            return parentPoint;

        IntPoint point = parentView->convertToRenderer(*renderer, parentPoint);
        // Subtract borders and padding
        point.move(-renderer->borderLeft() - renderer->paddingLeft(),
            -renderer->borderTop() - renderer->paddingTop());
        return point;
    }

    return parentPoint;
}

void FrameView::setTracksPaintInvalidations(bool trackPaintInvalidations)
{
    if (trackPaintInvalidations == m_isTrackingPaintInvalidations)
        return;

    for (Frame* frame = m_frame->tree().top(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        if (RenderView* renderView = toLocalFrame(frame)->contentRenderer())
            renderView->compositor()->setTracksPaintInvalidations(trackPaintInvalidations);
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
        "FrameView::setTracksPaintInvalidations", "enabled", trackPaintInvalidations);

    resetTrackedPaintInvalidations();
    m_isTrackingPaintInvalidations = trackPaintInvalidations;
}

void FrameView::resetTrackedPaintInvalidations()
{
    m_trackedPaintInvalidationRects.clear();
    if (RenderView* renderView = this->renderView())
        renderView->compositor()->resetTrackedPaintInvalidationRects();
}

String FrameView::trackedPaintInvalidationRectsAsText() const
{
    TextStream ts;
    if (!m_trackedPaintInvalidationRects.isEmpty()) {
        ts << "(repaint rects\n";
        for (size_t i = 0; i < m_trackedPaintInvalidationRects.size(); ++i)
            ts << "  (rect " << m_trackedPaintInvalidationRects[i].x() << " " << m_trackedPaintInvalidationRects[i].y() << " " << m_trackedPaintInvalidationRects[i].width() << " " << m_trackedPaintInvalidationRects[i].height() << ")\n";
        ts << ")\n";
    }
    return ts.release();
}

void FrameView::addResizerArea(RenderBox& resizerBox)
{
    if (!m_resizerAreas)
        m_resizerAreas = adoptPtr(new ResizerAreaSet);
    m_resizerAreas->add(&resizerBox);
}

void FrameView::removeResizerArea(RenderBox& resizerBox)
{
    if (!m_resizerAreas)
        return;

    ResizerAreaSet::iterator it = m_resizerAreas->find(&resizerBox);
    if (it != m_resizerAreas->end())
        m_resizerAreas->remove(it);
}

void FrameView::addScrollableArea(ScrollableArea* scrollableArea)
{
    ASSERT(scrollableArea);
    if (!m_scrollableAreas)
        m_scrollableAreas = adoptPtr(new ScrollableAreaSet);
    m_scrollableAreas->add(scrollableArea);
}

void FrameView::removeScrollableArea(ScrollableArea* scrollableArea)
{
    if (!m_scrollableAreas)
        return;
    m_scrollableAreas->remove(scrollableArea);
}

void FrameView::setParent(Widget* parentView)
{
    if (parentView == parent())
        return;

    if (m_scrollbarsAvoidingResizer && parent())
        toFrameView(parent())->adjustScrollbarsAvoidingResizerCount(-m_scrollbarsAvoidingResizer);

    Widget::setParent(parentView);

    if (m_scrollbarsAvoidingResizer && parent())
        toFrameView(parent())->adjustScrollbarsAvoidingResizerCount(m_scrollbarsAvoidingResizer);

    updateScrollableAreaSet();
}

void FrameView::removeChild(Widget* child)
{
    ASSERT(child->parent() == this);

    if (child->isFrameView())
        removeScrollableArea(toFrameView(child));

    child->setParent(0);
    m_children.remove(child);
}

bool FrameView::wheelEvent(const PlatformWheelEvent& wheelEvent)
{
    bool allowScrolling = userInputScrollable(HorizontalScrollbar) || userInputScrollable(VerticalScrollbar);

    // Note that to allow for rubber-band over-scroll behavior, even non-scrollable views
    // should handle wheel events.
#if !USE(RUBBER_BANDING)
    if (!isScrollable())
        allowScrolling = false;
#endif

    if (m_frame->settings()->rootLayerScrolls())
        allowScrolling = false;

    if (allowScrolling && ScrollableArea::handleWheelEvent(wheelEvent))
        return true;

    // If the frame didn't handle the event, give the pinch-zoom viewport a chance to
    // process the scroll event.
    if (m_frame->settings()->pinchVirtualViewportEnabled() && m_frame->isMainFrame())
        return page()->frameHost().pinchViewport().handleWheelEvent(wheelEvent);

    return false;
}

bool FrameView::isVerticalDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return true;

    return renderView->style()->isHorizontalWritingMode();
}

bool FrameView::isFlippedDocument() const
{
    RenderView* renderView = this->renderView();
    if (!renderView)
        return false;

    return renderView->hasFlippedBlocksWritingMode();
}

bool FrameView::scrollbarsDisabled() const
{
    if (!m_frame->settings() || !m_frame->settings()->pinchVirtualViewportEnabled())
        return false;

    // FIXME: This decision should be made based on whether or not to use
    // viewport scrollbars for the main frame. This is implicitly just Android,
    // but should be made explicit.
    // http://crbug.com/434533
#if !OS(ANDROID)
    return false;
#else
    return m_frame->isMainFrame();
#endif
}

AXObjectCache* FrameView::axObjectCache() const
{
    if (frame().document())
        return frame().document()->existingAXObjectCache();
    return 0;
}

void FrameView::setCursor(const Cursor& cursor)
{
    Page* page = frame().page();
    if (!page || !page->settings().deviceSupportsMouse())
        return;
    page->chrome().setCursor(cursor);
}

void FrameView::frameRectsChanged()
{
    if (layoutSizeFixedToFrameSize())
        setLayoutSizeInternal(frameRect().size());

    for (const auto& child : m_children)
        child->frameRectsChanged();
}

void FrameView::setLayoutSizeInternal(const IntSize& size)
{
    if (m_layoutSize == size)
        return;

    m_layoutSize = size;
    contentsResized();
}

void FrameView::didAddScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::didAddScrollbar(scrollbar, orientation);
    if (AXObjectCache* cache = axObjectCache())
        cache->handleScrollbarUpdate(this);
}

void FrameView::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    ScrollableArea::willRemoveScrollbar(scrollbar, orientation);
    if (AXObjectCache* cache = axObjectCache()) {
        cache->remove(scrollbar);
        cache->handleScrollbarUpdate(this);
    }
}

void FrameView::setTopControlsViewportAdjustment(float adjustment)
{
#if !OS(ANDROID)
    // The adjustment is done only for top controls which are android only.
    ASSERT(!adjustment);
#endif
    m_topControlsViewportAdjustment = adjustment;
}

IntPoint FrameView::maximumScrollPosition() const
{
    // Make the same calculation as in CC's LayerImpl::MaxScrollOffset()
    // FIXME: We probably shouldn't be storing the bounds in a float. crbug.com/422331.
    FloatSize visibleSize = unscaledVisibleContentSize(ExcludeScrollbars);
    visibleSize.expand(0, m_topControlsViewportAdjustment);

    FloatSize contentBounds = contentsSize();
    contentBounds.scale(visibleContentScaleFactor());
    contentBounds = flooredIntSize(contentBounds);

    FloatSize maximumOffset = contentBounds - visibleSize - toIntSize(scrollOrigin());

    // Convert back to CSS pixels.
    maximumOffset.scale(1 / visibleContentScaleFactor());

    IntPoint snappedMaximumOffset = flooredIntPoint(maximumOffset);
    snappedMaximumOffset.clampNegativeToZero();
    return snappedMaximumOffset;
}

void FrameView::addChild(PassRefPtrWillBeRawPtr<Widget> prpChild)
{
    Widget* child = prpChild.get();
    ASSERT(child != this && !child->parent());
    child->setParent(this);
    m_children.add(prpChild);
}

void FrameView::setHasHorizontalScrollbar(bool hasBar)
{
    if (hasBar && !m_horizontalScrollbar) {
        m_horizontalScrollbar = createScrollbar(HorizontalScrollbar);
        addChild(m_horizontalScrollbar.get());
        didAddScrollbar(m_horizontalScrollbar.get(), HorizontalScrollbar);
        m_horizontalScrollbar->styleChanged();
    } else if (!hasBar && m_horizontalScrollbar) {
        willRemoveScrollbar(m_horizontalScrollbar.get(), HorizontalScrollbar);
        // If the scrollbar has been marked as overlapping the window resizer,
        // then its removal should reduce the count.
        if (m_horizontalScrollbar->overlapsResizer())
            adjustScrollbarsAvoidingResizerCount(-1);
        removeChild(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;
    }
}

void FrameView::setHasVerticalScrollbar(bool hasBar)
{
    if (hasBar && !m_verticalScrollbar) {
        m_verticalScrollbar = createScrollbar(VerticalScrollbar);
        addChild(m_verticalScrollbar.get());
        didAddScrollbar(m_verticalScrollbar.get(), VerticalScrollbar);
        m_verticalScrollbar->styleChanged();
    } else if (!hasBar && m_verticalScrollbar) {
        willRemoveScrollbar(m_verticalScrollbar.get(), VerticalScrollbar);
        // If the scrollbar has been marked as overlapping the window resizer,
        // then its removal should reduce the count.
        if (m_verticalScrollbar->overlapsResizer())
            adjustScrollbarsAvoidingResizerCount(-1);
        removeChild(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
    }
}

void FrameView::setScrollbarModes(ScrollbarMode horizontalMode, ScrollbarMode verticalMode,
    bool horizontalLock, bool verticalLock)
{
    bool needsUpdate = false;

    if (horizontalMode != horizontalScrollbarMode() && !m_horizontalScrollbarLock) {
        m_horizontalScrollbarMode = horizontalMode;
        needsUpdate = true;
    }

    if (verticalMode != verticalScrollbarMode() && !m_verticalScrollbarLock) {
        m_verticalScrollbarMode = verticalMode;
        needsUpdate = true;
    }

    if (horizontalLock)
        setHorizontalScrollbarLock();

    if (verticalLock)
        setVerticalScrollbarLock();

    if (!needsUpdate)
        return;

    updateScrollbars(scrollOffsetDouble());

    if (!layerForScrolling())
        return;
    blink::WebLayer* layer = layerForScrolling()->platformLayer();
    if (!layer)
        return;
    layer->setUserScrollable(userInputScrollable(HorizontalScrollbar), userInputScrollable(VerticalScrollbar));
}

void FrameView::scrollbarModes(ScrollbarMode& horizontalMode, ScrollbarMode& verticalMode) const
{
    horizontalMode = m_horizontalScrollbarMode;
    verticalMode = m_verticalScrollbarMode;
}

void FrameView::setClipsRepaints(bool clipsRepaints)
{
    m_clipsRepaints = clipsRepaints;
}

IntSize FrameView::unscaledVisibleContentSize(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return scrollbarInclusion == ExcludeScrollbars ? excludeScrollbars(frameRect().size()) : frameRect().size();
}

IntSize FrameView::excludeScrollbars(const IntSize& size) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;

    if (Scrollbar* verticalBar = verticalScrollbar())
        verticalScrollbarWidth = !verticalBar->isOverlayScrollbar() ? verticalBar->width() : 0;
    if (Scrollbar* horizontalBar = horizontalScrollbar())
        horizontalScrollbarHeight = !horizontalBar->isOverlayScrollbar() ? horizontalBar->height() : 0;

    return IntSize(std::max(0, size.width() - verticalScrollbarWidth),
        std::max(0, size.height() - horizontalScrollbarHeight));

}

IntRect FrameView::visibleContentRect(IncludeScrollbarsInRect scollbarInclusion) const
{
    FloatSize visibleContentSize = unscaledVisibleContentSize(scollbarInclusion);
    visibleContentSize.scale(1 / visibleContentScaleFactor());
    return IntRect(flooredIntPoint(m_scrollPosition), expandedIntSize(visibleContentSize));
}

IntRect FrameView::visualViewportRect() const
{
    if (!m_frame->isMainFrame()
        || !m_frame->settings()
        || !m_frame->settings()->pinchVirtualViewportEnabled())
        return visibleContentRect();

    PinchViewport& pinchViewport = m_frame->page()->frameHost().pinchViewport();
    return enclosingIntRect(pinchViewport.visibleRectInDocument());
}

IntSize FrameView::contentsSize() const
{
    return m_contentsSize;
}

IntPoint FrameView::minimumScrollPosition() const
{
    return IntPoint(-scrollOrigin().x(), -scrollOrigin().y());
}

IntPoint FrameView::adjustScrollPositionWithinRange(const IntPoint& scrollPoint) const
{
    if (!constrainsScrollingToContentEdge())
        return scrollPoint;

    IntPoint newScrollPosition = scrollPoint.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());
    return newScrollPosition;
}

DoublePoint FrameView::adjustScrollPositionWithinRange(const DoublePoint& scrollPoint) const
{
    if (!constrainsScrollingToContentEdge())
        return scrollPoint;
    DoublePoint newScrollPosition = scrollPoint.shrunkTo(
        maximumScrollPosition().x(), maximumScrollPosition().y());
    newScrollPosition = newScrollPosition.expandedTo(
        minimumScrollPosition().x(), minimumScrollPosition().y());
    return newScrollPosition;
}

void FrameView::adjustScrollbarOpacity()
{
    if (m_horizontalScrollbar && layerForHorizontalScrollbar()) {
        bool isOpaqueScrollbar = !m_horizontalScrollbar->isOverlayScrollbar();
        layerForHorizontalScrollbar()->setContentsOpaque(isOpaqueScrollbar);
    }
    if (m_verticalScrollbar && layerForVerticalScrollbar()) {
        bool isOpaqueScrollbar = !m_verticalScrollbar->isOverlayScrollbar();
        layerForVerticalScrollbar()->setContentsOpaque(isOpaqueScrollbar);
    }
}

int FrameView::scrollSize(ScrollbarOrientation orientation) const
{
    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_horizontalScrollbar : m_verticalScrollbar).get();

    // If no scrollbars are present, the content may still be scrollable.
    if (!scrollbar) {
        IntSize scrollSize = m_contentsSize - visibleContentRect().size();
        scrollSize.clampNegativeToZero();
        return orientation == HorizontalScrollbar ? scrollSize.width() : scrollSize.height();
    }

    return scrollbar->totalSize() - scrollbar->visibleSize();
}

void FrameView::setScrollOffset(const IntPoint& offset)
{
    scrollTo(DoublePoint(adjustScrollPositionWithinRange(offset)));
}

void FrameView::setScrollOffset(const DoublePoint& offset)
{
    scrollTo(adjustScrollPositionWithinRange(offset));
}

bool FrameView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    ScrollDirection physicalDirection =
        toPhysicalDirection(direction, isVerticalDocument(), isFlippedDocument());

    return ScrollableArea::scroll(physicalDirection, granularity);
}

IntSize FrameView::overhangAmount() const
{
    IntSize stretch;

    IntPoint currentScrollPosition = scrollPosition();
    IntPoint minScrollPosition = minimumScrollPosition();
    IntPoint maxScrollPosition = maximumScrollPosition();

    if (currentScrollPosition.x() < minScrollPosition.x())
        stretch.setWidth(currentScrollPosition.x() - minScrollPosition.x());
    if (currentScrollPosition.x() > maxScrollPosition.x())
        stretch.setWidth(currentScrollPosition.x() - maxScrollPosition.x());

    if (currentScrollPosition.y() < minScrollPosition.y())
        stretch.setHeight(currentScrollPosition.y() - minScrollPosition.y());
    if (currentScrollPosition.y() > maxScrollPosition.y())
        stretch.setHeight(currentScrollPosition.y() - maxScrollPosition.y());

    return stretch;
}

void FrameView::windowResizerRectChanged()
{
    updateScrollbars(scrollOffsetDouble());
}

static bool useOverlayScrollbars()
{
    // FIXME: Need to detect the presence of CSS custom scrollbars, which are non-overlay regardless the ScrollbarTheme.
    return ScrollbarTheme::theme()->usesOverlayScrollbars();
}

void FrameView::computeScrollbarExistence(bool& newHasHorizontalScrollbar, bool& newHasVerticalScrollbar, const IntSize& docSize, ComputeScrollbarExistenceOption option) const
{
    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool hasVerticalScrollbar = m_verticalScrollbar;

    newHasHorizontalScrollbar = hasHorizontalScrollbar;
    newHasVerticalScrollbar = hasVerticalScrollbar;

    if (m_frame->settings() && m_frame->settings()->rootLayerScrolls())
        return;

    ScrollbarMode hScroll = m_horizontalScrollbarMode;
    ScrollbarMode vScroll = m_verticalScrollbarMode;

    if (hScroll != ScrollbarAuto)
        newHasHorizontalScrollbar = (hScroll == ScrollbarAlwaysOn);
    if (vScroll != ScrollbarAuto)
        newHasVerticalScrollbar = (vScroll == ScrollbarAlwaysOn);

    if (m_scrollbarsSuppressed || (hScroll != ScrollbarAuto && vScroll != ScrollbarAuto))
        return;

    if (hScroll == ScrollbarAuto)
        newHasHorizontalScrollbar = docSize.width() > visibleWidth();
    if (vScroll == ScrollbarAuto)
        newHasVerticalScrollbar = docSize.height() > visibleHeight();

    if (useOverlayScrollbars())
        return;

    IntSize fullVisibleSize = visibleContentRect(IncludeScrollbars).size();

    bool attemptToRemoveScrollbars = (option == FirstPass
        && docSize.width() <= fullVisibleSize.width() && docSize.height() <= fullVisibleSize.height());
    if (attemptToRemoveScrollbars) {
        if (hScroll == ScrollbarAuto)
            newHasHorizontalScrollbar = false;
        if (vScroll == ScrollbarAuto)
            newHasVerticalScrollbar = false;
    }

    // If we ever turn one scrollbar off, always turn the other one off too.
    // Never ever try to both gain/lose a scrollbar in the same pass.
    if (!newHasHorizontalScrollbar && hasHorizontalScrollbar && vScroll != ScrollbarAlwaysOn)
        newHasVerticalScrollbar = false;
    if (!newHasVerticalScrollbar && hasVerticalScrollbar && hScroll != ScrollbarAlwaysOn)
        newHasHorizontalScrollbar = false;
}

void FrameView::updateScrollbarGeometry()
{
    if (m_horizontalScrollbar) {
        int clientWidth = visibleWidth();
        IntRect oldRect(m_horizontalScrollbar->frameRect());
        IntRect hBarRect((shouldPlaceVerticalScrollbarOnLeft() && m_verticalScrollbar) ? m_verticalScrollbar->width() : 0,
            height() - m_horizontalScrollbar->height(),
            width() - (m_verticalScrollbar ? m_verticalScrollbar->width() : 0),
            m_horizontalScrollbar->height());
        m_horizontalScrollbar->setFrameRect(adjustScrollbarRectForResizer(hBarRect, m_horizontalScrollbar.get()));
        if (!m_scrollbarsSuppressed && oldRect != m_horizontalScrollbar->frameRect())
            m_horizontalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(true);
        m_horizontalScrollbar->setEnabled(contentsWidth() > clientWidth);
        m_horizontalScrollbar->setProportion(clientWidth, contentsWidth());
        m_horizontalScrollbar->offsetDidChange();
        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(false);
    }

    if (m_verticalScrollbar) {
        int clientHeight = visibleHeight();
        IntRect oldRect(m_verticalScrollbar->frameRect());
        IntRect vBarRect(shouldPlaceVerticalScrollbarOnLeft() ? 0 : (width() - m_verticalScrollbar->width()),
            0,
            m_verticalScrollbar->width(),
            height() - (m_horizontalScrollbar ? m_horizontalScrollbar->height() : 0));
        m_verticalScrollbar->setFrameRect(adjustScrollbarRectForResizer(vBarRect, m_verticalScrollbar.get()));
        if (!m_scrollbarsSuppressed && oldRect != m_verticalScrollbar->frameRect())
            m_verticalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(true);
        m_verticalScrollbar->setEnabled(contentsHeight() > clientHeight);
        m_verticalScrollbar->setProportion(clientHeight, contentsHeight());
        m_verticalScrollbar->offsetDidChange();
        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(false);
    }
}

IntRect FrameView::adjustScrollbarRectForResizer(const IntRect& rect, Scrollbar* scrollbar)
{
    // Get our window resizer rect and see if we overlap. Adjust to avoid the overlap
    // if necessary.
    IntRect adjustedRect(rect);
    bool overlapsResizer = false;
    if (!rect.isEmpty() && !windowResizerRect().isEmpty()) {
        IntRect resizerRect = convertFromContainingWindow(windowResizerRect());
        if (rect.intersects(resizerRect)) {
            if (scrollbar->orientation() == HorizontalScrollbar) {
                int overlap = rect.maxX() - resizerRect.x();
                if (overlap > 0 && resizerRect.maxX() >= rect.maxX()) {
                    adjustedRect.setWidth(rect.width() - overlap);
                    overlapsResizer = true;
                }
            } else {
                int overlap = rect.maxY() - resizerRect.y();
                if (overlap > 0 && resizerRect.maxY() >= rect.maxY()) {
                    adjustedRect.setHeight(rect.height() - overlap);
                    overlapsResizer = true;
                }
            }
        }
    }
    if (overlapsResizer != scrollbar->overlapsResizer()) {
        scrollbar->setOverlapsResizer(overlapsResizer);
        adjustScrollbarsAvoidingResizerCount(overlapsResizer ? 1 : -1);
    }
    return adjustedRect;
}

bool FrameView::adjustScrollbarExistence(ComputeScrollbarExistenceOption option)
{
    ASSERT(m_inUpdateScrollbars);

    // If we came in here with the view already needing a layout, then go ahead and do that
    // first.  (This will be the common case, e.g., when the page changes due to window resizing for example).
    // This layout will not re-enter updateScrollbars and does not count towards our max layout pass total.
    if (!m_scrollbarsSuppressed)
        scrollbarExistenceDidChange();

    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool hasVerticalScrollbar = m_verticalScrollbar;

    bool newHasHorizontalScrollbar = false;
    bool newHasVerticalScrollbar = false;
    computeScrollbarExistence(newHasHorizontalScrollbar, newHasVerticalScrollbar, contentsSize(), option);

    bool scrollbarExistenceChanged = hasHorizontalScrollbar != newHasHorizontalScrollbar || hasVerticalScrollbar != newHasVerticalScrollbar;
    if (!scrollbarExistenceChanged)
        return false;

    setHasHorizontalScrollbar(newHasHorizontalScrollbar);
    setHasVerticalScrollbar(newHasVerticalScrollbar);

    if (m_scrollbarsSuppressed)
        return true;

    if (!useOverlayScrollbars())
        contentsResized();
    scrollbarExistenceDidChange();
    return true;
}

void FrameView::updateScrollbars(const DoubleSize& desiredOffset)
{
    if (scrollbarsDisabled()) {
        setScrollOffsetFromUpdateScrollbars(desiredOffset);
        return;
    }

    if (m_inUpdateScrollbars)
        return;
    InUpdateScrollbarsScope inUpdateScrollbarsScope(this);

    IntSize oldVisibleSize = visibleSize();

    bool scrollbarExistenceChanged = false;
    int maxUpdateScrollbarsPass = useOverlayScrollbars() || m_scrollbarsSuppressed ? 1 : 3;
    for (int updateScrollbarsPass = 0; updateScrollbarsPass < maxUpdateScrollbarsPass; updateScrollbarsPass++) {
        if (!adjustScrollbarExistence(updateScrollbarsPass ? Incremental : FirstPass))
            break;
        scrollbarExistenceChanged = true;
    }

    updateScrollbarGeometry();

    if (scrollbarExistenceChanged) {
        // FIXME: Is frameRectsChanged really necessary here? Have any frame rects changed?
        frameRectsChanged();
        positionScrollbarLayers();
        updateScrollCorner();
    }

    // FIXME: We don't need to do this if we are composited.
    IntSize newVisibleSize = visibleSize();
    if (newVisibleSize.width() > oldVisibleSize.width()) {
        if (shouldPlaceVerticalScrollbarOnLeft())
            invalidateRect(IntRect(0, 0, newVisibleSize.width() - oldVisibleSize.width(), newVisibleSize.height()));
        else
            invalidateRect(IntRect(oldVisibleSize.width(), 0, newVisibleSize.width() - oldVisibleSize.width(), newVisibleSize.height()));
    }
    if (newVisibleSize.height() > oldVisibleSize.height())
        invalidateRect(IntRect(0, oldVisibleSize.height(), newVisibleSize.width(), newVisibleSize.height() - oldVisibleSize.height()));

    setScrollOffsetFromUpdateScrollbars(desiredOffset);
}

void FrameView::setScrollOffsetFromUpdateScrollbars(const DoubleSize& offset)
{
    DoublePoint adjustedScrollPosition = DoublePoint(offset);

    if (!isRubberBandInProgress())
        adjustedScrollPosition = adjustScrollPositionWithinRange(adjustedScrollPosition);

    if (adjustedScrollPosition != scrollPositionDouble() || scrollOriginChanged()) {
        ScrollableArea::scrollToOffsetWithoutAnimation(toFloatPoint(adjustedScrollPosition));
        resetScrollOriginChanged();
    }
}

const int panIconSizeLength = 16;

IntRect FrameView::rectToCopyOnScroll() const
{
    IntRect scrollViewRect = convertToContainingWindow(IntRect((shouldPlaceVerticalScrollbarOnLeft() && verticalScrollbar()) ? verticalScrollbar()->width() : 0, 0, visibleWidth(), visibleHeight()));
    if (hasOverlayScrollbars()) {
        int verticalScrollbarWidth = (verticalScrollbar() && !hasLayerForVerticalScrollbar()) ? verticalScrollbar()->width() : 0;
        int horizontalScrollbarHeight = (horizontalScrollbar() && !hasLayerForHorizontalScrollbar()) ? horizontalScrollbar()->height() : 0;

        scrollViewRect.setWidth(scrollViewRect.width() - verticalScrollbarWidth);
        scrollViewRect.setHeight(scrollViewRect.height() - horizontalScrollbarHeight);
    }
    return scrollViewRect;
}

void FrameView::scrollContentsIfNeeded()
{
    if (m_pendingScrollDelta.isZero())
        return;
    DoubleSize scrollDelta = m_pendingScrollDelta;
    m_pendingScrollDelta = DoubleSize();
    // FIXME: Change scrollContents() to take DoubleSize. crbug.com/414283.
    scrollContents(flooredIntSize(scrollDelta));
}

void FrameView::scrollContents(const IntSize& scrollDelta)
{
    HostWindow* window = hostWindow();
    if (!window)
        return;

    IntRect clipRect = windowClipRect();
    IntRect updateRect = clipRect;
    updateRect.intersect(rectToCopyOnScroll());

    if (m_shouldDrawPanScrollIcon) {
        // FIXME: the pan icon is broken when accelerated compositing is on, since it will draw under the compositing layers.
        // https://bugs.webkit.org/show_bug.cgi?id=47837
        int panIconDirtySquareSizeLength = 2 * (panIconSizeLength + std::max(abs(scrollDelta.width()), abs(scrollDelta.height()))); // We only want to repaint what's necessary
        IntPoint panIconDirtySquareLocation = IntPoint(m_panScrollIconPoint.x() - (panIconDirtySquareSizeLength / 2), m_panScrollIconPoint.y() - (panIconDirtySquareSizeLength / 2));
        IntRect panScrollIconDirtyRect = IntRect(panIconDirtySquareLocation, IntSize(panIconDirtySquareSizeLength, panIconDirtySquareSizeLength));
        panScrollIconDirtyRect.intersect(clipRect);
        window->invalidateContentsAndRootView(panScrollIconDirtyRect);
    }

    if (!scrollContentsFastPath(-scrollDelta))
        scrollContentsSlowPath(updateRect);

    // Invalidate the overhang areas if they are visible.
    updateOverhangAreas();

    // This call will move children with native widgets (plugins) and invalidate them as well.
    frameRectsChanged();
}

IntPoint FrameView::rootViewToContents(const IntPoint& rootViewPoint) const
{
    IntPoint viewPoint = convertFromContainingWindow(rootViewPoint);
    return viewPoint + scrollOffset();
}

IntPoint FrameView::contentsToRootView(const IntPoint& contentsPoint) const
{
    IntPoint viewPoint = contentsPoint - scrollOffset();
    return convertToContainingWindow(viewPoint);
}

IntRect FrameView::rootViewToContents(const IntRect& rootViewRect) const
{
    IntRect viewRect = convertFromContainingWindow(rootViewRect);
    viewRect.move(scrollOffset());
    return viewRect;
}

IntRect FrameView::contentsToRootView(const IntRect& contentsRect) const
{
    IntRect viewRect = contentsRect;
    viewRect.move(-scrollOffset());
    return convertToContainingWindow(viewRect);
}

IntPoint FrameView::windowToContents(const IntPoint& windowPoint) const
{
    IntPoint viewPoint = convertFromContainingWindow(windowPoint);
    return viewPoint + scrollOffset();
}

FloatPoint FrameView::windowToContents(const FloatPoint& windowPoint) const
{
    FloatPoint viewPoint = convertFromContainingWindow(windowPoint);
    return viewPoint + scrollOffset();
}

IntPoint FrameView::contentsToWindow(const IntPoint& contentsPoint) const
{
    IntPoint viewPoint = contentsPoint - scrollOffset();
    return convertToContainingWindow(viewPoint);
}

IntRect FrameView::windowToContents(const IntRect& windowRect) const
{
    IntRect viewRect = convertFromContainingWindow(windowRect);
    viewRect.move(scrollOffset());
    return viewRect;
}

IntRect FrameView::contentsToWindow(const IntRect& contentsRect) const
{
    IntRect viewRect = contentsRect;
    viewRect.move(-scrollOffset());
    return convertToContainingWindow(viewRect);
}

IntRect FrameView::contentsToScreen(const IntRect& rect) const
{
    HostWindow* window = hostWindow();
    if (!window)
        return IntRect();
    return window->rootViewToScreen(contentsToRootView(rect));
}

bool FrameView::containsScrollbarsAvoidingResizer() const
{
    return !m_scrollbarsAvoidingResizer;
}

void FrameView::adjustScrollbarsAvoidingResizerCount(int overlapDelta)
{
    int oldCount = m_scrollbarsAvoidingResizer;
    m_scrollbarsAvoidingResizer += overlapDelta;
    if (parent()) {
        toFrameView(parent())->adjustScrollbarsAvoidingResizerCount(overlapDelta);
    } else if (!scrollbarsSuppressed()) {
        // If we went from n to 0 or from 0 to n and we're the outermost view,
        // we need to invalidate the windowResizerRect(), since it will now need to paint
        // differently.
        if ((oldCount > 0 && m_scrollbarsAvoidingResizer == 0)
            || (oldCount == 0 && m_scrollbarsAvoidingResizer > 0))
            invalidateRect(windowResizerRect());
    }
}

void FrameView::setScrollbarsSuppressed(bool suppressed, bool repaintOnUnsuppress)
{
    if (suppressed == m_scrollbarsSuppressed)
        return;

    m_scrollbarsSuppressed = suppressed;

    if (repaintOnUnsuppress && !suppressed) {
        if (m_horizontalScrollbar)
            m_horizontalScrollbar->invalidate();
        if (m_verticalScrollbar)
            m_verticalScrollbar->invalidate();

        // Invalidate the scroll corner too on unsuppress.
        invalidateRect(scrollCornerRect());
    }
}

Scrollbar* FrameView::scrollbarAtWindowPoint(const IntPoint& windowPoint)
{
    IntPoint viewPoint = convertFromContainingWindow(windowPoint);
    return scrollbarAtViewPoint(viewPoint);
}

Scrollbar* FrameView::scrollbarAtViewPoint(const IntPoint& viewPoint)
{
    if (m_horizontalScrollbar && m_horizontalScrollbar->shouldParticipateInHitTesting() && m_horizontalScrollbar->frameRect().contains(viewPoint))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && m_verticalScrollbar->shouldParticipateInHitTesting() && m_verticalScrollbar->frameRect().contains(viewPoint))
        return m_verticalScrollbar.get();
    return 0;
}

static void positionScrollbarLayer(GraphicsLayer* graphicsLayer, Scrollbar* scrollbar)
{
    if (!graphicsLayer || !scrollbar)
        return;

    IntRect scrollbarRect = scrollbar->frameRect();
    graphicsLayer->setPosition(scrollbarRect.location());

    if (scrollbarRect.size() == graphicsLayer->size())
        return;

    graphicsLayer->setSize(scrollbarRect.size());

    if (graphicsLayer->hasContentsLayer()) {
        graphicsLayer->setContentsRect(IntRect(0, 0, scrollbarRect.width(), scrollbarRect.height()));
        return;
    }

    graphicsLayer->setDrawsContent(true);
    graphicsLayer->setNeedsDisplay();
}

static void positionScrollCornerLayer(GraphicsLayer* graphicsLayer, const IntRect& cornerRect)
{
    if (!graphicsLayer)
        return;
    graphicsLayer->setDrawsContent(!cornerRect.isEmpty());
    graphicsLayer->setPosition(cornerRect.location());
    if (cornerRect.size() != graphicsLayer->size())
        graphicsLayer->setNeedsDisplay();
    graphicsLayer->setSize(cornerRect.size());
}

void FrameView::positionScrollbarLayers()
{
    positionScrollbarLayer(layerForHorizontalScrollbar(), horizontalScrollbar());
    positionScrollbarLayer(layerForVerticalScrollbar(), verticalScrollbar());
    positionScrollCornerLayer(layerForScrollCorner(), scrollCornerRect());
}

bool FrameView::userInputScrollable(ScrollbarOrientation orientation) const
{
    Document* document = frame().document();
    Element* fullscreenElement = Fullscreen::fullscreenElementFrom(*document);
    if (fullscreenElement && fullscreenElement != document->documentElement())
        return false;

    ScrollbarMode mode = (orientation == HorizontalScrollbar) ?
        m_horizontalScrollbarMode : m_verticalScrollbarMode;

    return mode == ScrollbarAuto || mode == ScrollbarAlwaysOn;
}

bool FrameView::shouldPlaceVerticalScrollbarOnLeft() const
{
    return false;
}

IntRect FrameView::scrollCornerRect() const
{
    IntRect cornerRect;

    if (hasOverlayScrollbars())
        return cornerRect;

    if (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) {
        cornerRect.unite(IntRect(shouldPlaceVerticalScrollbarOnLeft() ? 0 : m_horizontalScrollbar->width(),
            height() - m_horizontalScrollbar->height(),
            width() - m_horizontalScrollbar->width(),
            m_horizontalScrollbar->height()));
    }

    if (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0) {
        cornerRect.unite(IntRect(shouldPlaceVerticalScrollbarOnLeft() ? 0 : (width() - m_verticalScrollbar->width()),
            m_verticalScrollbar->height(),
            m_verticalScrollbar->width(),
            height() - m_verticalScrollbar->height()));
    }

    return cornerRect;
}

bool FrameView::isScrollCornerVisible() const
{
    return !scrollCornerRect().isEmpty();
}

void FrameView::invalidateScrollCornerRect(const IntRect& rect)
{
    invalidateRect(rect);
}

void FrameView::paintPanScrollIcon(GraphicsContext* context)
{
    DEFINE_STATIC_REF(Image, panScrollIcon, (Image::loadPlatformResource("panIcon")));
    IntPoint iconGCPoint = m_panScrollIconPoint;
    if (parent())
        iconGCPoint = toFrameView(parent())->windowToContents(iconGCPoint);
    context->drawImage(panScrollIcon, iconGCPoint);
}

void FrameView::paint(GraphicsContext* context, const IntRect& rect)
{
    FramePainter(*this).paint(context, rect);
}

void FrameView::paintContents(GraphicsContext* context, const IntRect& damageRect)
{
    FramePainter(*this).paintContents(context, damageRect);
}

void FrameView::calculateOverhangAreasForPainting(IntRect& horizontalOverhangRect, IntRect& verticalOverhangRect)
{
    int verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar())
        ? verticalScrollbar()->width() : 0;
    int horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar())
        ? horizontalScrollbar()->height() : 0;

    int physicalScrollY = scrollPosition().y() + scrollOrigin().y();
    if (physicalScrollY < 0) {
        horizontalOverhangRect = frameRect();
        horizontalOverhangRect.setHeight(-physicalScrollY);
        horizontalOverhangRect.setWidth(horizontalOverhangRect.width() - verticalScrollbarWidth);
    } else if (physicalScrollY > 0 && contentsHeight() && physicalScrollY > contentsHeight() - visibleHeight()) {
        int height = physicalScrollY - (contentsHeight() - visibleHeight());
        horizontalOverhangRect = frameRect();
        horizontalOverhangRect.setY(frameRect().maxY() - height - horizontalScrollbarHeight);
        horizontalOverhangRect.setHeight(height);
        horizontalOverhangRect.setWidth(horizontalOverhangRect.width() - verticalScrollbarWidth);
    }

    int physicalScrollX = scrollPosition().x() + scrollOrigin().x();
    if (physicalScrollX < 0) {
        verticalOverhangRect.setWidth(-physicalScrollX);
        verticalOverhangRect.setHeight(frameRect().height() - horizontalOverhangRect.height() - horizontalScrollbarHeight);
        verticalOverhangRect.setX(frameRect().x());
        if (horizontalOverhangRect.y() == frameRect().y())
            verticalOverhangRect.setY(frameRect().y() + horizontalOverhangRect.height());
        else
            verticalOverhangRect.setY(frameRect().y());
    } else if (physicalScrollX > 0 && contentsWidth() && physicalScrollX > contentsWidth() - visibleWidth()) {
        int width = physicalScrollX - (contentsWidth() - visibleWidth());
        verticalOverhangRect.setWidth(width);
        verticalOverhangRect.setHeight(frameRect().height() - horizontalOverhangRect.height() - horizontalScrollbarHeight);
        verticalOverhangRect.setX(frameRect().maxX() - width - verticalScrollbarWidth);
        if (horizontalOverhangRect.y() == frameRect().y())
            verticalOverhangRect.setY(frameRect().y() + horizontalOverhangRect.height());
        else
            verticalOverhangRect.setY(frameRect().y());
    }
}

void FrameView::updateOverhangAreas()
{
    HostWindow* window = hostWindow();
    if (!window)
        return;

    IntRect horizontalOverhangRect;
    IntRect verticalOverhangRect;
    calculateOverhangAreasForPainting(horizontalOverhangRect, verticalOverhangRect);
    if (!horizontalOverhangRect.isEmpty())
        window->invalidateContentsAndRootView(horizontalOverhangRect);
    if (!verticalOverhangRect.isEmpty())
        window->invalidateContentsAndRootView(verticalOverhangRect);
}

bool FrameView::isPointInScrollbarCorner(const IntPoint& windowPoint)
{
    if (!scrollbarCornerPresent())
        return false;

    IntPoint viewPoint = convertFromContainingWindow(windowPoint);

    if (m_horizontalScrollbar) {
        int horizontalScrollbarYMin = m_horizontalScrollbar->frameRect().y();
        int horizontalScrollbarYMax = m_horizontalScrollbar->frameRect().y() + m_horizontalScrollbar->frameRect().height();
        int horizontalScrollbarXMin = m_horizontalScrollbar->frameRect().x() + m_horizontalScrollbar->frameRect().width();

        return viewPoint.y() > horizontalScrollbarYMin && viewPoint.y() < horizontalScrollbarYMax && viewPoint.x() > horizontalScrollbarXMin;
    }

    int verticalScrollbarXMin = m_verticalScrollbar->frameRect().x();
    int verticalScrollbarXMax = m_verticalScrollbar->frameRect().x() + m_verticalScrollbar->frameRect().width();
    int verticalScrollbarYMin = m_verticalScrollbar->frameRect().y() + m_verticalScrollbar->frameRect().height();

    return viewPoint.x() > verticalScrollbarXMin && viewPoint.x() < verticalScrollbarXMax && viewPoint.y() > verticalScrollbarYMin;
}

bool FrameView::scrollbarCornerPresent() const
{
    return (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0)
        || (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0);
}

IntRect FrameView::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& localRect) const
{
    // Scrollbars won't be transformed within us
    IntRect newRect = localRect;
    newRect.moveBy(scrollbar->location());
    return newRect;
}

IntRect FrameView::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    IntRect newRect = parentRect;
    // Scrollbars won't be transformed within us
    newRect.moveBy(-scrollbar->location());
    return newRect;
}

// FIXME: test these on windows
IntPoint FrameView::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& localPoint) const
{
    // Scrollbars won't be transformed within us
    IntPoint newPoint = localPoint;
    newPoint.moveBy(scrollbar->location());
    return newPoint;
}

IntPoint FrameView::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    IntPoint newPoint = parentPoint;
    // Scrollbars won't be transformed within us
    newPoint.moveBy(-scrollbar->location());
    return newPoint;
}

void FrameView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (!isSelfVisible())
        return;

    for (const auto& child : m_children)
        child->setParentVisible(visible);
}

void FrameView::show()
{
    if (!isSelfVisible()) {
        setSelfVisible(true);
        if (isParentVisible()) {
            for (const auto& child : m_children)
                child->setParentVisible(true);
        }
    }

    Widget::show();
}

void FrameView::hide()
{
    if (isSelfVisible()) {
        if (isParentVisible()) {
            for (const auto& child : m_children)
                child->setParentVisible(false);
        }
        setSelfVisible(false);
    }

    Widget::hide();
}

void FrameView::addPanScrollIcon(const IntPoint& iconPosition)
{
    HostWindow* window = hostWindow();
    if (!window)
        return;
    m_shouldDrawPanScrollIcon = true;
    m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2);
    window->invalidateContentsAndRootView(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)));
}

void FrameView::removePanScrollIcon()
{
    HostWindow* window = hostWindow();
    if (!window)
        return;
    m_shouldDrawPanScrollIcon = false;
    window->invalidateContentsAndRootView(IntRect(m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)));
}

void FrameView::setScrollOrigin(const IntPoint& origin, bool updatePositionAtAll, bool updatePositionSynchronously)
{
    if (scrollOrigin() == origin)
        return;

    ScrollableArea::setScrollOrigin(origin);

    // Update if the scroll origin changes, since our position will be different if the content size did not change.
    if (updatePositionAtAll && updatePositionSynchronously)
        updateScrollbars(scrollOffsetDouble());
}

} // namespace blink
