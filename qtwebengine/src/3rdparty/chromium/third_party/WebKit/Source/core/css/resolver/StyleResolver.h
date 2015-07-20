/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef StyleResolver_h
#define StyleResolver_h

#include "core/css/PseudoStyleRequest.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/css/TreeBoundaryCrossingRules.h"
#include "core/css/resolver/MatchedPropertiesCache.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResourceLoader.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class AnimatableValue;
class CSSRuleList;
class CSSStyleSheet;
class CSSValue;
class ContainerNode;
class Document;
class Element;
class ElementRuleCollector;
class Interpolation;
class MediaQueryEvaluator;
class RuleData;
class StyleKeyframe;
class StylePropertySet;
class StyleResolverStats;
class StyleRule;
class StyleRuleKeyframes;
class StyleRulePage;
class ViewportStyleResolver;

class MatchResult;

enum StyleSharingBehavior {
    AllowStyleSharing,
    DisallowStyleSharing,
};

enum RuleMatchingBehavior {
    MatchAllRules,
    MatchAllRulesExcludingSMIL
};

const unsigned styleSharingListSize = 15;
const unsigned styleSharingMaxDepth = 32;
typedef WillBeHeapDeque<RawPtrWillBeMember<Element>, styleSharingListSize> StyleSharingList;

