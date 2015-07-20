/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/css/resolver/StyleResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/StyleInterpolation.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePool.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFace.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include "core/css/resolver/MatchResult.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/css/resolver/StyleResolverParentScope.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/StdLibExtras.h"

namespace {

using namespace blink;

void setAnimationUpdateIfNeeded(StyleResolverState& state, Element& element)
{
    // If any changes to CSS Animations were detected, stash the update away for application after the
    // render object is updated if we're in the appropriate scope.
    if (state.animationUpdate())
        element.ensureActiveAnimations().cssAnimations().setPendingUpdate(state.takeAnimationUpdate());
}

} // namespace

namespace blink {

using namespace HTMLNames;

RenderStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration()
{
    DEFINE_STATIC_REF_WILL_BE_PERSISTENT(MutableStylePropertySet, leftToRightDecl, (MutableStylePropertySet::create()));
    if (leftToRightDecl->isEmpty())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl;
}

static StylePropertySet* rightToLeftDeclaration()
{
    DEFINE_STATIC_REF_WILL_BE_PERSISTENT(MutableStylePropertySet, rightToLeftDecl, (MutableStylePropertySet::create()));
    if (rightToLeftDecl->isEmpty())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl;
}

static void addFontFaceRule(Document* document, CSSFontSelector* cssFontSelector, const StyleRuleFontFace* fontFaceRule)
{
    RefPtrWillBeRawPtr<FontFace> fontFace = FontFace::create(document, fontFaceRule);
    if (fontFace)
        cssFontSelector->fontFaceCache()->add(cssFontSelector, fontFaceRule, fontFace);
}

StyleResolver::StyleResolver(Document& document)
    : m_document(document)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
    , m_needCollectFeatures(false)
    , m_printMediaType(false)
    , m_styleResourceLoader(document.fetcher())
    , m_styleSharingDepth(0)
    , m_styleResolverStatsSequence(0)
    , m_accessCount(0)
{
    FrameView* view = document.view();
    if (view) {
        m_medium = adoptPtr(new MediaQueryEvaluator(&view->frame()));
        m_printMediaType = equalIgnoringCase(view->mediaType(), MediaTypeNames::print);
    } else {
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));
    }

    initWatchedSelectorRules(CSSSelectorWatch::from(document).watchedCallbackSelectors());
}

void StyleResolver::initWatchedSelectorRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRule> >& watchedSelectors)
{
    if (!watchedSelectors.size())
        return;
    m_watchedSelectorsRules = RuleSet::create();
    for (unsigned i = 0; i < watchedSelectors.size(); ++i)
        m_watchedSelectorsRules->addStyleRule(watchedSelectors[i].get(), RuleHasNoSpecialState);
}

void StyleResolver::lazyAppendAuthorStyleSheets(unsigned firstNew, const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& styleSheets)
{
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i)
        m_pendingStyleSheets.add(styleSheets[i].get());
}

void StyleResolver::removePendingAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& styleSheets)
{
    for (unsigned i = 0; i < styleSheets.size(); ++i)
        m_pendingStyleSheets.remove(styleSheets[i].get());
}

void StyleResolver::appendCSSStyleSheet(CSSStyleSheet* cssSheet)
{
    ASSERT(cssSheet);
    ASSERT(!cssSheet->disabled());
    if (cssSheet->mediaQueries() && !m_medium->eval(cssSheet->mediaQueries(), &m_viewportDependentMediaQueryResults))
        return;

    TreeScope* treeScope = ScopedStyleResolver::treeScopeFor(document(), cssSheet);
    if (!treeScope)
        return;

    unsigned index = treeScope->ensureScopedStyleResolver().appendCSSStyleSheet(cssSheet);

    addRulesFromSheet(cssSheet, treeScope, index);
}

void StyleResolver::addRulesFromSheet(CSSStyleSheet* cssSheet, TreeScope* treeScope, unsigned index)
{
    StyleSheetContents* sheet = cssSheet->contents();
    AddRuleFlags addRuleFlags = document().securityOrigin()->canRequest(sheet->baseURL()) ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState;
    const RuleSet& ruleSet = sheet->ensureRuleSet(*m_medium, addRuleFlags);

    addMediaQueryResults(ruleSet.viewportDependentMediaQueryResults());
    processScopedRules(ruleSet, cssSheet, index, treeScope->rootNode());
}

void StyleResolver::appendPendingAuthorStyleSheets()
{
    for (const auto& styleSheet : m_pendingStyleSheets)
        appendCSSStyleSheet(styleSheet);

    m_pendingStyleSheets.clear();
    finishAppendAuthorStyleSheets();
}

void StyleResolver::appendAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = styleSheets.size();
    for (unsigned i = 0; i < size; ++i)
        appendCSSStyleSheet(styleSheets[i].get());
}

void StyleResolver::finishAppendAuthorStyleSheets()
{
    collectFeatures();

    if (document().renderView() && document().renderView()->style())
        document().renderView()->style()->font().update(document().styleEngine()->fontSelector());

    collectViewportRules();

    document().styleEngine()->resetCSSFeatureFlags(m_features);
}

void StyleResolver::resetRuleFeatures()
{
    // Need to recreate RuleFeatureSet.
    m_features.clear();
    m_siblingRuleSet.clear();
    m_uncommonAttributeRuleSet.clear();
    m_needCollectFeatures = true;
}

