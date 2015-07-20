/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef RenderSVGResourceGradient_h
#define RenderSVGResourceGradient_h

#include "core/rendering/svg/RenderSVGResourcePaintServer.h"
#include "core/svg/SVGGradientElement.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Gradient.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/HashMap.h"

namespace blink {

struct GradientData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<Gradient> gradient;
    AffineTransform userspaceTransform;
};

class GraphicsContext;

class RenderSVGResourceGradient : public RenderSVGResourcePaintServer {
public:
    explicit RenderSVGResourceGradient(SVGGradientElement*);

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override final;
    virtual void removeClientFromCache(RenderObject*, bool markForInvalidation = true) override final;

    virtual SVGPaintServer preparePaintServer(const RenderObject&) override final;

    virtual bool isChildAllowed(RenderObject* child, RenderStyle*) const override final;

protected:
    void addStops(GradientData*, const Vector<Gradient::ColorStop>&) const;

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
    virtual void calculateGradientTransform(AffineTransform&) = 0;
    virtual bool collectGradientAttributes(SVGGradientElement*) = 0;
    virtual void buildGradient(GradientData*) const = 0;

    GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType) const;

private:
    bool m_shouldCollectGradientAttributes : 1;
    HashMap<const RenderObject*, OwnPtr<GradientData> > m_gradientMap;
};

}

#endif
