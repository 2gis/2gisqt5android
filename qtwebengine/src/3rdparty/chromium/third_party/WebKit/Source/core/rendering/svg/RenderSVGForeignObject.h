/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
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

#ifndef RenderSVGForeignObject_h
#define RenderSVGForeignObject_h

#include "core/rendering/svg/RenderSVGBlock.h"

namespace blink {

class SVGForeignObjectElement;

class RenderSVGForeignObject final : public RenderSVGBlock {
public:
    explicit RenderSVGForeignObject(SVGForeignObjectElement*);
    virtual ~RenderSVGForeignObject();

    virtual const char* renderName() const override { return "RenderSVGForeignObject"; }

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const override;

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    virtual void layout() override;

    virtual FloatRect objectBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    virtual FloatRect strokeBoundingBox() const override { return FloatRect(FloatPoint(), m_viewport.size()); }
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const override { return FloatRect(FloatPoint(), m_viewport.size()); }

    virtual void mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState*) const override;
    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectSVGForeignObject || RenderSVGBlock::isOfType(type); }

    virtual void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    FloatRect viewportRect() { return m_viewport; }

private:
    virtual void updateLogicalWidth() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    virtual const AffineTransform& localToParentTransform() const override;

    bool m_needsTransformUpdate : 1;
    FloatRect m_viewport;
    mutable AffineTransform m_localToParentTransform;
};

}

#endif