void StyleResolver::processScopedRules(const RuleSet& authorRules, CSSStyleSheet* parentStyleSheet, unsigned parentIndex, ContainerNode& scope)
{
    const WillBeHeapVector<RawPtrWillBeMember<StyleRuleKeyframes> > keyframesRules = authorRules.keyframesRules();
    ScopedStyleResolver& resolver = scope.treeScope().ensureScopedStyleResolver();
    for (unsigned i = 0; i < keyframesRules.size(); ++i)
        resolver.addKeyframeStyle(keyframesRules[i]);

    m_treeBoundaryCrossingRules.addTreeBoundaryCrossingRules(authorRules, parentStyleSheet, parentIndex, scope);

    // FIXME(BUG 72461): We don't add @font-face rules of scoped style sheets for the moment.
    if (scope.isDocumentNode()) {
        const WillBeHeapVector<RawPtrWillBeMember<StyleRuleFontFace> > fontFaceRules = authorRules.fontFaceRules();
        for (unsigned i = 0; i < fontFaceRules.size(); ++i)
            addFontFaceRule(m_document, document().styleEngine()->fontSelector(), fontFaceRules[i]);
        if (fontFaceRules.size())
            invalidateMatchedPropertiesCache();
    }
}

void StyleResolver::resetAuthorStyle(TreeScope& treeScope)
{
    m_treeBoundaryCrossingRules.reset(&treeScope.rootNode());
    resetRuleFeatures();

    ScopedStyleResolver* resolver = treeScope.scopedStyleResolver();
    if (!resolver)
        return;

    resolver->resetAuthorStyle();
    if (treeScope.rootNode().isDocumentNode())
        return;

    // resolver is going to be freed below.
    treeScope.clearScopedStyleResolver();
}

static PassOwnPtrWillBeRawPtr<RuleSet> makeRuleSet(const WillBeHeapVector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    OwnPtrWillBeRawPtr<RuleSet> ruleSet = RuleSet::create();
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(rules[i].rule, rules[i].selectorIndex, rules[i].hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState);
    return ruleSet.release();
}

void StyleResolver::collectFeatures()
{
    m_features.clear();
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    if (defaultStyleSheets.defaultStyle())
        m_features.add(defaultStyleSheets.defaultStyle()->features());

    if (document().isViewSource())
        m_features.add(defaultStyleSheets.defaultViewSourceStyle()->features());

    if (document().isTransitionDocument())
        m_features.add(defaultStyleSheets.defaultTransitionStyle()->features());

    if (m_watchedSelectorsRules)
        m_features.add(m_watchedSelectorsRules->features());

    m_treeBoundaryCrossingRules.collectFeaturesTo(m_features);

    document().styleEngine()->collectScopedStyleFeaturesTo(m_features);

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
    m_needCollectFeatures = false;
}

bool StyleResolver::hasRulesForId(const AtomicString& id) const
{
    return m_features.hasSelectorForId(id);
}

void StyleResolver::addToStyleSharingList(Element& element)
{
    // Never add elements to the style sharing list if we're not in a recalcStyle,
    // otherwise we could leave stale pointers in there.
    if (!document().inStyleRecalc())
        return;
    INCREMENT_STYLE_STATS_COUNTER(*this, sharedStyleCandidates);
    StyleSharingList& list = styleSharingList();
    if (list.size() >= styleSharingListSize)
        list.removeLast();
    list.prepend(&element);
}

StyleSharingList& StyleResolver::styleSharingList()
{
    m_styleSharingLists.resize(styleSharingMaxDepth);

    // We never put things at depth 0 into the list since that's only the <html> element
    // and it has no siblings or cousins to share with.
    unsigned depth = std::max(std::min(m_styleSharingDepth, styleSharingMaxDepth), 1u) - 1u;

    if (!m_styleSharingLists[depth])
        m_styleSharingLists[depth] = adoptPtrWillBeNoop(new StyleSharingList);
    return *m_styleSharingLists[depth];
}

void StyleResolver::clearStyleSharingList()
{
    m_styleSharingLists.resize(0);
}

void StyleResolver::pushParentElement(Element& parent)
{
    const ContainerNode* parentsParent = parent.parentOrShadowHostElement();

    // We are not always invoked consistently. For example, script execution can cause us to enter
    // style recalc in the middle of tree building. We may also be invoked from somewhere within the tree.
    // Reset the stack in this case, or if we see a new root element.
    // Otherwise just push the new parent.
    if (!parentsParent || m_selectorFilter.parentStackIsEmpty())
        m_selectorFilter.setupParentStack(parent);
    else
        m_selectorFilter.pushParent(parent);
}

void StyleResolver::popParentElement(Element& parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(&parent))
        m_selectorFilter.popParent();
}

StyleResolver::~StyleResolver()
{
}

