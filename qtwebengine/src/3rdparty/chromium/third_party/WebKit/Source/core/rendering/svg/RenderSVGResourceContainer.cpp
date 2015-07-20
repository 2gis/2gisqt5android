/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "core/rendering/svg/RenderSVGResourceContainer.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"

#include "wtf/TemporaryChange.h"

namespace blink {

static inline SVGDocumentExtensions& svgExtensionsFromElement(SVGElement* element)
{
    ASSERT(element);
    return element->document().accessSVGExtensions();
}

RenderSVGResourceContainer::RenderSVGResourceContainer(SVGElement* node)
    : RenderSVGHiddenContainer(node)
    , m_isInLayout(false)
    , m_id(node->getIdAttribute())
    , m_invalidationMask(0)
    , m_registered(false)
    , m_isInvalidating(false)
{
}

RenderSVGResourceContainer::~RenderSVGResourceContainer()
{
}

void RenderSVGResourceContainer::layout()
{
    // FIXME: Investigate a way to detect and break resource layout dependency cycles early.
    // Then we can remove this method altogether, and fall back onto RenderSVGHiddenContainer::layout().
    ASSERT(needsLayout());
    if (m_isInLayout)
        return;

    TemporaryChange<bool> inLayoutChange(m_isInLayout, true);

    RenderSVGHiddenContainer::layout();

    clearInvalidationMask();
}

void RenderSVGResourceContainer::willBeDestroyed()
{
    SVGResourcesCache::resourceDestroyed(this);
    RenderSVGHiddenContainer::willBeDestroyed();
    if (m_registered)
        svgExtensionsFromElement(element()).removeResource(m_id);
}

void RenderSVGResourceContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (!m_registered) {
        m_registered = true;
        registerResource();
    }
}

void RenderSVGResourceContainer::idChanged()
{
    // Invalidate all our current clients.
    removeAllClientsFromCache();

    // Remove old id, that is guaranteed to be present in cache.
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    extensions.removeResource(m_id);
    m_id = element()->getIdAttribute();

    registerResource();
}

void RenderSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    if ((m_clients.isEmpty() && m_clientLayers.isEmpty()) || m_isInvalidating)
        return;

    if (m_invalidationMask & mode)
        return;

    m_invalidationMask |= mode;
    m_isInvalidating = true;
    bool needsLayout = mode == LayoutAndBoundariesInvalidation;
    bool markForInvalidation = mode != ParentOnlyInvalidation;

    HashSet<RenderObject*>::iterator end = m_clients.end();
    for (HashSet<RenderObject*>::iterator it = m_clients.begin(); it != end; ++it) {
        RenderObject* client = *it;
        if (client->isSVGResourceContainer()) {
            toRenderSVGResourceContainer(client)->removeAllClientsFromCache(markForInvalidation);
            continue;
        }

        if (markForInvalidation)
            markClientForInvalidation(client, mode);

        RenderSVGResourceContainer::markForLayoutAndParentResourceInvalidation(client, needsLayout);
    }

    markAllClientLayersForInvalidation();

    m_isInvalidating = false;
}

void RenderSVGResourceContainer::markAllClientLayersForInvalidation()
{
    HashSet<RenderLayer*>::iterator layerEnd = m_clientLayers.end();
    for (HashSet<RenderLayer*>::iterator it = m_clientLayers.begin(); it != layerEnd; ++it)
        (*it)->filterNeedsPaintInvalidation();
}

void RenderSVGResourceContainer::markClientForInvalidation(RenderObject* client, InvalidationMode mode)
{
    ASSERT(client);
    ASSERT(!m_clients.isEmpty());

    switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
        client->setNeedsBoundariesUpdate();
        break;
    case PaintInvalidation:
        client->setShouldDoFullPaintInvalidation();
        break;
    case ParentOnlyInvalidation:
        break;
    }
}

void RenderSVGResourceContainer::addClient(RenderObject* client)
{
    ASSERT(client);
    m_clients.add(client);
    clearInvalidationMask();
}

void RenderSVGResourceContainer::removeClient(RenderObject* client)
{
    ASSERT(client);
    removeClientFromCache(client, false);
    m_clients.remove(client);
}

