/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef RenderBlock_h
#define RenderBlock_h

#include "core/rendering/ColumnInfo.h"
#include "core/rendering/FloatingObjects.h"
#include "core/rendering/GapRects.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLineBoxList.h"
#include "core/rendering/RootInlineBox.h"
#include "core/rendering/style/ShapeValue.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextRun.h"
#include "wtf/ListHashSet.h"
#include "wtf/OwnPtr.h"

namespace blink {

class LineInfo;
class LineLayoutState;
struct PaintInfo;
class RenderInline;
class RenderRubyRun;
class RenderText;
class WordMeasurement;

template <class Run> class BidiRunList;
typedef WTF::ListHashSet<RenderBox*, 16> TrackedRendererListHashSet;
typedef WTF::HashMap<const RenderBlock*, OwnPtr<TrackedRendererListHashSet> > TrackedDescendantsMap;
typedef WTF::HashMap<const RenderBox*, OwnPtr<HashSet<RenderBlock*> > > TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

typedef WTF::HashMap<RenderBlock*, OwnPtr<ListHashSet<RenderInline*> > > ContinuationOutlineTableMap;

ContinuationOutlineTableMap* continuationOutlineTable();

class RenderBlock : public RenderBox {
public:
    virtual void destroy() override;
    virtual void trace(Visitor*) override;
    friend class LineLayoutState;

protected:
    explicit RenderBlock(ContainerNode*);
    virtual ~RenderBlock();

public:
    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a RenderBlock, use firstChild or lastChild instead.
    void slowFirstChild() const WTF_DELETED_FUNCTION;
    void slowLastChild() const WTF_DELETED_FUNCTION;

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    bool beingDestroyed() const { return m_beingDestroyed; }

    // These two functions are overridden for inline-block.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override final;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    LayoutUnit minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const;

    RenderLineBoxList* lineBoxes() { return &m_lineBoxes; }

protected:
    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }

    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(firstLineBox()); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(lastLineBox()); }

public:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void deleteLineBoxTree();

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    virtual void removeChild(RenderObject*) override;

    virtual void layoutBlock(bool relayoutChildren);

    void insertPositionedObject(RenderBox*);
    static void removePositionedObject(RenderBox*);
    void removePositionedObjects(RenderBlock*, ContainingBlockState = SameContainingBlock);

    TrackedRendererListHashSet* positionedObjects() const;
    bool hasPositionedObjects() const
    {
        TrackedRendererListHashSet* objects = positionedObjects();
        return objects && !objects->isEmpty();
    }

    void addPercentHeightDescendant(RenderBox*);
    static void removePercentHeightDescendant(RenderBox*);
    static bool hasPercentHeightContainerMap();
    static bool hasPercentHeightDescendant(RenderBox*);
    static void clearPercentHeightDescendantsFrom(RenderBox*);
    static void removePercentHeightDescendantIfNeeded(RenderBox*);

    TrackedRendererListHashSet* percentHeightDescendants() const;
    bool hasPercentHeightDescendants() const
    {
        TrackedRendererListHashSet* descendants = percentHeightDescendants();
        return descendants && !descendants->isEmpty();
    }

    void setHasMarkupTruncation(bool b) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }

    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }

    bool hasMarginBeforeQuirk(const RenderBox* child) const;
    bool hasMarginAfterQuirk(const RenderBox* child) const;

    void markPositionedObjectsForLayout();
    // FIXME: Do we really need this to be virtual? It's just so we can call this on
    // RenderBoxes without needed to check whether they're RenderBlocks first.
    virtual void markForPaginationRelayoutIfNeeded(SubtreeLayoutScope&) override final;

    LayoutUnit textIndentOffset() const;

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual LayoutUnit availableLogicalWidth() const override final;

    LayoutPoint flipForWritingModeIncludingColumns(const LayoutPoint&) const;
    void adjustStartEdgeForWritingModeIncludingColumns(LayoutRect&) const;

    LayoutUnit blockDirectionOffset(const LayoutSize& offsetFromBlock) const;
    LayoutUnit inlineDirectionOffset(const LayoutSize& offsetFromBlock) const;

    virtual bool shouldPaintSelectionGaps() const override final;
    GapRects selectionGapRectsForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const;
    LayoutRect logicalLeftSelectionGap(const RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                       const RenderObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo*) const;
    LayoutRect logicalRightSelectionGap(const RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                        const RenderObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo*) const;
    void getSelectionGapInfo(SelectionState, bool& leftGap, bool& rightGap) const;
    RenderBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    virtual void setSelectionState(SelectionState) override;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect) const;

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int) const;
    int lineCount(const RootInlineBox* = 0, bool* = 0) const;
    int heightForLineCount(int);
    void clearTruncation();

    void adjustRectForColumns(LayoutRect&) const;
    virtual LayoutSize columnOffset(const LayoutPoint&) const override;
    void adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const;

    void addContinuationWithOutline(RenderInline*);

    virtual RenderBoxModelObject* virtualContinuation() const override final { return continuation(); }
    bool isAnonymousBlockContinuation() const { return continuation() && isAnonymousBlock(); }
    RenderInline* inlineElementContinuation() const;
    RenderBlock* blockElementContinuation() const;

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    static RenderBlock* createAnonymousWithParentRendererAndDisplay(const RenderObject*, EDisplay = BLOCK);
    static RenderBlockFlow* createAnonymousColumnsWithParentRenderer(const RenderObject*);
    static RenderBlockFlow* createAnonymousColumnSpanWithParentRenderer(const RenderObject*);
    RenderBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentRendererAndDisplay(this, display); }
    RenderBlockFlow* createAnonymousColumnsBlock() const { return createAnonymousColumnsWithParentRenderer(this); }
    RenderBlockFlow* createAnonymousColumnSpanBlock() const { return createAnonymousColumnSpanWithParentRenderer(this); }

    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const override;

    ColumnInfo* columnInfo() const;
    int columnGap() const;

    // These two functions take the ColumnInfo* to avoid repeated lookups of the info in the global HashMap.
    unsigned columnCount(ColumnInfo*) const;
    LayoutRect columnRectAt(ColumnInfo*, unsigned) const;

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const { return m_pageLogicalOffset; }
    void setPageLogicalOffset(LayoutUnit offset) { m_pageLogicalOffset = offset; }

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    LayoutUnit logicalWidthForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->width() : child->height(); }
    LayoutUnit logicalHeightForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->height() : child->width(); }
    LayoutSize logicalSizeForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->size() : child->size().transposedSize(); }
    LayoutUnit logicalTopForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->y() : child->x(); }
    LayoutUnit marginBeforeForChild(const RenderBoxModelObject* child) const { return child->marginBefore(style()); }
    LayoutUnit marginAfterForChild(const RenderBoxModelObject* child) const { return child->marginAfter(style()); }
    LayoutUnit marginStartForChild(const RenderBoxModelObject* child) const { return child->marginStart(style()); }
    LayoutUnit marginEndForChild(const RenderBoxModelObject* child) const { return child->marginEnd(style()); }
    void setMarginStartForChild(RenderBox* child, LayoutUnit value) const { child->setMarginStart(value, style()); }
    void setMarginEndForChild(RenderBox* child, LayoutUnit value) const { child->setMarginEnd(value, style()); }
    void setMarginBeforeForChild(RenderBox* child, LayoutUnit value) const { child->setMarginBefore(value, style()); }
    void setMarginAfterForChild(RenderBox* child, LayoutUnit value) const { child->setMarginAfter(value, style()); }
    LayoutUnit collapsedMarginBeforeForChild(const RenderBox* child) const;
    LayoutUnit collapsedMarginAfterForChild(const RenderBox* child) const;

    virtual void updateFirstLetter();
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/) { }

    LayoutUnit availableLogicalWidthForContent() const { return max<LayoutUnit>(0, logicalRightOffsetForContent() - logicalLeftOffsetForContent()); }
    LayoutUnit logicalLeftOffsetForContent() const { return isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
    LayoutUnit logicalRightOffsetForContent() const { return logicalLeftOffsetForContent() + availableLogicalWidth(); }
    LayoutUnit startOffsetForContent() const { return style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }
    LayoutUnit endOffsetForContent() const { return !style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }

    virtual LayoutUnit logicalLeftSelectionOffset(const RenderBlock* rootBlock, LayoutUnit position) const;
    virtual LayoutUnit logicalRightSelectionOffset(const RenderBlock* rootBlock, LayoutUnit position) const;

    GapRects selectionGaps(const RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo* = 0) const;

#if ENABLE(ASSERT)
    void checkPositionedObjectsNeedLayout();
    bool paintsContinuationOutline(RenderInline* flow);
#endif
#ifndef NDEBUG
    void showLineTreeAndMark(const InlineBox* = 0, const char* = 0, const InlineBox* = 0, const char* = 0, const RenderObject* = 0) const;
#endif

    bool recalcChildOverflowAfterStyleChange();
    bool recalcOverflowAfterStyleChange();

protected:
    virtual void willBeDestroyed() override;

    void dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope&);

    virtual void layout() override;
    virtual bool updateImageLoadingPriorities() override final;

    enum PositionedLayoutBehavior {
        DefaultLayout,
        LayoutOnlyFixedPositionedObjects,
        ForcedLayoutAfterContainingBlockMoved
    };

    void layoutPositionedObjects(bool relayoutChildren, PositionedLayoutBehavior = DefaultLayout);
    void markFixedPositionObjectForLayoutIfNeeded(RenderObject* child, SubtreeLayoutScope&);

    LayoutUnit marginIntrinsicLogicalWidthForChild(RenderBox* child) const;

    int beforeMarginInLineDirection(LineDirectionMode) const;

    virtual void paint(PaintInfo&, const LayoutPoint&) override;
