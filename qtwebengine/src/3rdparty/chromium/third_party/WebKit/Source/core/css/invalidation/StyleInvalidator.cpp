
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/css/invalidation/DescendantInvalidationSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

void StyleInvalidator::invalidate(Document& document)
{
    if (Element* documentElement = document.documentElement())
        invalidate(*documentElement);
    document.clearChildNeedsStyleInvalidation();
    document.clearNeedsStyleInvalidation();
    clearPendingInvalidations();
}

void StyleInvalidator::scheduleInvalidation(PassRefPtrWillBeRawPtr<DescendantInvalidationSet> invalidationSet, Element& element)
{
    ASSERT(element.inActiveDocument());
    ASSERT(element.styleChangeType() < SubtreeStyleChange);
    InvalidationList& list = ensurePendingInvalidationList(element);
    // If we're already going to invalidate the whole subtree we don't need to store any new sets.
    if (!list.isEmpty() && list.last()->wholeSubtreeInvalid())
        return;
    // If this set would invalidate the whole subtree we can discard all existing sets.
    if (invalidationSet->wholeSubtreeInvalid())
        list.clear();
    list.append(invalidationSet);
    element.setNeedsStyleInvalidation();
}

StyleInvalidator::InvalidationList& StyleInvalidator::ensurePendingInvalidationList(Element& element)
{
    PendingInvalidationMap::AddResult addResult = m_pendingInvalidationMap.add(&element, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtrWillBeNoop(new InvalidationList);
    return *addResult.storedValue->value;
}

void StyleInvalidator::clearInvalidation(Node& node)
{
    if (node.isElementNode() && node.needsStyleInvalidation())
        m_pendingInvalidationMap.remove(toElement(&node));
}

void StyleInvalidator::clearPendingInvalidations()
{
    m_pendingInvalidationMap.clear();
}

StyleInvalidator::StyleInvalidator()
{
}

StyleInvalidator::~StyleInvalidator()
{
}

void StyleInvalidator::RecursionData::pushInvalidationSet(const DescendantInvalidationSet& invalidationSet)
{
    ASSERT(!m_wholeSubtreeInvalid);
    if (invalidationSet.treeBoundaryCrossing())
        m_treeBoundaryCrossing = true;
    if (invalidationSet.wholeSubtreeInvalid()) {
        m_wholeSubtreeInvalid = true;
        return;
    }
    m_invalidationSets.append(&invalidationSet);
    m_invalidateCustomPseudo = invalidationSet.customPseudoInvalid();
}

bool StyleInvalidator::RecursionData::matchesCurrentInvalidationSets(Element& element)
{
    ASSERT(!m_wholeSubtreeInvalid);

    if (m_invalidateCustomPseudo && element.shadowPseudoId() != nullAtom)
        return true;

    for (InvalidationSets::iterator it = m_invalidationSets.begin(); it != m_invalidationSets.end(); ++it) {
        if ((*it)->invalidatesElement(element))
            return true;
    }

    return false;
}

bool StyleInvalidator::checkInvalidationSetsAgainstElement(Element& element)
{
    if (element.styleChangeType() >= SubtreeStyleChange || m_recursionData.wholeSubtreeInvalid()) {
        m_recursionData.setWholeSubtreeInvalid();
        return false;
    }
    if (element.needsStyleInvalidation()) {
        if (InvalidationList* invalidationList = m_pendingInvalidationMap.get(&element)) {
            for (InvalidationList::const_iterator it = invalidationList->begin(); it != invalidationList->end(); ++it)
                m_recursionData.pushInvalidationSet(**it);
            // FIXME: It's really only necessary to clone the render style for this element, not full style recalc.
            return true;
        }
    }
    return m_recursionData.matchesCurrentInvalidationSets(element);
}

bool StyleInvalidator::invalidateChildren(Element& element)
{
    bool someChildrenNeedStyleRecalc = false;
    for (ShadowRoot* root = element.youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!m_recursionData.treeBoundaryCrossing() && !root->childNeedsStyleInvalidation() && !root->needsStyleInvalidation())
            continue;
        for (Element* child = ElementTraversal::firstChild(*root); child; child = ElementTraversal::nextSibling(*child)) {
            bool childRecalced = invalidate(*child);
            someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
        }
        root->clearChildNeedsStyleInvalidation();
        root->clearNeedsStyleInvalidation();
    }
    for (Element* child = ElementTraversal::firstChild(element); child; child = ElementTraversal::nextSibling(*child)) {
        bool childRecalced = invalidate(*child);
        someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
    }
    return someChildrenNeedStyleRecalc;
}

bool StyleInvalidator::invalidate(Element& element)
{
    RecursionCheckpoint checkpoint(&m_recursionData);

    bool thisElementNeedsStyleRecalc = checkInvalidationSetsAgainstElement(element);

    bool someChildrenNeedStyleRecalc = false;
    if (m_recursionData.hasInvalidationSets() || element.childNeedsStyleInvalidation())
        someChildrenNeedStyleRecalc = invalidateChildren(element);

    if (thisElementNeedsStyleRecalc) {
        element.setNeedsStyleRecalc(m_recursionData.wholeSubtreeInvalid() ? SubtreeStyleChange : LocalStyleChange);
    } else if (m_recursionData.hasInvalidationSets() && someChildrenNeedStyleRecalc) {
        // Clone the RenderStyle in order to preserve correct style sharing, if possible. Otherwise recalc style.
        if (RenderObject* renderer = element.renderer())
            renderer->setStyleInternal(RenderStyle::clone(renderer->style()));
        else
            element.setNeedsStyleRecalc(LocalStyleChange);
    }

    element.clearChildNeedsStyleInvalidation();
    element.clearNeedsStyleInvalidation();

    return thisElementNeedsStyleRecalc;
}

void StyleInvalidator::trace(Visitor* visitor)
{
    visitor->trace(m_pendingInvalidationMap);
}

} // namespace WebCore
