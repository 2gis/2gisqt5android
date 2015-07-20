/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderReplaced_h
#define RenderReplaced_h

#include "core/rendering/RenderBox.h"

namespace blink {

class RenderReplaced : public RenderBox {
public:
    RenderReplaced(Element*);
    RenderReplaced(Element*, const LayoutSize& intrinsicSize);
    virtual ~RenderReplaced();

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    virtual LayoutUnit computeReplacedLogicalHeight() const override;

    bool hasReplacedLogicalHeight() const;
    LayoutRect replacedContentRect(const LayoutSize* overriddenIntrinsicSize = 0) const;

    virtual bool needsPreferredWidthsRecalculation() const override;

    // These values are specified to be 300 and 150 pixels in the CSS 2.1 spec.
    // http://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
    static const int defaultWidth;
    static const int defaultHeight;
    virtual bool canHaveChildren() const override { return false; }
    bool shouldPaint(PaintInfo&, const LayoutPoint&);
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) { }
    LayoutRect localSelectionRect(bool checkWhetherSelected = true) const; // This is in local coordinates, but it's a physical rect (so the top left corner is physical top left).

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

protected:
    virtual void willBeDestroyed() override;

    virtual void layout() override;

    virtual LayoutSize intrinsicSize() const override final { return m_intrinsicSize; }
    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const override;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override final;

    virtual LayoutUnit intrinsicContentLogicalHeight() const { return intrinsicLogicalHeight(); }

    virtual LayoutUnit minimumReplacedHeight() const { return LayoutUnit(); }

    virtual void setSelectionState(SelectionState) override final;

    bool isSelected() const;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }
    virtual void intrinsicSizeChanged();

    virtual RenderBox* embeddedContentBox() const { return 0; }

private:
    virtual const char* renderName() const override { return "RenderReplaced"; }

    virtual void computePreferredLogicalWidths() override final;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0) const override;

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) override final;

    virtual bool canBeSelectionLeaf() const override { return true; }

    virtual LayoutRect selectionRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const override final;
    void computeAspectRatioInformationForRenderBox(RenderBox*, FloatSize& constrainedSize, double& intrinsicRatio) const;

    mutable LayoutSize m_intrinsicSize;
};

}

#endif
