/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "core/css/FontFaceSet.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/css/CSSFontFaceLoadEvent.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"

namespace WebCore {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver FINAL : public FontFace::LoadFontCallback {
public:
    static PassRefPtrWillBeRawPtr<LoadFontPromiseResolver> create(FontFaceArray faces, ScriptState* scriptState)
    {
        return adoptRefWillBeNoop(new LoadFontPromiseResolver(faces, scriptState));
    }

    void loadFonts(ExecutionContext*);
    ScriptPromise promise() { return m_resolver->promise(); }

    virtual void notifyLoaded(FontFace*) OVERRIDE;
    virtual void notifyError(FontFace*) OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    LoadFontPromiseResolver(FontFaceArray faces, ScriptState* scriptState)
        : m_numLoading(faces.size())
        , m_errorOccured(false)
        , m_resolver(ScriptPromiseResolverWithContext::create(scriptState))
    {
        m_fontFaces.swap(faces);
    }

    WillBeHeapVector<RefPtrWillBeMember<FontFace> > m_fontFaces;
    int m_numLoading;
    bool m_errorOccured;
    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
};

void LoadFontPromiseResolver::loadFonts(ExecutionContext* context)
{
    if (!m_numLoading) {
        m_resolver->resolve(m_fontFaces);
        return;
    }

    for (size_t i = 0; i < m_fontFaces.size(); i++)
        m_fontFaces[i]->loadWithCallback(this, context);
}

void LoadFontPromiseResolver::notifyLoaded(FontFace* fontFace)
{
    m_numLoading--;
    if (m_numLoading || m_errorOccured)
        return;

    m_resolver->resolve(m_fontFaces);
}

void LoadFontPromiseResolver::notifyError(FontFace* fontFace)
{
    m_numLoading--;
    if (!m_errorOccured) {
        m_errorOccured = true;
        m_resolver->reject(fontFace->error());
    }
}

void LoadFontPromiseResolver::trace(Visitor* visitor)
{
    visitor->trace(m_fontFaces);
    LoadFontCallback::trace(visitor);
}

class FontsReadyPromiseResolver {
public:
    static PassOwnPtr<FontsReadyPromiseResolver> create(ScriptState* scriptState)
    {
        return adoptPtr(new FontsReadyPromiseResolver(scriptState));
    }

    void resolve(PassRefPtrWillBeRawPtr<FontFaceSet> fontFaceSet)
    {
        m_resolver->resolve(fontFaceSet);
    }

    ScriptPromise promise() { return m_resolver->promise(); }

private:
    explicit FontsReadyPromiseResolver(ScriptState* scriptState)
        : m_resolver(ScriptPromiseResolverWithContext::create(scriptState))
    {
    }

    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
};

FontFaceSet::FontFaceSet(Document& document)
    : ActiveDOMObject(&document)
    , m_shouldFireLoadingEvent(false)
    , m_asyncRunner(this, &FontFaceSet::handlePendingEventsAndPromises)
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(executionContext());
}

bool FontFaceSet::inActiveDocumentContext() const
{
    ExecutionContext* context = executionContext();
    return context && toDocument(context)->isActive();
}

void FontFaceSet::addFontFacesToFontFaceCache(FontFaceCache* fontFaceCache, CSSFontSelector* fontSelector)
{
    for (ListHashSet<RefPtrWillBeMember<FontFace> >::iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it)
        fontFaceCache->addFontFace(fontSelector, *it, false);
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded", AtomicString::ConstructFromLiteral));
    return (!m_loadingFonts.isEmpty() || hasLoadedFonts()) ? loading : loaded;
}

void FontFaceSet::handlePendingEventsAndPromisesSoon()
{
    // setPendingActivity() is unnecessary because m_asyncRunner will be
    // automatically stopped on destruction.
    m_asyncRunner.runAsync();
}

void FontFaceSet::didLayout()
{
    if (document()->frame()->isMainFrame() && m_loadingFonts.isEmpty())
        m_histogram.record();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;
    handlePendingEventsAndPromisesSoon();
}

void FontFaceSet::handlePendingEventsAndPromises()
{
    fireLoadingEvent();
    fireDoneEventIfPossible();
}

void FontFaceSet::fireLoadingEvent()
{
    if (m_shouldFireLoadingEvent) {
        m_shouldFireLoadingEvent = false;
        dispatchEvent(CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loading));
    }
}

void FontFaceSet::suspend()
{
    m_asyncRunner.suspend();
}

void FontFaceSet::resume()
{
    m_asyncRunner.resume();
}

