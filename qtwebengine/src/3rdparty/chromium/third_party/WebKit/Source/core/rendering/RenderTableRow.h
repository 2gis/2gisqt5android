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

#ifndef RenderTableRow_h
#define RenderTableRow_h

#include "core/rendering/RenderTableSection.h"

namespace WebCore {

static const unsigned unsetRowIndex = 0x7FFFFFFF;
static const unsigned maxRowIndex = 0x7FFFFFFE; // 2,147,483,646

class RenderTableRow FINAL : public RenderBox {
public:
    explicit RenderTableRow(Element*);

    RenderTableCell* firstCell() const;
    RenderTableCell* lastCell() const;

    RenderTableRow* previousRow() const;
    RenderTableRow* nextRow() const;

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    RenderTableSection* section() const { return toRenderTableSection(parent()); }
    RenderTable* table() const { return toRenderTable(parent()->parent()); }

    void paintOutlineForRowIfNeeded(PaintInfo&, const LayoutPoint&);

    static RenderTableRow* createAnonymous(Document*);
    static RenderTableRow* createAnonymousWithParentRenderer(const RenderObject*);
    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const OVERRIDE
    {
        return createAnonymousWithParentRenderer(parent);
    }

    void setRowIndex(unsigned rowIndex)
    {
        if (UNLIKELY(rowIndex > maxRowIndex))
            CRASH();

        m_rowIndex = rowIndex;
    }

    bool rowIndexWasSet() const { return m_rowIndex != unsetRowIndex; }
    unsigned rowIndex() const
    {
        ASSERT(rowIndexWasSet());
        return m_rowIndex;
    }

    const BorderValue& borderAdjoiningTableStart() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style()->borderStart();

        return style()->borderEnd();
    }

    const BorderValue& borderAdjoiningTableEnd() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style()->borderEnd();

        return style()->borderStart();
    }

    const BorderValue& borderAdjoiningStartCell(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningEndCell(const RenderTableCell*) const;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

private:
    virtual RenderObjectChildList* virtualChildren() OVERRIDE { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const OVERRIDE { return children(); }

    virtual const char* renderName() const OVERRIDE { return (isAnonymous() || isPseudoElement()) ? "RenderTableRow (anonymous)" : "RenderTableRow"; }

    virtual bool isTableRow() const OVERRIDE { return true; }

    virtual void willBeRemovedFromTree() OVERRIDE;

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void layout() OVERRIDE;

    virtual LayerType layerTypeRequired() const OVERRIDE
    {
        if (hasTransform() || hasHiddenBackface() || hasClipPath() || createsGroup() || isStickyPositioned() || style()->shouldCompositeForCurrentAnimations())
            return NormalLayer;

        if (hasOverflowClip())
            return OverflowClipLayer;

        return NoLayer;
    }

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) OVERRIDE;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    void nextSibling() const WTF_DELETED_FUNCTION;
    void previousSibling() const WTF_DELETED_FUNCTION;

    RenderObjectChildList m_children;
    unsigned m_rowIndex : 31;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderTableRow, isTableRow());

inline RenderTableRow* RenderTableRow::previousRow() const
{
    return toRenderTableRow(RenderObject::previousSibling());
}

inline RenderTableRow* RenderTableRow::nextRow() const
{
    return toRenderTableRow(RenderObject::nextSibling());
}

inline RenderTableRow* RenderTableSection::firstRow() const
{
    ASSERT(children() == virtualChildren());
    return toRenderTableRow(children()->firstChild());
}

inline RenderTableRow* RenderTableSection::lastRow() const
{
    ASSERT(children() == virtualChildren());
    return toRenderTableRow(children()->lastChild());
}

} // namespace WebCore

#endif // RenderTableRow_h
