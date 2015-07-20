/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderRegion.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/HitTestLocation.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderView.h"

namespace blink {

RenderRegion::RenderRegion(Element* element, RenderFlowThread* flowThread)
    : RenderBlockFlow(element)
    , m_flowThread(flowThread)
    , m_isValid(false)
{
}

LayoutUnit RenderRegion::pageLogicalWidth() const
{
    ASSERT(m_flowThread);
    return m_flowThread->isHorizontalWritingMode() ? contentWidth() : contentHeight();
}

LayoutUnit RenderRegion::pageLogicalHeight() const
{
    ASSERT(m_flowThread);
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

LayoutRect RenderRegion::flowThreadPortionOverflowRect() const
{
    return overflowRectForFlowThreadPortion(flowThreadPortionRect(), isFirstRegion(), isLastRegion());
}

LayoutRect RenderRegion::overflowRectForFlowThreadPortion(const LayoutRect& flowThreadPortionRect, bool isFirstPortion, bool isLastPortion) const
{
    ASSERT(isValid());

    if (hasOverflowClip())
        return flowThreadPortionRect;

    LayoutRect flowThreadOverflow = m_flowThread->visualOverflowRect();

    // Only clip along the flow thread axis.
    LayoutRect clipRect;
    if (m_flowThread->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstPortion ? flowThreadOverflow.y() : flowThreadPortionRect.y();
        LayoutUnit maxY = isLastPortion ? std::max(flowThreadPortionRect.maxY(), flowThreadOverflow.maxY()) : flowThreadPortionRect.maxY();
        LayoutUnit minX = std::min(flowThreadPortionRect.x(), flowThreadOverflow.x());
        LayoutUnit maxX = std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstPortion ? flowThreadOverflow.x() : flowThreadPortionRect.x();
        LayoutUnit maxX = isLastPortion ? std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX()) : flowThreadPortionRect.maxX();
        LayoutUnit minY = std::min(flowThreadPortionRect.y(), (flowThreadOverflow.y()));
        LayoutUnit maxY = std::max(flowThreadPortionRect.y(), (flowThreadOverflow.maxY()));
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }

    return clipRect;
}

bool RenderRegion::isFirstRegion() const
{
    ASSERT(isValid());

    return m_flowThread->firstRegion() == this;
}

bool RenderRegion::isLastRegion() const
{
    ASSERT(isValid());

    return m_flowThread->lastRegion() == this;
}

void RenderRegion::layoutBlock(bool relayoutChildren)
{
    RenderBlockFlow::layoutBlock(relayoutChildren);

    // FIXME: We need to find a way to set up overflow properly. Our flow thread hasn't gotten a layout
    // yet, so we can't look to it for correct information. It's possible we could wait until after the RenderFlowThread
    // gets a layout, and then try to propagate overflow information back to the region, and then mark for a second layout.
    // That second layout would then be able to use the information from the RenderFlowThread to set up overflow.
    //
    // The big problem though is that overflow needs to be region-specific. We can't simply use the RenderFlowThread's global
    // overflow values, since then we'd always think any narrow region had huge overflow (all the way to the width of the
    // RenderFlowThread itself).
}

void RenderRegion::paintInvalidationOfFlowThreadContentRectangle(const LayoutRect& paintInvalidationRect, const LayoutRect& flowThreadPortionRect, const LayoutRect& flowThreadPortionOverflowRect, const LayoutPoint& regionLocation) const
{
    ASSERT(isValid());

    // We only have to issue a paint invalidation in this region if the region rect intersects the paint invalidation rect.
    LayoutRect flippedFlowThreadPortionRect(flowThreadPortionRect);
    LayoutRect flippedFlowThreadPortionOverflowRect(flowThreadPortionOverflowRect);
    flowThread()->flipForWritingMode(flippedFlowThreadPortionRect); // Put the region rects into physical coordinates.
    flowThread()->flipForWritingMode(flippedFlowThreadPortionOverflowRect);

    LayoutRect clippedRect(paintInvalidationRect);
    clippedRect.intersect(flippedFlowThreadPortionOverflowRect);
    if (clippedRect.isEmpty())
        return;

    // Put the region rect into the region's physical coordinate space.
    clippedRect.setLocation(regionLocation + (clippedRect.location() - flippedFlowThreadPortionRect.location()));

    // Now switch to the region's writing mode coordinate space and let it issue paint invalidations itself.
    flipForWritingMode(clippedRect);

    invalidatePaintRectangle(clippedRect);
}

void RenderRegion::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!isValid()) {
        RenderBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }

    minLogicalWidth = m_flowThread->minPreferredLogicalWidth();
    maxLogicalWidth = m_flowThread->maxPreferredLogicalWidth();
}

} // namespace blink