void StyleResolver::matchAuthorRulesForShadowHost(Element* element, ElementRuleCollector& collector, bool includeEmptyRules, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolversInShadowTree)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    CascadeScope cascadeScope = 0;
    CascadeOrder cascadeOrder = 0;

    for (int j = resolversInShadowTree.size() - 1; j >= 0; --j)
        resolversInShadowTree.at(j)->collectMatchingAuthorRules(collector, includeEmptyRules, cascadeScope, cascadeOrder++);

    if (resolvers.isEmpty() || resolvers.first()->treeScope() != element->treeScope())
        ++cascadeScope;
    cascadeOrder += resolvers.size();
    for (unsigned i = 0; i < resolvers.size(); ++i)
        resolvers.at(i)->collectMatchingAuthorRules(collector, includeEmptyRules, cascadeScope++, --cascadeOrder);

    m_treeBoundaryCrossingRules.collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAuthorRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    if (document().styleEngine()->onlyDocumentHasStyles()) {
        ScopedStyleResolver* resolver = document().scopedStyleResolver();
        if (!resolver)
            return;
        // If we have no resolver for a document, the document has no styles.
        // We don't need to see any rules (including treeboundary crossing ones).
        resolver->collectMatchingAuthorRules(collector, includeEmptyRules, ignoreCascadeScope);
        m_treeBoundaryCrossingRules.collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
        collector.sortAndTransferMatchedRules();
        return;
    }

    WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8> resolvers;
    resolveScopedStyles(element, resolvers);

    WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8> resolversInShadowTree;
    collectScopedResolversForHostedShadowTrees(element, resolversInShadowTree);
    if (!resolversInShadowTree.isEmpty()) {
        matchAuthorRulesForShadowHost(element, collector, includeEmptyRules, resolvers, resolversInShadowTree);
        return;
    }

    if (!resolvers.isEmpty()) {
        CascadeScope cascadeScope = 0;
        CascadeOrder cascadeOrder = resolvers.size();
        for (unsigned i = 0; i < resolvers.size(); ++i, --cascadeOrder) {
            ScopedStyleResolver* resolver = resolvers.at(i);
            // FIXME: Need to clarify how to treat style scoped.
            resolver->collectMatchingAuthorRules(collector, includeEmptyRules, cascadeScope++, resolver->treeScope() == element->treeScope() && resolver->treeScope().rootNode().isShadowRoot() ? 0 : cascadeOrder);
        }
    }

    m_treeBoundaryCrossingRules.collectTreeBoundaryCrossingRules(element, collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector)
{
    collector.setMatchingUARules(true);

    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    RuleSet* userAgentStyleSheet = m_printMediaType ? defaultStyleSheets.defaultPrintStyle() : defaultStyleSheets.defaultStyle();
    matchRuleSet(collector, userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (document().inQuirksMode())
        matchRuleSet(collector, defaultStyleSheets.defaultQuirksStyle());

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document().isViewSource())
        matchRuleSet(collector, defaultStyleSheets.defaultViewSourceStyle());

    if (document().isTransitionDocument())
        matchRuleSet(collector, defaultStyleSheets.defaultTransitionStyle());

    collector.setMatchingUARules(false);
}

