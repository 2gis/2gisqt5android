/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc. All rights reserved.
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

#ifndef RenderTableSection_h
#define RenderTableSection_h

#include "core/rendering/RenderTable.h"
#include "wtf/Vector.h"

namespace blink {

// This variable is used to balance the memory consumption vs the paint invalidation time on big tables.
const float gMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

enum CollapsedBorderSide {
    CBSBefore,
    CBSAfter,
    CBSStart,
    CBSEnd
};

// Helper class for paintObject.
class CellSpan {
public:
    CellSpan(unsigned start, unsigned end)
        : m_start(start)
        , m_end(end)
    {
    }

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    void decreaseStart() { --m_start; }
    void increaseEnd() { ++m_end; }

private:
    unsigned m_start;
    unsigned m_end;
};

class RenderTableCell;
class RenderTableRow;

class RenderTableSection final : public RenderBox {
public:
    RenderTableSection(Element*);
    virtual ~RenderTableSection();
    virtual void trace(Visitor*) override;

    RenderTableRow* firstRow() const;
    RenderTableRow* lastRow() const;

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;

    virtual int firstLineBoxBaseline() const override;

    void addCell(RenderTableCell*, RenderTableRow* row);

    int calcRowLogicalHeight();
    void layoutRows();
    void computeOverflowFromCells();

    RenderTable* table() const { return toRenderTable(parent()); }

    typedef WillBeHeapVector<RawPtrWillBeMember<RenderTableCell>, 2> SpanningRenderTableCells;

    struct CellStruct {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        WillBeHeapVector<RawPtrWillBeMember<RenderTableCell>, 1> cells;
        bool inColSpan; // true for columns after the first in a colspan

        CellStruct()
            : inColSpan(false)
        {
        }
        void trace(Visitor*);

        RenderTableCell* primaryCell()
        {
            return hasCells() ? cells[cells.size() - 1].get() : 0;
        }

        const RenderTableCell* primaryCell() const
        {
            return hasCells() ? cells[cells.size() - 1].get() : 0;
        }

        bool hasCells() const { return cells.size() > 0; }
    };

    typedef WillBeHeapVector<CellStruct> Row;

    struct RowStruct {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        RowStruct()
            : rowRenderer(nullptr)
            , baseline()
        {
        }
        void trace(Visitor*);

        Row row;
        RawPtrWillBeMember<RenderTableRow> rowRenderer;
        LayoutUnit baseline;
        Length logicalHeight;
    };

    struct SpanningRowsHeight {
        WTF_MAKE_NONCOPYABLE(SpanningRowsHeight);

    public:
        SpanningRowsHeight()
            : totalRowsHeight(0)
            , spanningCellHeightIgnoringBorderSpacing(0)
            , isAnyRowWithOnlySpanningCells(false)
        {
        }

        Vector<int> rowHeight;
        int totalRowsHeight;
        int spanningCellHeightIgnoringBorderSpacing;
        bool isAnyRowWithOnlySpanningCells;
    };

    const BorderValue& borderAdjoiningTableStart() const
    {
        if (hasSameDirectionAs(table()))
            return style()->borderStart();

        return style()->borderEnd();
    }

    const BorderValue& borderAdjoiningTableEnd() const
    {
        if (hasSameDirectionAs(table()))
            return style()->borderEnd();

        return style()->borderStart();
    }

