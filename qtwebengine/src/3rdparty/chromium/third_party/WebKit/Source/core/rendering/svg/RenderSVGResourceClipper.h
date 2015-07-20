/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef RenderSVGResourceClipper_h
#define RenderSVGResourceClipper_h

#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGClipPathElement.h"

namespace blink {

class DisplayList;

class RenderSVGResourceClipper final : public RenderSVGResourceContainer {
public:
    enum ClipperState {
        ClipperNotApplied,
        ClipperAppliedPath,
        ClipperAppliedMask
    };

    explicit RenderSVGResourceClipper(SVGClipPathElement*);
    virtual ~RenderSVGResourceClipper();

    virtual const char* renderName() const override { return "RenderSVGResourceClipper"; }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override;
    virtual void removeClientFromCache(RenderObject*, bool markForInvalidation = true) override;

    // FIXME: Filters are also stateful resources that could benefit from having their state managed
    //        on the caller stack instead of the current hashmap. We should look at refactoring these
    //        into a general interface that can be shared.
    bool applyStatefulResource(RenderObject*, GraphicsContext*&, ClipperState&);
    void postApplyStatefulResource(RenderObject*, GraphicsContext*&, ClipperState&);

    // clipPath can be clipped too, but don't have a boundingBox or paintInvalidationRect. So we can't call
    // applyResource directly and use the rects from the object, since they are empty for RenderSVGResources
    // FIXME: We made applyClippingToContext public because we cannot call applyResource on HTML elements (it asserts on RenderObject::objectBoundingBox)
    bool applyClippingToContext(RenderObject*, const FloatRect&, const FloatRect&, GraphicsContext*, ClipperState&);

    FloatRect resourceBoundingBox(const RenderObject*);

    static const RenderSVGResourceType s_resourceType = ClipperResourceType;
    virtual RenderSVGResourceType resourceType() const override { return s_resourceType; }

    bool hitTestClipContent(const FloatRect&, const FloatPoint&);

    SVGUnitTypes::SVGUnitType clipPathUnits() const { return toSVGClipPathElement(element())->clipPathUnits()->currentValue()->enumValue(); }

private:
    bool tryPathOnlyClipping(GraphicsContext*, const AffineTransform&, const FloatRect&);
    void drawClipMaskContent(GraphicsContext*, const FloatRect& targetBoundingBox);
    void createDisplayList(GraphicsContext*);
    void calculateClipContentPaintInvalidationRect();

    RefPtr<DisplayList> m_clipContentDisplayList;
    FloatRect m_clipBoundaries;

    // Reference cycle detection.
    bool m_inClipExpansion;
};

DEFINE_RENDER_SVG_RESOURCE_TYPE_CASTS(RenderSVGResourceClipper, ClipperResourceType);

}

#endif