void StyleResolver::matchRuleSet(ElementRuleCollector& collector, RuleSet* rules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUARule = collector.matchedResult().matchedProperties.size() - 1;

    RuleRange ruleRange = collector.matchedResult().ranges.UARuleRange();
    collector.collectMatchingRules(MatchRequest(rules), ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAllRules(StyleResolverState& state, ElementRuleCollector& collector, bool includeSMILProperties)
{
    matchUARules(collector);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (state.element()->isStyledElement()) {
        collector.addElementStyleProperties(state.element()->presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        collector.addElementStyleProperties(state.element()->additionalPresentationAttributeStyle());

        if (state.element()->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(state.element())->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }

    matchAuthorRules(state.element(), collector, false);

    if (state.element()->isStyledElement()) {
        if (state.element()->inlineStyle()) {
            // Inline style is immutable as long as there is no CSSOM wrapper.
            bool isInlineStyleCacheable = !state.element()->inlineStyle()->isMutable();
            collector.addElementStyleProperties(state.element()->inlineStyle(), isInlineStyleCacheable);
        }

        // Now check SMIL animation override style.
        if (includeSMILProperties && state.element()->isSVGElement())
            collector.addElementStyleProperties(toSVGElement(state.element())->animatedSMILStyleProperties(), false /* isCacheable */);
    }
}

PassRefPtr<RenderStyle> StyleResolver::styleForDocument(Document& document)
{
    const LocalFrame* frame = document.frame();

    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    documentStyle->setDisplay(BLOCK);
    documentStyle->setRTLOrdering(document.visuallyOrdered() ? VisualOrder : LogicalOrder);
    documentStyle->setZoom(frame && !document.printing() ? frame->pageZoomFactor() : 1);
    documentStyle->setLocale(document.contentLanguage());
    documentStyle->setZIndex(0);
    documentStyle->setUserModify(document.inDesignMode() ? READ_WRITE : READ_ONLY);

    document.setupFontBuilder(documentStyle.get());

    return documentStyle.release();
}

static inline void resetDirectionAndWritingModeOnDocument(Document& document)
{
    document.setDirectionSetOnDocumentElement(false);
    document.setWritingModeSetOnDocumentElement(false);
}

static void addContentAttrValuesToFeatures(const Vector<AtomicString>& contentAttrValues, RuleFeatureSet& features)
{
    for (size_t i = 0; i < contentAttrValues.size(); ++i)
        features.addContentAttr(contentAttrValues[i]);
}

void StyleResolver::adjustRenderStyle(StyleResolverState& state, Element* element)
{
    StyleAdjuster adjuster(document().inQuirksMode());
    adjuster.adjustRenderStyle(state.style(), state.parentStyle(), element, state.cachedUAStyle());
}

// Start loading resources referenced by this style.
void StyleResolver::loadPendingResources(StyleResolverState& state)
{
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());
    document().styleEngine()->fontSelector()->fontLoader()->loadPendingFonts();
}

PassRefPtr<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent, StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(!hasPendingAuthorStyleSheets());
    ASSERT(!m_needCollectFeatures);

    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !document().isRenderingReady() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(document().styleEngine()->fontSelector());
        }

        document().setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    didAccess();

    StyleResolverParentScope::ensureParentStackIsPushed();

    if (element == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), element, defaultParent);

    if (sharingBehavior == AllowStyleSharing && state.parentStyle()) {
        SharedStyleFinder styleFinder(state.elementContext(), m_features, m_siblingRuleSet.get(), m_uncommonAttributeRuleSet.get(), *this);
        if (RefPtr<RenderStyle> sharedStyle = styleFinder.findSharedStyle())
            return sharedStyle.release();
    }

    ActiveAnimations* activeAnimations = element->activeAnimations();
    const RenderStyle* baseRenderStyle = activeAnimations ? activeAnimations->baseRenderStyle() : nullptr;

    if (baseRenderStyle) {
        state.setStyle(RenderStyle::clone(baseRenderStyle));
        if (!state.parentStyle())
            state.setParentStyle(defaultStyleForElement());
    } else {
        if (state.parentStyle()) {
            state.setStyle(RenderStyle::create());
            state.style()->inheritFrom(state.parentStyle(), isAtShadowBoundary(element) ? RenderStyle::AtShadowBoundary : RenderStyle::NotAtShadowBoundary);
        } else {
            state.setStyle(defaultStyleForElement());
            state.setParentStyle(RenderStyle::clone(state.style()));
        }
    }

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (state.distributedToInsertionPoint()) {
        if (Element* parent = element->parentElement()) {
            if (RenderStyle* styleOfShadowHost = parent->renderStyle())
                state.style()->setUserModify(styleOfShadowHost->userModify());
        }
    }

    if (element->isLink()) {
        state.style()->setIsLink(true);
        EInsideLink linkState = state.elementLinkState();
        if (linkState != NotInsideLink) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoVisited);
            if (forceVisited)
                linkState = InsideVisitedLink;
        }
        state.style()->setInsideLink(linkState);
    }

    if (!baseRenderStyle) {

        bool needsCollection = false;
        CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetsForElement(element, needsCollection);
        if (needsCollection)
            collectFeatures();

        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());

        matchAllRules(state, collector, matchingBehavior != MatchAllRulesExcludingSMIL);

        applyMatchedProperties(state, collector.matchedResult());
        applyCallbackSelectors(state);

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);

        // Cache our original display.
        state.style()->setOriginalDisplay(state.style()->display());

        adjustRenderStyle(state, element);

        if (activeAnimations)
            activeAnimations->updateBaseRenderStyle(state.style());
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    if (applyAnimatedProperties(state, element))
        adjustRenderStyle(state, element);

    if (isHTMLBodyElement(*element))
        document().textLinkColors().setTextColor(state.style()->color());

    setAnimationUpdateIfNeeded(state, *element);

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForKeyframe(Element& element, const RenderStyle& elementStyle, RenderStyle* parentStyle, const StyleKeyframe* keyframe, const AtomicString& animationName)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(!hasPendingAuthorStyleSheets());

    if (&element == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), &element, parentStyle);

    MatchResult result;
    result.addMatchedProperties(&keyframe->properties());

    ASSERT(!state.style());

    // Create the style
    state.setStyle(RenderStyle::clone(&elementStyle));

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    // We also don't need to bother with animation properties since the only
    // relevant one is animation-timing-function and we special-case that in
    // CSSAnimations.cpp
    bool inheritedOnly = false;
    applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Now do rest of the properties.
    applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    loadPendingResources(state);

    didAccess();

    return state.takeStyle();
}

// This function is used by the WebAnimations JavaScript API method animate().
// FIXME: Remove this when animate() switches away from resolution-dependent parsing.
PassRefPtrWillBeRawPtr<AnimatableValue> StyleResolver::createAnimatableValueSnapshot(Element& element, CSSPropertyID property, CSSValue& value)
{
    RefPtr<RenderStyle> style;
    if (element.renderStyle())
        style = RenderStyle::clone(element.renderStyle());
    else
        style = RenderStyle::create();
    StyleResolverState state(element.document(), &element);
    state.setStyle(style);
    return createAnimatableValueSnapshot(state, property, value);
}

PassRefPtrWillBeRawPtr<AnimatableValue> StyleResolver::createAnimatableValueSnapshot(StyleResolverState& state, CSSPropertyID property, CSSValue& value)
{
    StyleBuilder::applyProperty(property, state, &value);
    return CSSAnimatableValueFactory::create(property, *state.style());
}

PassRefPtrWillBeRawPtr<PseudoElement> StyleResolver::createPseudoElementIfNeeded(Element& parent, PseudoId pseudoId)
{
    RenderObject* parentRenderer = parent.renderer();
    if (!parentRenderer)
        return nullptr;

    if (pseudoId < FIRST_INTERNAL_PSEUDOID && !parentRenderer->style()->hasPseudoStyle(pseudoId))
        return nullptr;

    if (pseudoId == BACKDROP && !parent.isInTopLayer())
        return nullptr;

    if (!parentRenderer->canHaveGeneratedChildren())
        return nullptr;

    RenderStyle* parentStyle = parentRenderer->style();
    if (RenderStyle* cachedStyle = parentStyle->getCachedPseudoStyle(pseudoId)) {
        if (!pseudoElementRendererIsNeeded(cachedStyle))
            return nullptr;
        return PseudoElement::create(&parent, pseudoId);
    }

    StyleResolverState state(document(), &parent, parentStyle);
    if (!pseudoStyleForElementInternal(parent, pseudoId, parentStyle, state))
        return nullptr;
    RefPtr<RenderStyle> style = state.takeStyle();
    ASSERT(style);
    parentStyle->addCachedPseudoStyle(style);

    if (!pseudoElementRendererIsNeeded(style.get()))
        return nullptr;

    RefPtrWillBeRawPtr<PseudoElement> pseudo = PseudoElement::create(&parent, pseudoId);

    setAnimationUpdateIfNeeded(state, *pseudo);
    if (ActiveAnimations* activeAnimations = pseudo->activeAnimations())
        activeAnimations->cssAnimations().maybeApplyPendingUpdate(pseudo.get());
    return pseudo.release();
}