struct CSSPropertyValue {
    STACK_ALLOCATED();
public:
    CSSPropertyValue(CSSPropertyID property, CSSValue* value)
        : property(property), value(value) { }
    // Stores value=propertySet.getPropertyCSSValue(id).get().
    CSSPropertyValue(CSSPropertyID, const StylePropertySet&);
    CSSPropertyID property;
    RawPtrWillBeMember<CSSValue> value;
};

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class StyleResolver final : public NoBaseWillBeGarbageCollectedFinalized<StyleResolver> {
    WTF_MAKE_NONCOPYABLE(StyleResolver); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    explicit StyleResolver(Document&);
    virtual ~StyleResolver();

    // FIXME: StyleResolver should not be keeping tree-walk state.
    // These should move to some global tree-walk state, or should be contained in a
    // TreeWalkContext or similar which is passed in to StyleResolver methods when available.
    // Using these during tree walk will allow style selector to optimize child and descendant selector lookups.
    void pushParentElement(Element&);
    void popParentElement(Element&);

    PassRefPtr<RenderStyle> styleForElement(Element*, RenderStyle* parentStyle = 0, StyleSharingBehavior = AllowStyleSharing,
        RuleMatchingBehavior = MatchAllRules);

    PassRefPtr<RenderStyle> styleForKeyframe(Element&, const RenderStyle&, RenderStyle* parentStyle, const StyleKeyframe*, const AtomicString& animationName);
    static PassRefPtrWillBeRawPtr<AnimatableValue> createAnimatableValueSnapshot(Element&, CSSPropertyID, CSSValue&);
    static PassRefPtrWillBeRawPtr<AnimatableValue> createAnimatableValueSnapshot(StyleResolverState&, CSSPropertyID, CSSValue&);

    PassRefPtr<RenderStyle> pseudoStyleForElement(Element*, const PseudoStyleRequest&, RenderStyle* parentStyle);

    PassRefPtr<RenderStyle> styleForPage(int pageIndex);
    PassRefPtr<RenderStyle> defaultStyleForElement();
    PassRefPtr<RenderStyle> styleForText(Text*);

    static PassRefPtr<RenderStyle> styleForDocument(Document&);

    // FIXME: It could be better to call appendAuthorStyleSheets() directly after we factor StyleResolver further.
    // https://bugs.webkit.org/show_bug.cgi?id=108890
    void appendAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >&);
    void resetAuthorStyle(TreeScope&);
    void finishAppendAuthorStyleSheets();

    void processScopedRules(const RuleSet& authorRules, CSSStyleSheet*, unsigned sheetIndex, ContainerNode& scope);

    void lazyAppendAuthorStyleSheets(unsigned firstNew, const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >&);
    void removePendingAuthorStyleSheets(const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >&);
    void appendPendingAuthorStyleSheets();
    bool hasPendingAuthorStyleSheets() const { return m_pendingStyleSheets.size() > 0 || m_needCollectFeatures; }

    SelectorFilter& selectorFilter() { return m_selectorFilter; }

    void styleTreeResolveScopedKeyframesRules(const Element*, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>&);

    // These methods will give back the set of rules that matched for a given element (or a pseudo-element).
    enum CSSRuleFilter {
        UAAndUserCSSRules   = 1 << 1,
        AuthorCSSRules      = 1 << 2,
        EmptyCSSRules       = 1 << 3,
        CrossOriginCSSRules = 1 << 4,
        AllButEmptyCSSRules = UAAndUserCSSRules | AuthorCSSRules | CrossOriginCSSRules,
        AllCSSRules         = AllButEmptyCSSRules | EmptyCSSRules,
    };
    PassRefPtrWillBeRawPtr<CSSRuleList> cssRulesForElement(Element*, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtrWillBeRawPtr<CSSRuleList> pseudoCSSRulesForElement(Element*, PseudoId, unsigned rulesToInclude = AllButEmptyCSSRules);
    PassRefPtrWillBeRawPtr<StyleRuleList> styleRulesForElement(Element*, unsigned rulesToInclude);

    // |properties| is an array with |count| elements.
    void applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle*);

    ViewportStyleResolver* viewportStyleResolver() { return m_viewportStyleResolver.get(); }

    void addMediaQueryResults(const MediaQueryResultList&);
    MediaQueryResultList* viewportDependentMediaQueryResults() { return &m_viewportDependentMediaQueryResults; }
    bool hasViewportDependentMediaQueries() const { return !m_viewportDependentMediaQueryResults.isEmpty(); }
    bool mediaQueryAffectedByViewportChange() const;

    // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
    void invalidateMatchedPropertiesCache();

    void notifyResizeForViewportUnits();

    // Exposed for RenderStyle::isStyleAvilable().
    static RenderStyle* styleNotYetAvailable() { return s_styleNotYetAvailable; }

    RuleFeatureSet& ensureUpdatedRuleFeatureSet()
    {
        if (hasPendingAuthorStyleSheets())
            appendPendingAuthorStyleSheets();
        return m_features;
    }

    RuleFeatureSet& ruleFeatureSet()
    {
        return m_features;
    }

    StyleSharingList& styleSharingList();

    bool hasRulesForId(const AtomicString&) const;

    void addToStyleSharingList(Element&);
    void clearStyleSharingList();

    StyleResolverStats* stats() { return m_styleResolverStats.get(); }
    StyleResolverStats* statsTotals() { return m_styleResolverStatsTotals.get(); }
    enum StatsReportType { ReportDefaultStats, ReportSlowStats };
    void enableStats(StatsReportType = ReportDefaultStats);
    void disableStats();
    void printStats();

    unsigned accessCount() const { return m_accessCount; }
    void didAccess() { ++m_accessCount; }

    void increaseStyleSharingDepth() { ++m_styleSharingDepth; }
    void decreaseStyleSharingDepth() { --m_styleSharingDepth; }

    PassRefPtrWillBeRawPtr<PseudoElement> createPseudoElementIfNeeded(Element& parent, PseudoId);

    void trace(Visitor*);

