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

#ifndef RuleFeature_h
#define RuleFeature_h

#include "core/css/CSSSelector.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CSSSelectorList;
class DescendantInvalidationSet;
class QualifiedName;
class RuleData;
class SpaceSplitString;
class StyleRule;

struct RuleFeature {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    RuleFeature(StyleRule* rule, unsigned selectorIndex, bool hasDocumentSecurityOrigin);

    void trace(Visitor*);

    RawPtrWillBeMember<StyleRule> rule;
    unsigned selectorIndex;
    bool hasDocumentSecurityOrigin;
};

class RuleFeatureSet {
    DISALLOW_ALLOCATION();
public:
    RuleFeatureSet();
    ~RuleFeatureSet();

    void add(const RuleFeatureSet&);
    void clear();

    void collectFeaturesFromRuleData(const RuleData&);

    bool usesSiblingRules() const { return !siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_metadata.usesFirstLineRules; }

    unsigned maxDirectAdjacentSelectors() const { return m_metadata.maxDirectAdjacentSelectors; }
    void setMaxDirectAdjacentSelectors(unsigned value)  { m_metadata.maxDirectAdjacentSelectors = std::max(value, m_metadata.maxDirectAdjacentSelectors); }

    bool hasSelectorForAttribute(const AtomicString& attributeName) const
    {
        ASSERT(!attributeName.isEmpty());
        return m_attributeInvalidationSets.contains(attributeName);
    }

    bool hasSelectorForClass(const AtomicString& classValue) const
    {
        ASSERT(!classValue.isEmpty());
        return m_classInvalidationSets.contains(classValue);
    }

    bool hasSelectorForId(const AtomicString& idValue) const { return m_idInvalidationSets.contains(idValue); }
    bool hasSelectorForPseudoType(CSSSelector::PseudoType pseudo) const { return m_pseudoInvalidationSets.contains(pseudo); }

    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element&);
    void scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element&);
    void scheduleStyleInvalidationForAttributeChange(const QualifiedName& attributeName, Element&);
    void scheduleStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, Element&);
    void scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoType, Element&);

    bool hasIdsInSelectors() const
    {
        return m_idInvalidationSets.size() > 0;
    }

    // Marks the given attribute name as "appearing in a selector". Used for
    // CSS properties such as content: ... attr(...) ...
    // FIXME: record these internally to this class instead calls from StyleResolver to here.
    void addContentAttr(const AtomicString& attributeName);

    StyleInvalidator& styleInvalidator();

    void trace(Visitor*);

    WillBeHeapVector<RuleFeature> siblingRules;
    WillBeHeapVector<RuleFeature> uncommonAttributeRules;

protected:
    DescendantInvalidationSet* invalidationSetForSelector(const CSSSelector&);

private:
    typedef WillBeHeapHashMap<AtomicString, RefPtrWillBeMember<DescendantInvalidationSet> > InvalidationSetMap;
    typedef WillBeHeapHashMap<CSSSelector::PseudoType, RefPtrWillBeMember<DescendantInvalidationSet>, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned> > PseudoTypeInvalidationSetMap;

    struct FeatureMetadata {
        FeatureMetadata()
            : usesFirstLineRules(false)
            , foundSiblingSelector(false)
            , maxDirectAdjacentSelectors(0)
        { }
        void add(const FeatureMetadata& other);
        void clear();

        bool usesFirstLineRules;
        bool foundSiblingSelector;
        unsigned maxDirectAdjacentSelectors;
    };

    enum InvalidationSetMode {
        AddFeatures,
        UseLocalStyleChange,
        UseSubtreeStyleChange
    };

    static InvalidationSetMode invalidationSetModeForSelector(const CSSSelector&);

    void collectFeaturesFromSelector(const CSSSelector&, FeatureMetadata&, InvalidationSetMode);
    void collectFeaturesFromSelectorList(const CSSSelectorList*, FeatureMetadata&, InvalidationSetMode);

    DescendantInvalidationSet& ensureClassInvalidationSet(const AtomicString& className);
    DescendantInvalidationSet& ensureAttributeInvalidationSet(const AtomicString& attributeName);
    DescendantInvalidationSet& ensureIdInvalidationSet(const AtomicString& attributeName);
    DescendantInvalidationSet& ensurePseudoInvalidationSet(CSSSelector::PseudoType);

    InvalidationSetMode updateInvalidationSets(const CSSSelector&);

    struct InvalidationSetFeatures {
        InvalidationSetFeatures()
            : customPseudoElement(false)
            , treeBoundaryCrossing(false)
            , adjacent(false)
            , insertionPointCrossing(false)
        { }
        Vector<AtomicString> classes;
        Vector<AtomicString> attributes;
        AtomicString id;
        AtomicString tagName;
        bool customPseudoElement;
        bool treeBoundaryCrossing;
        bool adjacent;
        bool insertionPointCrossing;
    };

    static void extractInvalidationSetFeature(const CSSSelector&, InvalidationSetFeatures&);
    const CSSSelector* extractInvalidationSetFeatures(const CSSSelector&, InvalidationSetFeatures&, bool negated);

    void addFeaturesToInvalidationSet(DescendantInvalidationSet&, const InvalidationSetFeatures&);
    void addFeaturesToInvalidationSets(const CSSSelector&, InvalidationSetFeatures&);

    void addClassToInvalidationSet(const AtomicString& className, Element&);

    FeatureMetadata m_metadata;
    InvalidationSetMap m_classInvalidationSets;
    InvalidationSetMap m_attributeInvalidationSets;
    InvalidationSetMap m_idInvalidationSets;
    PseudoTypeInvalidationSetMap m_pseudoInvalidationSets;
    StyleInvalidator m_styleInvalidator;
};


} // namespace blink

#endif // RuleFeature_h
