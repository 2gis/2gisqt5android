/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Computer, Inc
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#ifndef RenderSVGShape_h
#define RenderSVGShape_h

#include "core/rendering/svg/RenderSVGModelObject.h"
#include "core/rendering/svg/SVGMarkerData.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class FloatPoint;
class PointerEventsHitRules;
class SVGGraphicsElement;

enum ShapeGeometryCodePath {
    PathGeometry,
    RectGeometryFastPath,
    EllipseGeometryFastPath
};

class RenderSVGShape : public RenderSVGModelObject {
public:
    explicit RenderSVGShape(SVGGraphicsElement*);
    virtual ~RenderSVGShape();

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    virtual void setNeedsBoundariesUpdate() override final { m_needsBoundariesUpdate = true; }
    virtual void setNeedsTransformUpdate() override final { m_needsTransformUpdate = true; }

    bool nodeAtFloatPointInternal(const HitTestRequest&, const FloatPoint&, PointerEventsHitRules);

    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }
    bool hasPath() const { return m_path.get(); }

    virtual bool isShapeEmpty() const { return path().isEmpty(); }

    virtual FloatRect paintInvalidationRectInLocalCoordinates() const override final { return m_paintInvalidationBoundingBox; }

    bool hasNonScalingStroke() const { return style()->svgStyle().vectorEffect() == VE_NON_SCALING_STROKE; }
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;
    AffineTransform nonScalingStrokeTransform() const;
    virtual AffineTransform localTransform() const override final { return m_localTransform; }

    virtual const Vector<MarkerPosition>* markerPositions() const { return 0; }

    float strokeWidth() const;

    virtual ShapeGeometryCodePath geometryCodePath() const { return PathGeometry; }
    virtual const Vector<FloatPoint>* zeroLengthLineCaps() const { return 0; }

    virtual FloatRect objectBoundingBox() const override final { return m_fillBoundingBox; }

protected:
    virtual void updateShapeFromElement();
    virtual bool shapeDependentStrokeContains(const FloatPoint&);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    bool hasSmoothStroke() const;

    // Give RenderSVGPath a hook for updating markers in updateShapeFromElement.
    virtual void processMarkerPositions() { };

    FloatRect m_fillBoundingBox;
    FloatRect m_strokeBoundingBox;

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = RULE_NONZERO);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    virtual const AffineTransform& localToParentTransform() const override final { return m_localTransform; }

    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectSVGShape || RenderSVGModelObject::isOfType(type); }
    virtual const char* renderName() const override { return "RenderSVGShape"; }

    virtual void layout() override final;
    virtual void paint(PaintInfo&, const LayoutPoint&) override final;
    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const override final;

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override final;

    virtual FloatRect strokeBoundingBox() const override final { return m_strokeBoundingBox; }
    FloatRect calculateObjectBoundingBox() const;
    FloatRect calculateStrokeBoundingBox() const;
    void updatePaintInvalidationBoundingBox();

private:
    FloatRect m_paintInvalidationBoundingBox;
    AffineTransform m_localTransform;
    OwnPtr<Path> m_path;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGShape, isSVGShape());

}

#endif
