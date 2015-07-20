/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef RenderGrid_h
#define RenderGrid_h

#include "core/rendering/OrderIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/style/GridResolvedPosition.h"

namespace blink {

struct GridCoordinate;
struct GridSpan;
class GridTrack;
class GridItemWithSpan;

class RenderGrid final : public RenderBlock {
public:
    RenderGrid(Element*);
    virtual ~RenderGrid();

    virtual const char* renderName() const override;

    virtual void layoutBlock(bool relayoutChildren) override;

    virtual bool canCollapseAnonymousBlockChild() const override { return false; }

    void dirtyGrid();

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    typedef Vector<RenderBox*, 1> GridCell;
    const GridCell& gridCell(int row, int column) { return m_grid[row][column]; }
    const Vector<RenderBox*>& itemsOverflowingGridArea() { return m_gridItemsOverflowingGridArea; }
    int paintIndexForGridItem(const RenderBox* renderBox) { return m_gridItemsIndexesMap.get(renderBox); }

    bool gridIsDirty() const { return m_gridIsDirty; }

private:
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectRenderGrid || RenderBlock::isOfType(type); }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    void addChildToIndexesMap(RenderBox&);
    virtual void removeChild(RenderObject*) override;

    virtual void styleDidChange(StyleDifference, const RenderStyle*) override;

    bool explicitGridDidResize(const RenderStyle*) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle*) const;

    class GridIterator;
    struct GridSizingData;
    bool gridElementIsShrinkToFit();
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&);
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);
    LayoutUnit computeUsedBreadthOfMinLength(GridTrackSizingDirection, const GridLength&) const;
    LayoutUnit computeUsedBreadthOfMaxLength(GridTrackSizingDirection, const GridLength&, LayoutUnit usedBreadth) const;
    LayoutUnit computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection, const Length&) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);

    void ensureGridSize(size_t maximumRowIndex, size_t maximumColumnIndex);
    void insertItemIntoGrid(RenderBox&, const GridCoordinate&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    PassOwnPtr<GridCoordinate> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox&, GridTrackSizingDirection, const GridSpan& specifiedPositions) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemOnGrid(RenderBox&, std::pair<size_t, size_t>& autoPlacementCursor);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void layoutGridItems();
    void populateGridPositions(const GridSizingData&);

    typedef LayoutUnit (RenderGrid::* SizingFunction)(RenderBox&, GridTrackSizingDirection, Vector<GridTrack>&);
    typedef LayoutUnit (GridTrack::* AccumulatorGetter)() const;
    typedef void (GridTrack::* AccumulatorGrowFunction)(LayoutUnit);
    typedef bool (GridTrackSize::* FilterFunction)() const;
    void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, GridItemWithSpan&, FilterFunction, SizingFunction, AccumulatorGetter, AccumulatorGrowFunction, FilterFunction growAboveMaxBreadthFilterFunction = nullptr);
    void distributeSpaceToTracks(Vector<GridTrack*>&, Vector<size_t>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter, AccumulatorGrowFunction, GridSizingData&, LayoutUnit& availableLogicalSpace);

    double computeNormalizedFractionBreadth(Vector<GridTrack>&, const GridSpan& tracksSpan, GridTrackSizingDirection, LayoutUnit availableLogicalSpace) const;

    GridTrackSize gridTrackSize(GridTrackSizingDirection, size_t) const;

    LayoutUnit logicalHeightForChild(RenderBox&, Vector<GridTrack>&);
    LayoutUnit minContentForChild(RenderBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit maxContentForChild(RenderBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit startOfColumnForChild(const RenderBox& child) const;
    LayoutUnit endOfColumnForChild(const RenderBox& child) const;
    LayoutUnit columnPositionAlignedWithGridContainerStart(const RenderBox&) const;
    LayoutUnit columnPositionAlignedWithGridContainerEnd(const RenderBox&) const;
    LayoutUnit centeredColumnPositionForChild(const RenderBox&) const;
    LayoutUnit columnPositionForChild(const RenderBox&) const;
    LayoutUnit startOfRowForChild(const RenderBox& child) const;
    LayoutUnit endOfRowForChild(const RenderBox& child) const;
    LayoutUnit centeredRowPositionForChild(const RenderBox&) const;
    LayoutUnit rowPositionForChild(const RenderBox&) const;
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridCoordinate cachedGridCoordinate(const RenderBox&) const;

    LayoutUnit gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection, const Vector<GridTrack>&) const;

    virtual void paintChildren(PaintInfo&, const LayoutPoint&) override;
    bool needToStretchChildLogicalHeight(const RenderBox&) const;
    LayoutUnit childIntrinsicHeight(const RenderBox&) const;
    LayoutUnit childIntrinsicWidth(const RenderBox&) const;
    LayoutUnit intrinsicLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit marginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&, LayoutUnit gridAreaBreadthForChild);

#if ENABLE(ASSERT)
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, const Vector<GridTrack>&);
#endif

    size_t gridItemSpan(const RenderBox&, GridTrackSizingDirection);
    bool spanningItemCrossesFlexibleSizedTracks(const GridCoordinate&, GridTrackSizingDirection) const;

    size_t gridColumnCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid[0].size();
    }
    size_t gridRowCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid.size();
    }

    typedef Vector<Vector<GridCell> > GridRepresentation;
    GridRepresentation m_grid;
    bool m_gridIsDirty;
    Vector<LayoutUnit> m_rowPositions;
    Vector<LayoutUnit> m_columnPositions;
    HashMap<const RenderBox*, GridCoordinate> m_gridItemCoordinate;
    OrderIterator m_orderIterator;
    Vector<RenderBox*> m_gridItemsOverflowingGridArea;
    HashMap<const RenderBox*, size_t> m_gridItemsIndexesMap;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderGrid, isRenderGrid());

} // namespace blink

#endif // RenderGrid_h