bool StyleResolver::pseudoStyleForElementInternal(Element& element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle, StyleResolverState& state)
{
    ASSERT(document().frame());
    ASSERT(document().settings());
    ASSERT(pseudoStyleRequest.pseudoId != FIRST_LINE_INHERITED);
    ASSERT(state.parentStyle());

    StyleResolverParentScope::ensureParentStackIsPushed();

    Element* pseudoElement = element.pseudoElement(pseudoStyleRequest.pseudoId);

    ActiveAnimations* activeAnimations = pseudoElement ? pseudoElement->activeAnimations() : nullptr;
    const RenderStyle* baseRenderStyle = activeAnimations ? activeAnimations->baseRenderStyle() : nullptr;

    if (baseRenderStyle) {
        state.setStyle(RenderStyle::clone(baseRenderStyle));
    } else if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    // Since we don't use pseudo-elements in any of our quirk/print
    // user agent rules, don't waste time walking those rules.

    if (!baseRenderStyle) {
        // Check UA, user and author rules.
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
        collector.setPseudoStyleRequest(pseudoStyleRequest);

        matchUARules(collector);
        matchAuthorRules(state.element(), collector, false);

        if (collector.matchedResult().matchedProperties.isEmpty())
            return false;

        applyMatchedProperties(state, collector.matchedResult());
        applyCallbackSelectors(state);

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);

        // Cache our original display.
        state.style()->setOriginalDisplay(state.style()->display());

        // FIXME: Passing 0 as the Element* introduces a lot of complexity
        // in the adjustRenderStyle code.
        adjustRenderStyle(state, 0);

        if (activeAnimations)
            activeAnimations->updateBaseRenderStyle(state.style());
    }

    // FIXME: The CSSWG wants to specify that the effects of animations are applied before
    // important rules, but this currently happens here as we require adjustment to have happened
    // before deciding which properties to transition.
    if (applyAnimatedProperties(state, pseudoElement))
        adjustRenderStyle(state, 0);

    didAccess();

    if (state.style()->hasViewportUnits())
        document().setHasViewportUnits();

    return true;
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!element)
        return nullptr;

    StyleResolverState state(document(), element, parentStyle);
    if (!pseudoStyleForElementInternal(*element, pseudoStyleRequest, parentStyle, state)) {
        if (pseudoStyleRequest.type == PseudoStyleRequest::ForRenderer)
            return nullptr;
        return state.takeStyle();
    }

    if (PseudoElement* pseudoElement = element->pseudoElement(pseudoStyleRequest.pseudoId))
        setAnimationUpdateIfNeeded(state, *pseudoElement);

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    ASSERT(!hasPendingAuthorStyleSheets());
    resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), document().documentElement()); // m_rootElementStyle will be set to the document style.

    state.setStyle(RenderStyle::create());
    const RenderStyle* rootElementStyle = state.rootElementStyle() ? state.rootElementStyle() : document().renderStyle();
    ASSERT(rootElementStyle);
    state.style()->inheritFrom(rootElementStyle);

    PageRuleCollector collector(rootElementStyle, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::instance().defaultPrintStyle());

    if (ScopedStyleResolver* scopedResolver = document().scopedStyleResolver())
        scopedResolver->matchPageRules(collector);

    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);

    loadPendingResources(state);

    didAccess();

    // Now return the style.
    return state.takeStyle();
}

void StyleResolver::collectViewportRules()
{
    CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
    viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (!InspectorInstrumentation::applyViewportStyleOverride(&document(), this))
        viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultViewportStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (document().isMobileDocument())
        viewportStyleResolver()->collectViewportRules(defaultStyleSheets.defaultXHTMLMobileProfileStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (ScopedStyleResolver* scopedResolver = document().scopedStyleResolver())
        scopedResolver->collectViewportRulesTo(this);

    viewportStyleResolver()->resolve();
}

PassRefPtr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    StyleResolverState state(document(), 0);
    state.setStyle(RenderStyle::create());
    state.fontBuilder().setInitial(state.style()->effectiveZoom());
    state.style()->font().update(document().styleEngine()->fontSelector());
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    Node* parentNode = NodeRenderingTraversal::parent(textNode);
    if (!parentNode || !parentNode->renderStyle())
        return defaultStyleForElement();
    return parentNode->renderStyle();
}

void StyleResolver::updateFont(StyleResolverState& state)
{
    state.fontBuilder().createFont(document().styleEngine()->fontSelector(), state.parentStyle(), state.style());
}

PassRefPtrWillBeRawPtr<StyleRuleList> StyleResolver::styleRulesForElement(Element* element, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);
    collectPseudoRulesForElement(element, collector, NOPSEUDO, rulesToInclude);
    return collector.matchedStyleRuleList();
}