void FontFaceSet::stop()
{
    m_asyncRunner.stop();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    addToLoadingFonts(fontFace);
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled())
        m_loadedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled())
        m_failedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::addToLoadingFonts(PassRefPtrWillBeRawPtr<FontFace> fontFace)
{
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled() && m_loadingFonts.isEmpty() && !hasLoadedFonts()) {
        m_shouldFireLoadingEvent = true;
        handlePendingEventsAndPromisesSoon();
    }
    m_loadingFonts.add(fontFace);
}

void FontFaceSet::removeFromLoadingFonts(PassRefPtrWillBeRawPtr<FontFace> fontFace)
{
    m_loadingFonts.remove(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled() && m_loadingFonts.isEmpty())
        handlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready(ScriptState* scriptState)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();
    OwnPtr<FontsReadyPromiseResolver> resolver = FontsReadyPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_readyResolvers.append(resolver.release());
    handlePendingEventsAndPromisesSoon();
    return promise;
}

void FontFaceSet::add(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return;
    }
    if (m_nonCSSConnectedFaces.contains(fontFace))
        return;
    if (isCSSConnectedFontFace(fontFace)) {
        exceptionState.throwDOMException(InvalidModificationError, "Cannot add a CSS-connected FontFace.");
        return;
    }
    CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
    m_nonCSSConnectedFaces.add(fontFace);
    fontSelector->fontFaceCache()->addFontFace(fontSelector, fontFace, false);
    if (fontFace->loadStatus() == FontFace::Loading)
        addToLoadingFonts(fontFace);
    fontSelector->fontFaceInvalidated();
}

void FontFaceSet::clear()
{
    if (!inActiveDocumentContext() || m_nonCSSConnectedFaces.isEmpty())
        return;
    CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();
    for (ListHashSet<RefPtrWillBeMember<FontFace> >::iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it) {
        fontFaceCache->removeFontFace(it->get(), false);
        if ((*it)->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(*it);
    }
    m_nonCSSConnectedFaces.clear();
    fontSelector->fontFaceInvalidated();
}

bool FontFaceSet::remove(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    ListHashSet<RefPtrWillBeMember<FontFace> >::iterator it = m_nonCSSConnectedFaces.find(fontFace);
    if (it != m_nonCSSConnectedFaces.end()) {
        m_nonCSSConnectedFaces.remove(it);
        CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
        fontSelector->fontFaceCache()->removeFontFace(fontFace, false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
        fontSelector->fontFaceInvalidated();
        return true;
    }
    if (isCSSConnectedFontFace(fontFace))
        exceptionState.throwDOMException(InvalidModificationError, "Cannot delete a CSS-connected FontFace.");
    return false;
}

bool FontFaceSet::has(FontFace* fontFace, ExceptionState& exceptionState) const
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    return m_nonCSSConnectedFaces.contains(fontFace) || isCSSConnectedFontFace(fontFace);
}

const ListHashSet<RefPtrWillBeMember<FontFace> >& FontFaceSet::cssConnectedFontFaceList() const
{
    Document* d = document();
    d->ensureStyleResolver(); // Flush pending style changes.
    return d->styleEngine()->fontSelector()->fontFaceCache()->cssConnectedFontFaces();
}

bool FontFaceSet::isCSSConnectedFontFace(FontFace* fontFace) const
{
    return cssConnectedFontFaceList().contains(fontFace);
}

void FontFaceSet::forEach(PassOwnPtr<FontFaceSetForEachCallback> callback, ScriptValue& thisArg) const
{
    forEachInternal(callback, &thisArg);
}

void FontFaceSet::forEach(PassOwnPtr<FontFaceSetForEachCallback> callback) const
{
    forEachInternal(callback, 0);
}

void FontFaceSet::forEachInternal(PassOwnPtr<FontFaceSetForEachCallback> callback, ScriptValue* thisArg) const
{
    if (!inActiveDocumentContext())
        return;
    const ListHashSet<RefPtrWillBeMember<FontFace> >& cssConnectedFaces = cssConnectedFontFaceList();
    WillBeHeapVector<RefPtrWillBeMember<FontFace> > fontFaces;
    fontFaces.reserveInitialCapacity(cssConnectedFaces.size() + m_nonCSSConnectedFaces.size());
    for (ListHashSet<RefPtrWillBeMember<FontFace> >::const_iterator it = cssConnectedFaces.begin(); it != cssConnectedFaces.end(); ++it)
        fontFaces.append(*it);
    for (ListHashSet<RefPtrWillBeMember<FontFace> >::const_iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it)
        fontFaces.append(*it);

    for (size_t i = 0; i < fontFaces.size(); ++i) {
        FontFace* face = fontFaces[i].get();
        if (thisArg)
            callback->handleItem(*thisArg, face, face, const_cast<FontFaceSet*>(this));
        else
            callback->handleItem(face, face, const_cast<FontFaceSet*>(this));
    }
}

unsigned long FontFaceSet::size() const
{
    if (!inActiveDocumentContext())
        return m_nonCSSConnectedFaces.size();
    return cssConnectedFontFaceList().size() + m_nonCSSConnectedFaces.size();
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (m_shouldFireLoadingEvent)
        return;
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (hasLoadedFonts()) {
        RefPtrWillBeRawPtr<CSSFontFaceLoadEvent> doneEvent = nullptr;
        RefPtrWillBeRawPtr<CSSFontFaceLoadEvent> errorEvent = nullptr;
        doneEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingdone, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingerror, m_failedFonts);
            m_failedFonts.clear();
        }
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (!m_readyResolvers.isEmpty()) {
        Vector<OwnPtr<FontsReadyPromiseResolver> > resolvers;
        m_readyResolvers.swap(resolvers);
        for (size_t index = 0; index < resolvers.size(); ++index)
            resolvers[index]->resolve(this);
    }
}