public:
    virtual void paintObject(PaintInfo&, const LayoutPoint&) override;
    virtual void paintChildren(PaintInfo&, const LayoutPoint&);

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool) { }

protected:
    virtual void adjustInlineDirectionLineBounds(unsigned /* expansionOpportunityCount */, float& /* logicalLeft */, float& /* logicalWidth */) const { }

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;
    void adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    virtual int firstLineBoxBaseline() const override;
    virtual int inlineBlockBaseline(LineDirectionMode) const override;
    int lastLineBoxBaseline(LineDirectionMode) const;

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    // Delay update scrollbar until finishDelayUpdateScrollInfo() will be
    // called. This function is used when a flexbox is laying out its
    // descendant. If multiple calls are made to startDelayUpdateScrollInfo(),
    // finishDelayUpdateScrollInfo() will do nothing until finishDelayUpdateScrollInfo()
    // is called the same number of times.
    static void startDelayUpdateScrollInfo();
    static void finishDelayUpdateScrollInfo();

    void updateScrollInfoAfterLayout();

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual bool hasLineIfEmpty() const;

    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

    void setDesiredColumnCountAndWidth(int, LayoutUnit);

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool = false);
protected:
    virtual void addOverflowFromChildren();
    void addOverflowFromPositionedObjects();
    void addOverflowFromBlockChildren();
    void addVisualOverflowFromTheme();

    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const override;

    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const override;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox*);

    virtual bool isInlineBlockOrInlineTable() const override final { return isInline() && isReplaced(); }

    virtual void invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState& childPaintInvalidationState) override;

private:
    virtual RenderObjectChildList* virtualChildren() override final { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const override final { return children(); }

    virtual const char* renderName() const override;

    virtual bool isRenderBlock() const override final { return true; }

    void makeChildrenNonInline(RenderObject* insertionPoint = 0);
    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);

    static void collapseAnonymousBlockChild(RenderBlock* parent, RenderBlock* child);

    virtual void dirtyLinesFromChangedChild(RenderObject* child) override final { m_lineBoxes.dirtyLinesFromChangedChild(this, child); }

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild) override;
    void addChildToAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild);

    void addChildIgnoringAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild = 0);

    virtual bool isSelfCollapsingBlock() const override;

    void insertIntoTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);
    static void removeFromTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);

    void createFirstLetterRenderer(RenderObject* firstLetterBlock, RenderText& currentChild, unsigned length);
    void updateFirstLetterStyle(RenderObject* firstLetterBlock, RenderObject* firstLetterContainer);
    bool isInvalidFirstLetterRenderer(RenderObject*) const;

    Node* nodeForHitTest() const;

    bool tryLayoutDoingPositionedMovementOnly();

    virtual bool avoidsFloats() const override { return true; }

    bool hitTestColumns(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    bool hitTestContents(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset);

    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const override;

    virtual LayoutRect rectWithOutlineForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, LayoutUnit outlineWidth, const PaintInvalidationState* = 0) const override final;

    virtual RenderObject* hoverAncestor() const override final;
    virtual void updateDragState(bool dragOn) override final;
    virtual void childBecameNonInline(RenderObject* child) override final;

    virtual LayoutRect selectionRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const override final
    {
        return selectionGapRectsForPaintInvalidation(paintInvalidationContainer);
    }
    bool isSelectionRoot() const;
    GapRects blockSelectionGaps(const RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo*) const;
    LayoutRect blockSelectionGap(const RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                 LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const PaintInfo*) const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void clipOutFloatingObjects(const RenderBlock*, const PaintInfo*, const LayoutPoint&, const LayoutSize&) const { };

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    LayoutUnit desiredColumnWidth() const;