private:
    void initWatchedSelectorRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRule> >& watchedSelectors);

    // FIXME: This should probably go away, folded into FontBuilder.
    void updateFont(StyleResolverState&);

    void loadPendingResources(StyleResolverState&);
    void adjustRenderStyle(StyleResolverState&, Element*);

    void appendCSSStyleSheet(CSSStyleSheet*);
    void addRulesFromSheet(CSSStyleSheet*, TreeScope*, unsigned);

    void collectPseudoRulesForElement(Element*, ElementRuleCollector&, PseudoId, unsigned rulesToInclude);
    void matchRuleSet(ElementRuleCollector&, RuleSet*);
    void matchUARules(ElementRuleCollector&);
    void matchAuthorRules(Element*, ElementRuleCollector&, bool includeEmptyRules);
    void matchAuthorRulesForShadowHost(Element*, ElementRuleCollector&, bool includeEmptyRules, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolvers, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>& resolversInShadowTree);
    void matchAllRules(StyleResolverState&, ElementRuleCollector&, bool includeSMILProperties);
    void collectFeatures();
    void resetRuleFeatures();

    bool fastRejectSelector(const RuleData&) const;

    void applyMatchedProperties(StyleResolverState&, const MatchResult&);
    bool applyAnimatedProperties(StyleResolverState&, const Element* animatingElement);
    void applyCallbackSelectors(StyleResolverState&);

    void resolveScopedStyles(const Element*, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>&);
    void collectScopedResolversForHostedShadowTrees(const Element*, WillBeHeapVector<RawPtrWillBeMember<ScopedStyleResolver>, 8>&);

    enum StyleApplicationPass {
        HighPriorityProperties,
        LowPriorityProperties
    };
    template <StyleResolver::StyleApplicationPass pass>
    static inline CSSPropertyID firstCSSPropertyId();
    template <StyleResolver::StyleApplicationPass pass>
    static inline CSSPropertyID lastCSSPropertyId();
    template <StyleResolver::StyleApplicationPass pass>
    static inline bool isPropertyForPass(CSSPropertyID);
    template <StyleApplicationPass pass>
    void applyMatchedProperties(StyleResolverState&, const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly);
    template <StyleApplicationPass pass>
    void applyProperties(StyleResolverState&, const StylePropertySet* properties, bool isImportant, bool inheritedOnly, PropertyWhitelistType = PropertyWhitelistNone);
    template <StyleApplicationPass pass>
    void applyAnimatedProperties(StyleResolverState&, const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation> >&);
    template <StyleResolver::StyleApplicationPass pass>
    void applyAllProperty(StyleResolverState&, CSSValue*);

    void matchPageRules(MatchResult&, RuleSet*, bool isLeftPage, bool isFirstPage, const String& pageName);
    void matchPageRulesForList(WillBeHeapVector<RawPtrWillBeMember<StyleRulePage> >& matchedRules, const WillBeHeapVector<RawPtrWillBeMember<StyleRulePage> >&, bool isLeftPage, bool isFirstPage, const String& pageName);
    void collectViewportRules();

    bool isLeftPage(int pageIndex) const;
    bool isRightPage(int pageIndex) const { return !isLeftPage(pageIndex); }
    bool isFirstPage(int pageIndex) const;
    String pageName(int pageIndex) const;

    bool pseudoStyleForElementInternal(Element&, const PseudoStyleRequest&, RenderStyle* parentStyle, StyleResolverState&);

    Document& document() { return *m_document; }

    // FIXME: This likely belongs on RuleSet.
    typedef WillBeHeapHashMap<StringImpl*, RefPtrWillBeMember<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;

    static RenderStyle* s_styleNotYetAvailable;

    void cacheBorderAndBackground();

    MatchedPropertiesCache m_matchedPropertiesCache;

    OwnPtr<MediaQueryEvaluator> m_medium;
    MediaQueryResultList m_viewportDependentMediaQueryResults;

    RawPtrWillBeMember<Document> m_document;
    SelectorFilter m_selectorFilter;

    OwnPtrWillBeMember<ViewportStyleResolver> m_viewportStyleResolver;

    WillBeHeapListHashSet<RawPtrWillBeMember<CSSStyleSheet>, 16> m_pendingStyleSheets;

    // FIXME: The entire logic of collecting features on StyleResolver, as well as transferring them
    // between various parts of machinery smells wrong. This needs to be better somehow.
    RuleFeatureSet m_features;
    OwnPtrWillBeMember<RuleSet> m_siblingRuleSet;
    OwnPtrWillBeMember<RuleSet> m_uncommonAttributeRuleSet;
    OwnPtrWillBeMember<RuleSet> m_watchedSelectorsRules;
    TreeBoundaryCrossingRules m_treeBoundaryCrossingRules;

    bool m_needCollectFeatures;
    bool m_printMediaType;

    StyleResourceLoader m_styleResourceLoader;

    unsigned m_styleSharingDepth;
    WillBeHeapVector<OwnPtrWillBeMember<StyleSharingList>, styleSharingMaxDepth> m_styleSharingLists;

    OwnPtr<StyleResolverStats> m_styleResolverStats;
    OwnPtr<StyleResolverStats> m_styleResolverStatsTotals;
    unsigned m_styleResolverStatsSequence;

    // Use only for Internals::updateStyleAndReturnAffectedElementCount.
    unsigned m_accessCount;
};

} // namespace blink

#endif // StyleResolver_h