    const BorderValue& borderAdjoiningStartCell(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningEndCell(const RenderTableCell*) const;

    const RenderTableCell* firstRowCellAdjoiningTableStart() const;
    const RenderTableCell* firstRowCellAdjoiningTableEnd() const;

    CellStruct& cellAt(unsigned row,  unsigned col) { return m_grid[row].row[col]; }
    const CellStruct& cellAt(unsigned row, unsigned col) const { return m_grid[row].row[col]; }
    RenderTableCell* primaryCellAt(unsigned row, unsigned col)
    {
        CellStruct& c = m_grid[row].row[col];
        return c.primaryCell();
    }

    RenderTableRow* rowRendererAt(unsigned row) const { return m_grid[row].rowRenderer; }

    void appendColumn(unsigned pos);
    void splitColumn(unsigned pos, unsigned first);

    enum BlockBorderSide { BorderBefore, BorderAfter };
    int calcBlockDirectionOuterBorder(BlockBorderSide) const;
    enum InlineBorderSide { BorderStart, BorderEnd };
    int calcInlineDirectionOuterBorder(InlineBorderSide) const;
    void recalcOuterBorder();

    int outerBorderBefore() const { return m_outerBorderBefore; }
    int outerBorderAfter() const { return m_outerBorderAfter; }
    int outerBorderStart() const { return m_outerBorderStart; }
    int outerBorderEnd() const { return m_outerBorderEnd; }

    unsigned numRows() const { return m_grid.size(); }
    unsigned numColumns() const;
    void recalcCells();
    void recalcCellsIfNeeded()
    {
        if (m_needsCellRecalc)
            recalcCells();
    }

    bool needsCellRecalc() const { return m_needsCellRecalc; }
    void setNeedsCellRecalc();

    LayoutUnit rowBaseline(unsigned row) { return m_grid[row].baseline; }

    void rowLogicalHeightChanged(RenderTableRow*);

    void removeCachedCollapsedBorders(const RenderTableCell*);
    void setCachedCollapsedBorder(const RenderTableCell*, CollapsedBorderSide, CollapsedBorderValue);
    CollapsedBorderValue& cachedCollapsedBorder(const RenderTableCell*, CollapsedBorderSide);

    // distributeExtraLogicalHeightToRows methods return the *consumed* extra logical height.
    // FIXME: We may want to introduce a structure holding the in-flux layout information.
    int distributeExtraLogicalHeightToRows(int extraLogicalHeight);

    static RenderTableSection* createAnonymousWithParentRenderer(const RenderObject*);
    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const override
    {
        return createAnonymousWithParentRenderer(parent);
    }

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    // Flip the rect so it aligns with the coordinates used by the rowPos and columnPos vectors.
    LayoutRect logicalRectForWritingModeAndDirection(const LayoutRect&) const;

    CellSpan dirtiedRows(const LayoutRect& paintInvalidationRect) const;
    CellSpan dirtiedColumns(const LayoutRect& paintInvalidationRect) const;
    WillBeHeapHashSet<RawPtrWillBeMember<RenderTableCell> >& overflowingCells() { return m_overflowingCells; }
    bool hasMultipleCellLevels() { return m_hasMultipleCellLevels; }

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

private:
    virtual RenderObjectChildList* virtualChildren() override { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const override { return children(); }

    virtual const char* renderName() const override { return (isAnonymous() || isPseudoElement()) ? "RenderTableSection (anonymous)" : "RenderTableSection"; }

    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectTableSection || RenderBox::isOfType(type); }

    virtual void willBeRemovedFromTree() override;

    virtual void layout() override;

    virtual void paintObject(PaintInfo&, const LayoutPoint&) override;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override;

    int borderSpacingForRow(unsigned row) const { return m_grid[row].rowRenderer ? table()->vBorderSpacing() : 0; }

    void ensureRows(unsigned);

    bool rowHasOnlySpanningCells(unsigned);
    unsigned calcRowHeightHavingOnlySpanningCells(unsigned);
    void updateRowsHeightHavingOnlySpanningCells(RenderTableCell*, struct SpanningRowsHeight&);
    bool isHeightNeededForRowHavingOnlySpanningCells(unsigned);

    void populateSpanningRowsHeightFromCell(RenderTableCell*, struct SpanningRowsHeight&);
    void distributeExtraRowSpanHeightToPercentRows(RenderTableCell*, int, int&, Vector<int>&);
    void distributeWholeExtraRowSpanHeightToPercentRows(RenderTableCell*, int, int&, Vector<int>&);
    void distributeExtraRowSpanHeightToAutoRows(RenderTableCell*, int, int&, Vector<int>&);
    void distributeExtraRowSpanHeightToRemainingRows(RenderTableCell*, int, int&, Vector<int>&);
    void distributeRowSpanHeightToRows(SpanningRenderTableCells& rowSpanCells);

    void distributeExtraLogicalHeightToPercentRows(int& extraLogicalHeight, int totalPercent);
    void distributeExtraLogicalHeightToAutoRows(int& extraLogicalHeight, unsigned autoRowsCount);
    void distributeRemainingExtraLogicalHeight(int& extraLogicalHeight);

    void updateBaselineForCell(RenderTableCell*, unsigned row, LayoutUnit& baselineDescent);

    bool hasOverflowingCell() const { return m_overflowingCells.size() || m_forceSlowPaintPathWithOverflowingCell; }

    void computeOverflowFromCells(unsigned totalRows, unsigned nEffCols);

    CellSpan fullTableRowSpan() const { return CellSpan(0, m_grid.size()); }
    CellSpan fullTableColumnSpan() const { return CellSpan(0, table()->columns().size()); }

    // These two functions take a rectangle as input that has been flipped by logicalRectForWritingModeAndDirection.
    // The returned span of rows or columns is end-exclusive, and empty if start==end.
    CellSpan spannedRows(const LayoutRect& flippedRect) const;
    CellSpan spannedColumns(const LayoutRect& flippedRect) const;

    void setLogicalPositionForCell(RenderTableCell*, unsigned effectiveColumn) const;

    RenderObjectChildList m_children;

    WillBeHeapVector<RowStruct> m_grid;
    Vector<int> m_rowPos;

    // the current insertion position
    unsigned m_cCol;
    unsigned m_cRow;

    int m_outerBorderStart;
    int m_outerBorderEnd;
    int m_outerBorderBefore;
    int m_outerBorderAfter;

    bool m_needsCellRecalc;

    // This HashSet holds the overflowing cells for faster painting.
    // If we have more than gMaxAllowedOverflowingCellRatio * total cells, it will be empty
    // and m_forceSlowPaintPathWithOverflowingCell will be set to save memory.
    WillBeHeapHashSet<RawPtrWillBeMember<RenderTableCell> > m_overflowingCells;
    bool m_forceSlowPaintPathWithOverflowingCell;

    bool m_hasMultipleCellLevels;

    // This map holds the collapsed border values for cells with collapsed borders.
    // It is held at RenderTableSection level to spare memory consumption by table cells.
    WillBeHeapHashMap<pair<RawPtrWillBeMember<const RenderTableCell>, int>, CollapsedBorderValue > m_cellsCollapsedBorders;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderTableSection, isTableSection());

} // namespace blink

#if ENABLE(OILPAN)
namespace WTF {

template<> struct VectorTraits<blink::RenderTableSection::RowStruct> : VectorTraitsBase<blink::RenderTableSection::RowStruct> {
    static const bool needsDestruction = false;
};

} // namespace WTF
#endif

#endif // RenderTableSection_h