private:
    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) override final;

    void adjustPointToColumnContents(LayoutPoint&) const;

    void markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest = 0);

    Position positionForBox(InlineBox*, bool start = true) const;
    PositionWithAffinity positionForPointWithInlineChildren(const LayoutPoint&);

    void calcColumnWidth();
    void makeChildrenAnonymousColumnBlocks(RenderObject* beforeChild, RenderBlockFlow* newBlockBox, RenderObject* newChild);

    void splitBlocks(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                     RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderBoxModelObject* oldCont);
    RenderBlock* clone() const;
    RenderBlock* continuationBefore(RenderObject* beforeChild);
    RenderBlockFlow* containingColumnsBlock(bool allowAnonymousColumnBlock = true);
    RenderBlockFlow* columnsBlockForSpanningElement(RenderObject* newChild);

    // End helper functions and structs used by layoutBlockChildren.

    void removeFromGlobalMaps();
    bool widthAvailableToChildrenHasChanged();

protected:
    // Returns the logicalOffset at the top of the next page. If the offset passed in is already at the top of the current page,
    // then nextPageLogicalTop with ExcludePageBoundary will still move to the top of the next page. nextPageLogicalTop with
    // IncludePageBoundary set will not.
    //
    // For a page height of 800px, the first rule will return 800 if the value passed in is 0. The second rule will simply return 0.
    enum PageBoundaryRule { ExcludePageBoundary, IncludePageBoundary };
    LayoutUnit nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;

    bool createsBlockFormattingContext() const;

public:
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit offset) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule = IncludePageBoundary) const;

protected:
    // A page break is required at some offset due to space shortage in the current fragmentainer.
    void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage);

    // Update minimum page height required to avoid fragmentation where it shouldn't occur (inside
    // unbreakable content, between orphans and widows, etc.). This will be used as a hint to the
    // column balancer to help set a good minimum column height.
    void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight);

    // Adjust from painting offsets to the local coords of this renderer
    void offsetForContents(LayoutPoint&) const;

    bool requiresColumns(int desiredColumnCount) const;

    virtual bool updateLogicalWidthAndColumnWidth();

    virtual bool canCollapseAnonymousBlockChild() const { return true; }

public:
    virtual LayoutUnit offsetFromLogicalTopOfFirstPage() const override final;

protected:
    RenderObjectChildList m_children;
    RenderLineBoxList m_lineBoxes;   // All of the root line boxes created for this block flow.  For example, <div>Hello<br>world.</div> will have two total lines for the <div>.

    LayoutUnit m_pageLogicalOffset;

    unsigned m_hasMarginBeforeQuirk : 1; // Note these quirk values can't be put in RenderBlockRareData since they are set too frequently.
    unsigned m_hasMarginAfterQuirk : 1;
    unsigned m_beingDestroyed : 1;
    unsigned m_hasMarkupTruncation : 1;
    unsigned m_hasBorderOrPaddingLogicalWidthChanged : 1;
    mutable unsigned m_hasOnlySelfCollapsingChildren : 1;
    mutable unsigned m_descendantsWithFloatsMarkedForLayout : 1;

    // RenderRubyBase objects need to be able to split and merge, moving their children around
    // (calling moveChildTo, moveAllChildrenTo, and makeChildrenNonInline).
    friend class RenderRubyBase;
    // FIXME-BLOCKFLOW: Remove this when the line layout stuff has all moved out of RenderBlock
    friend class LineBreaker;

    // FIXME: This is temporary as we move code that accesses block flow
    // member variables out of RenderBlock and into RenderBlockFlow.
    friend class RenderBlockFlow;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderBlock, isRenderBlock());

} // namespace blink

#endif // RenderBlock_h