PassRefPtrWillBeRawPtr<CSSRuleList> StyleResolver::pseudoCSSRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingCSSRules);
    collectPseudoRulesForElement(element, collector, pseudoId, rulesToInclude);
    return collector.matchedCSSRuleList();
}

PassRefPtrWillBeRawPtr<CSSRuleList> StyleResolver::cssRulesForElement(Element* element, unsigned rulesToInclude)
{
    return pseudoCSSRulesForElement(element, NOPSEUDO, rulesToInclude);
}

void StyleResolver::collectPseudoRulesForElement(Element* element, ElementRuleCollector& collector, PseudoId pseudoId, unsigned rulesToInclude)
{
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));

    if (rulesToInclude & UAAndUserCSSRules)
        matchUARules(collector);

    if (rulesToInclude & AuthorCSSRules) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));
        matchAuthorRules(element, collector, rulesToInclude & EmptyCSSRules);
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

bool StyleResolver::applyAnimatedProperties(StyleResolverState& state, const Element* animatingElement)
{
    Element* element = state.element();
    ASSERT(element);

    // The animating element may be this element, or its pseudo element. It is
    // null when calculating the style for a potential pseudo element that has
    // yet to be created.
    ASSERT(animatingElement == element || !animatingElement || animatingElement->parentOrShadowHostElement() == element);

    if (!(animatingElement && animatingElement->hasActiveAnimations())
        && !state.style()->transitions() && !state.style()->animations())
        return false;

    state.setAnimationUpdate(CSSAnimations::calculateUpdate(animatingElement, *element, *state.style(), state.parentStyle(), this));
    if (!state.animationUpdate())
        return false;

    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >& activeInterpolationsForAnimations = state.animationUpdate()->activeInterpolationsForAnimations();
    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >& activeInterpolationsForTransitions = state.animationUpdate()->activeInterpolationsForTransitions();
    applyAnimatedProperties<HighPriorityProperties>(state, activeInterpolationsForAnimations);
    applyAnimatedProperties<HighPriorityProperties>(state, activeInterpolationsForTransitions);

    updateFont(state);

    applyAnimatedProperties<LowPriorityProperties>(state, activeInterpolationsForAnimations);
    applyAnimatedProperties<LowPriorityProperties>(state, activeInterpolationsForTransitions);

    // Start loading resources used by animations.
    loadPendingResources(state);

    ASSERT(!state.fontBuilder().fontDirty());

    return true;
}

static inline ScopedStyleResolver* scopedResolverFor(const Element* element)
{
    for (TreeScope* treeScope = &element->treeScope(); treeScope; treeScope = treeScope->parentTreeScope()) {
        if (ScopedStyleResolver* scopedStyleResolver = treeScope->scopedStyleResolver())
            return scopedStyleResolver;
    }
    return 0;
}

void StyleResolver::resolveScopedStyles(const Element* element, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers)
{
    for (ScopedStyleResolver* scopedResolver = scopedResolverFor(element); scopedResolver; scopedResolver = scopedResolver->parent())
        resolvers.append(scopedResolver);
}

void StyleResolver::collectScopedResolversForHostedShadowTrees(const Element* element, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers)
{
    ElementShadow* shadow = element->shadow();
    if (!shadow)
        return;

    // Adding scoped resolver for active shadow roots for shadow host styling.
    for (ShadowRoot* shadowRoot = shadow->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->numberOfStyles() > 0) {
            if (ScopedStyleResolver* resolver = shadowRoot->scopedStyleResolver())
                resolvers.append(resolver);
        }
    }
}

void StyleResolver::styleTreeResolveScopedKeyframesRules(const Element* element, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers)
{
    TreeScope& treeScope = element->treeScope();

    // Add resolvers for shadow roots hosted by the given element.
    collectScopedResolversForHostedShadowTrees(element, resolvers);

    // Add resolvers while walking up DOM tree from the given element.
    for (ScopedStyleResolver* scopedResolver = scopedResolverFor(element); scopedResolver; scopedResolver = scopedResolver->parent()) {
        if (scopedResolver->treeScope() == treeScope)
            resolvers.append(scopedResolver);
    }
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyAnimatedProperties(StyleResolverState& state, const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >& activeInterpolations)
{
    for (const auto& interpolationEntry : activeInterpolations) {
        CSSPropertyID property = interpolationEntry.key;
        if (!isPropertyForPass<pass>(property))
            continue;
        const StyleInterpolation* interpolation = toStyleInterpolation(interpolationEntry.value.get());
        interpolation->apply(state);
    }
}

static inline bool isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    // FIXME: 'text-decoration' shorthand to be handled when available.
    // See https://chromiumcodereview.appspot.com/19516002 for details.
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
        return RuntimeEnabledFeatures::css3TextDecorationsEnabled();
    default:
        break;
    }
    return false;
}

static inline bool isValidFirstLetterStyleProperty(CSSPropertyID id)
{
    switch (id) {
    // Valid ::first-letter properties listed in spec:
    // http://www.w3.org/TR/css3-selectors/#application-in-css
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColor:
    case CSSPropertyFloat:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontWeight:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyTextTransform:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWordSpacing:
        return true;
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
        return RuntimeEnabledFeatures::css3TextDecorationsEnabled();

    // text-shadow added in text decoration spec:
    // http://www.w3.org/TR/css-text-decor-3/#text-shadow-property
    case CSSPropertyTextShadow:
    // box-shadox added in CSS3 backgrounds spec:
    // http://www.w3.org/TR/css3-background/#placement
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
    // Properties that we currently support outside of spec.
    case CSSPropertyWebkitLineBoxContain:
    case CSSPropertyVisibility:
        return true;

    default:
        return false;
    }
}

