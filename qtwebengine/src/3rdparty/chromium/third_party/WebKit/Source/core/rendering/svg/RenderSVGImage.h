/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef RenderSVGImage_h
#define RenderSVGImage_h

#include "core/rendering/svg/RenderSVGModelObject.h"

namespace blink {

class DisplayList;
class RenderImageResource;
class SVGImageElement;

class RenderSVGImage final : public RenderSVGModelObject {
public:
    explicit RenderSVGImage(SVGImageElement*);
    virtual ~RenderSVGImage();
    virtual void destroy() override;

    bool updateImageViewport();
    virtual void setNeedsBoundariesUpdate() override { m_needsBoundariesUpdate = true; }
    virtual void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    RenderImageResource* imageResource() { return m_imageResource.get(); }

    virtual const AffineTransform& localToParentTransform() const override { return m_localTransform; }
    RefPtr<DisplayList>& bufferedForeground() { return m_bufferedForeground; }

    virtual FloatRect paintInvalidationRectInLocalCoordinates() const override { return m_paintInvalidationBoundingBox; }
    virtual FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectSVGImage || RenderSVGModelObject::isOfType(type); }

private:
    virtual const char* renderName() const override { return "RenderSVGImage"; }

    virtual FloatRect strokeBoundingBox() const override { return m_objectBoundingBox; }

    virtual void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const override;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override;

    virtual void layout() override;
    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    FloatSize computeImageViewportSize(ImageResource&) const;

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    virtual AffineTransform localTransform() const override { return m_localTransform; }

    bool m_needsBoundariesUpdate : 1;
    bool m_needsTransformUpdate : 1;
    AffineTransform m_localTransform;
    FloatRect m_objectBoundingBox;
    FloatRect m_paintInvalidationBoundingBox;
    OwnPtr<RenderImageResource> m_imageResource;

    RefPtr<DisplayList> m_bufferedForeground;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGImage, isSVGImage());

} // namespace blink

#endif // RenderSVGImage_h