static const String& nullToSpace(const String& s)
{
    DEFINE_STATIC_LOCAL(String, space, (" "));
    return s.isNull() ? space : s;
}

ScriptPromise FontFaceSet::load(ScriptState* scriptState, const String& fontString, const String& text)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
        ScriptPromise promise = resolver->promise();
        resolver->reject(DOMError::create(SyntaxError, "Could not resolve '" + fontString + "' as a font."));
        return promise;
    }

    FontFaceCache* fontFaceCache = document()->styleEngine()->fontSelector()->fontFaceCache();
    FontFaceArray faces;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* segmentedFontFace = fontFaceCache->get(font.fontDescription(), f->family());
        if (segmentedFontFace)
            segmentedFontFace->match(nullToSpace(text), faces);
    }

    RefPtrWillBeRawPtr<LoadFontPromiseResolver> resolver = LoadFontPromiseResolver::create(faces, scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->loadFonts(executionContext()); // After this, resolver->promise() may return null.
    return promise;
}

bool FontFaceSet::check(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwDOMException(SyntaxError, "Could not resolve '" + fontString + "' as a font.");
        return false;
    }

    CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();

    bool hasLoadedFaces = false;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = fontFaceCache->get(font.fontDescription(), f->family());
        if (face) {
            if (!face->checkFont(nullToSpace(text)))
                return false;
            hasLoadedFaces = true;
        }
    }
    if (hasLoadedFaces)
        return true;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        if (fontSelector->isPlatformFontAvailable(font.fontDescription(), f->family()))
            return true;
    }
    return false;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    RefPtrWillBeRawPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    BisonCSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, fontString, true, HTMLStandardMode, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<RenderStyle> style = RenderStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };
    StyleResolver& styleResolver = document()->ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), style.get());

    font = style->font();
    font.update(document()->styleEngine()->fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::updateStatus(FontFace* fontFace)
{
    if (m_status == Reported)
        return;
    if (fontFace->hadBlankText())
        m_status = HadBlankText;
    else if (m_status == NoWebFonts)
        m_status = DidNotHaveBlankText;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (!m_recorded) {
        m_recorded = true;
        blink::Platform::current()->histogramCustomCounts("WebFont.WebFontsInPage", m_count, 1, 100, 50);
    }
    if (m_status == HadBlankText || m_status == DidNotHaveBlankText) {
        blink::Platform::current()->histogramEnumeration("WebFont.HadBlankText", m_status == HadBlankText ? 1 : 0, 2);
        m_status = Reported;
    }
}

static const char* supplementName()
{
    return "FontFaceSet";
}

PassRefPtrWillBeRawPtr<FontFaceSet> FontFaceSet::from(Document& document)
{
    RefPtrWillBeRawPtr<FontFaceSet> fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName()));
    if (!fonts) {
        fonts = FontFaceSet::create(document);
        SupplementType::provideTo(document, supplementName(), fonts);
    }

    return fonts.release();
}

void FontFaceSet::didLayout(Document& document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName())))
        fonts->didLayout();
}

#if ENABLE(OILPAN)
void FontFaceSet::trace(Visitor* visitor)
{
    visitor->trace(m_loadingFonts);
    visitor->trace(m_loadedFonts);
    visitor->trace(m_failedFonts);
    visitor->trace(m_nonCSSConnectedFaces);
    DocumentSupplement::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
}
#endif

} // namespace WebCore