void RenderSVGResourceContainer::addClientRenderLayer(Node* node)
{
    ASSERT(node);
    if (!node->renderer() || !node->renderer()->hasLayer())
        return;
    m_clientLayers.add(toRenderLayerModelObject(node->renderer())->layer());
    clearInvalidationMask();
}

void RenderSVGResourceContainer::addClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.add(client);
    clearInvalidationMask();
}

void RenderSVGResourceContainer::removeClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.remove(client);
}

void RenderSVGResourceContainer::invalidateCacheAndMarkForLayout(SubtreeLayoutScope* layoutScope)
{
    if (selfNeedsLayout())
        return;

    setNeedsLayoutAndFullPaintInvalidation(MarkContainingBlockChain, layoutScope);

    if (everHadLayout())
        removeAllClientsFromCache();
}

void RenderSVGResourceContainer::registerResource()
{
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    if (!extensions.hasPendingResource(m_id)) {
        extensions.addResource(m_id, this);
        return;
    }

    OwnPtrWillBeRawPtr<SVGDocumentExtensions::SVGPendingElements> clients(extensions.removePendingResource(m_id));

    // Cache us with the new id.
    extensions.addResource(m_id, this);

    // Update cached resources of pending clients.
    const SVGDocumentExtensions::SVGPendingElements::const_iterator end = clients->end();
    for (SVGDocumentExtensions::SVGPendingElements::const_iterator it = clients->begin(); it != end; ++it) {
        ASSERT((*it)->hasPendingResources());
        extensions.clearHasPendingResourcesIfPossible(*it);
        RenderObject* renderer = (*it)->renderer();
        if (!renderer)
            continue;

        StyleDifference diff;
        diff.setNeedsFullLayout();
        SVGResourcesCache::clientStyleChanged(renderer, diff, renderer->style());
        renderer->setNeedsLayoutAndFullPaintInvalidation();
    }
}

static inline void removeFromCacheAndInvalidateDependencies(RenderObject* object, bool needsLayout)
{
    ASSERT(object);
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object)) {
        if (RenderSVGResourceFilter* filter = resources->filter())
            filter->removeClientFromCache(object);

        if (RenderSVGResourceMasker* masker = resources->masker())
            masker->removeClientFromCache(object);

        if (RenderSVGResourceClipper* clipper = resources->clipper())
            clipper->removeClientFromCache(object);
    }

    if (!object->node() || !object->node()->isSVGElement())
        return;
    SVGElementSet* dependencies = toSVGElement(object->node())->setOfIncomingReferences();
    if (!dependencies)
        return;

    // We allow cycles in SVGDocumentExtensions reference sets in order to avoid expensive
    // reference graph adjustments on changes, so we need to break possible cycles here.
    // This strong reference is safe, as it is guaranteed that this set will be emptied
    // at the end of recursion.
    typedef WillBeHeapHashSet<RawPtrWillBeMember<SVGElement> > SVGElementSet;
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<SVGElementSet>, invalidatingDependencies, (adoptPtrWillBeNoop(new SVGElementSet)));

    SVGElementSet::iterator end = dependencies->end();
    for (SVGElementSet::iterator it = dependencies->begin(); it != end; ++it) {
        if (RenderObject* renderer = (*it)->renderer()) {
            if (UNLIKELY(!invalidatingDependencies->add(*it).isNewEntry)) {
                // Reference cycle: we are in process of invalidating this dependant.
                continue;
            }

            RenderSVGResourceContainer::markForLayoutAndParentResourceInvalidation(renderer, needsLayout);
            invalidatingDependencies->remove(*it);
        }
    }
}

void RenderSVGResourceContainer::markForLayoutAndParentResourceInvalidation(RenderObject* object, bool needsLayout)
{
    ASSERT(object);
    ASSERT(object->node());

    if (needsLayout && !object->documentBeingDestroyed())
        object->setNeedsLayoutAndFullPaintInvalidation();

    removeFromCacheAndInvalidateDependencies(object, needsLayout);

    // Invalidate resources in ancestor chain, if needed.
    RenderObject* current = object->parent();
    while (current) {
        removeFromCacheAndInvalidateDependencies(current, needsLayout);

        if (current->isSVGResourceContainer()) {
            // This will process the rest of the ancestors.
            toRenderSVGResourceContainer(current)->removeAllClientsFromCache();
            break;
        }

        current = current->parent();
    }
}

}