// FIXME: Consider refactoring to create a new class which owns the following
// first/last/range properties.
// This method returns the first CSSPropertyId of high priority properties.
// Other properties can depend on high priority properties. For example,
// border-color property with currentColor value depends on color property.
// All high priority properties are obtained by using
// firstCSSPropertyId<HighPriorityProperties> and
// lastCSSPropertyId<HighPriorityProperties>.
template<> CSSPropertyID StyleResolver::firstCSSPropertyId<StyleResolver::HighPriorityProperties>()
{
    COMPILE_ASSERT(CSSPropertyColor == firstCSSProperty, CSS_color_is_first_high_priority_property);
    return CSSPropertyColor;
}

// This method returns the last CSSPropertyId of high priority properties.
template<> CSSPropertyID StyleResolver::lastCSSPropertyId<StyleResolver::HighPriorityProperties>()
{
    COMPILE_ASSERT(CSSPropertyZoom == CSSPropertyColor + 16, CSS_zoom_is_end_of_high_priority_property_range);
    COMPILE_ASSERT(CSSPropertyTextRendering == CSSPropertyZoom - 1, CSS_text_rendering_is_before_zoom);
    return CSSPropertyZoom;
}

// This method returns the first CSSPropertyId of remaining properties,
// i.e. low priority properties. No properties depend on low priority
// properties. So we don't need to resolve such properties quickly.
// All low priority properties are obtained by using
// firstCSSPropertyId<LowPriorityProperties> and
// lastCSSPropertyId<LowPriorityProperties>.
template<> CSSPropertyID StyleResolver::firstCSSPropertyId<StyleResolver::LowPriorityProperties>()
{
    COMPILE_ASSERT(CSSPropertyAlignContent == CSSPropertyZoom + 1, CSS_align_content_is_first_low_priority_property);
    return CSSPropertyAlignContent;
}

// This method returns the last CSSPropertyId of low priority properties.
template<> CSSPropertyID StyleResolver::lastCSSPropertyId<StyleResolver::LowPriorityProperties>()
{
    return static_cast<CSSPropertyID>(lastCSSProperty);
}

template <StyleResolver::StyleApplicationPass pass>
bool StyleResolver::isPropertyForPass(CSSPropertyID property)
{
    return firstCSSPropertyId<pass>() <= property && property <= lastCSSPropertyId<pass>();
}

// This method expands the 'all' shorthand property to longhand properties
// and applies the expanded longhand properties.
template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyAllProperty(StyleResolverState& state, CSSValue* allValue)
{
    bool isUnsetValue = !allValue->isInitialValue() && !allValue->isInheritedValue();
    unsigned startCSSProperty = firstCSSPropertyId<pass>();
    unsigned endCSSProperty = lastCSSPropertyId<pass>();

    for (unsigned i = startCSSProperty; i <= endCSSProperty; ++i) {
        CSSPropertyID propertyId = static_cast<CSSPropertyID>(i);

        // StyleBuilder does not allow any expanded shorthands.
        if (isShorthandProperty(propertyId))
            continue;

        // all shorthand spec says:
        // The all property is a shorthand that resets all CSS properties
        // except direction and unicode-bidi.
        // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
        // We skip applyProperty when a given property is unicode-bidi or
        // direction.
        if (!CSSProperty::isAffectedByAllProperty(propertyId))
            continue;

        CSSValue* value;
        if (!isUnsetValue) {
            value = allValue;
        } else {
            if (CSSPropertyMetadata::isInheritedProperty(propertyId))
                value = cssValuePool().createInheritedValue().get();
            else
                value = cssValuePool().createExplicitInitialValue().get();
        }
        StyleBuilder::applyProperty(propertyId, state, value);
    }
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyProperties(StyleResolverState& state, const StylePropertySet* properties, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    unsigned propertyCount = properties->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        StylePropertySet::PropertyReference current = properties->propertyAt(i);
        if (isImportant != current.isImportant())
            continue;

        CSSPropertyID property = current.id();
        if (property == CSSPropertyAll) {
            applyAllProperty<pass>(state, current.value());
            continue;
        }

        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(property))
            continue;
        if (propertyWhitelistType == PropertyWhitelistFirstLetter && !isValidFirstLetterStyleProperty(property))
            continue;

        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }

        if (!isPropertyForPass<pass>(property))
            continue;

        StyleBuilder::applyProperty(current.id(), state, current.value());
    }
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.m_types.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<pass>(state, matchedProperties.properties.get(), isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.m_types.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<pass>(state, matchedProperties.properties.get(), isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.m_types.whitelistType));
    }
}

static unsigned computeMatchedPropertiesHash(const MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

void StyleResolver::notifyResizeForViewportUnits()
{
    collectViewportRules();
    m_matchedPropertiesCache.clearViewportDependent();
}

void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult)
{
    const Element* element = state.element();
    ASSERT(element);

    INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyApply);

    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const CachedMatchedProperties* cachedMatchedProperties = cacheHash ? m_matchedPropertiesCache.find(cacheHash, state, matchResult) : 0;

    if (cachedMatchedProperties && MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheHit);
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cachedMatchedProperties->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cachedMatchedProperties->parentRenderStyle.get()) && !isAtShadowBoundary(element)
            && (!state.distributedToInsertionPoint() || state.style()->userModify() == READ_ONLY)) {
            INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheInheritedHit);

            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cachedMatchedProperties->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true;
    }

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (UNLIKELY(isSVGForeignObjectElement(element))) {
        // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should not be scaled again.
        //
        // FIXME: The following hijacks the zoom property for foreignObject so that children of foreignObject get the
        // correct font-size in case of zooming. 'zoom' is part of HighPriorityProperties, along with other font-related
        // properties used as input to the FontBuilder, so resetting it here may cause the FontBuilder to recompute the
        // font used as inheritable font for foreignObject content. If we want to support zoom on foreignObject we'll
        // need to find another way of handling the SVG zoom model.
        state.setEffectiveZoom(RenderStyle::initialZoom());
    }

    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.fontBuilder().setFontDirty(true);
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache the UA properties to pass them to RenderTheme in adjustRenderStyle.
    state.cacheUserAgentBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    loadPendingResources(state);

    if (!cachedMatchedProperties && cacheHash && MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle())) {
        INCREMENT_STYLE_STATS_COUNTER(*this, matchedPropertyCacheAdded);
        m_matchedPropertiesCache.add(state.style(), state.parentStyle(), cacheHash, matchResult);
    }

    ASSERT(!state.fontBuilder().fontDirty());
}

void StyleResolver::applyCallbackSelectors(StyleResolverState& state)
{
    if (!m_watchedSelectorsRules)
        return;

    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);

    MatchRequest matchRequest(m_watchedSelectorsRules.get(), true);
    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange);
    collector.sortAndTransferMatchedRules();

    RefPtrWillBeRawPtr<StyleRuleList> rules = collector.matchedStyleRuleList();
    if (!rules)
        return;
    for (size_t i = 0; i < rules->m_list.size(); i++)
        state.style()->addCallbackSelector(rules->m_list[i]->selectorList().selectorsText());
}

CSSPropertyValue::CSSPropertyValue(CSSPropertyID id, const StylePropertySet& propertySet)
    : property(id), value(propertySet.getPropertyCSSValue(id).get())
{ }

void StyleResolver::enableStats(StatsReportType reportType)
{
    if (m_styleResolverStats)
        return;
    m_styleResolverStats = StyleResolverStats::create();
    m_styleResolverStatsTotals = StyleResolverStats::create();
    if (reportType == ReportSlowStats) {
        m_styleResolverStats->printMissedCandidateCount = true;
        m_styleResolverStatsTotals->printMissedCandidateCount = true;
    }
}

void StyleResolver::disableStats()
{
    m_styleResolverStatsSequence = 0;
    m_styleResolverStats.clear();
    m_styleResolverStatsTotals.clear();
}

void StyleResolver::printStats()
{
    if (!m_styleResolverStats)
        return;
    fprintf(stderr, "=== Style Resolver Stats (resolve #%u) (%s) ===\n", ++m_styleResolverStatsSequence, document().url().string().utf8().data());
    fprintf(stderr, "%s\n", m_styleResolverStats->report().utf8().data());
    fprintf(stderr, "== Totals ==\n");
    fprintf(stderr, "%s\n", m_styleResolverStatsTotals->report().utf8().data());
}

void StyleResolver::applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle* style)
{
    StyleResolverState state(document(), document().documentElement(), style);
    state.setStyle(style);

    for (size_t i = 0; i < count; ++i) {
        if (properties[i].value) {
            // As described in BUG66291, setting font-size and line-height on a font may entail a CSSPrimitiveValue::computeLengthDouble call,
            // which assumes the fontMetrics are available for the affected font, otherwise a crash occurs (see http://trac.webkit.org/changeset/96122).
            // The updateFont() call below updates the fontMetrics and ensure the proper setting of font-size and line-height.
            switch (properties[i].property) {
            case CSSPropertyFontSize:
            case CSSPropertyLineHeight:
                updateFont(state);
                break;
            default:
                break;
            }
            StyleBuilder::applyProperty(properties[i].property, state, properties[i].value);
        }
    }
}

void StyleResolver::addMediaQueryResults(const MediaQueryResultList& list)
{
    for (size_t i = 0; i < list.size(); ++i)
        m_viewportDependentMediaQueryResults.append(list[i]);
}

bool StyleResolver::mediaQueryAffectedByViewportChange() const
{
    for (unsigned i = 0; i < m_viewportDependentMediaQueryResults.size(); ++i) {
        if (m_medium->eval(m_viewportDependentMediaQueryResults[i]->expression()) != m_viewportDependentMediaQueryResults[i]->result())
            return true;
    }
    return false;
}

void StyleResolver::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_keyframesRuleMap);
    visitor->trace(m_matchedPropertiesCache);
    visitor->trace(m_viewportDependentMediaQueryResults);
    visitor->trace(m_selectorFilter);
    visitor->trace(m_viewportStyleResolver);
    visitor->trace(m_features);
    visitor->trace(m_siblingRuleSet);
    visitor->trace(m_uncommonAttributeRuleSet);
    visitor->trace(m_watchedSelectorsRules);
    visitor->trace(m_treeBoundaryCrossingRules);
    visitor->trace(m_styleSharingLists);
    visitor->trace(m_pendingStyleSheets);
    visitor->trace(m_document);
#endif
}

} // namespace blink
