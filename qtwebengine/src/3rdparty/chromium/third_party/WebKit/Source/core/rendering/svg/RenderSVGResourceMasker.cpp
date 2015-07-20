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

#include "config.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"

#include "core/dom/ElementTraversal.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker()
{
}

void RenderSVGResourceMasker::removeAllClientsFromCache(bool markForInvalidation)
{
    m_maskContentDisplayList.clear();
    m_maskContentBoundaries = FloatRect();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceMasker::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceMasker::prepareEffect(RenderObject* object, GraphicsContext*& context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    clearInvalidationMask();

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();
    if (paintInvalidationRect.isEmpty() || !element()->hasChildren())
        return false;

    // Content layer start.
    context->beginTransparencyLayer(1, &paintInvalidationRect);

    return true;
}

void RenderSVGResourceMasker::finishEffect(RenderObject* object, GraphicsContext*& context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();

    const SVGRenderStyle& svgStyle = style()->svgStyle();
    ColorFilter maskLayerFilter = svgStyle.maskType() == MT_LUMINANCE
        ? ColorFilterLuminanceToAlpha : ColorFilterNone;
    ColorFilter maskContentFilter = svgStyle.colorInterpolation() == CI_LINEARRGB
        ? ColorFilterSRGBToLinearRGB : ColorFilterNone;

    // Mask layer start.
    context->beginLayer(1, CompositeDestinationIn, &paintInvalidationRect, maskLayerFilter);
    {
        // Draw the mask with color conversion (when needed).
        GraphicsContextStateSaver maskContentSaver(*context);
        context->setColorFilter(maskContentFilter);

        drawMaskForRenderer(context, object->objectBoundingBox());
    }

    // Transfer mask layer -> content layer (DstIn)
    context->endLayer();
    // Transfer content layer -> backdrop (SrcOver)
    context->endLayer();
}

void RenderSVGResourceMasker::drawMaskForRenderer(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    SVGUnitTypes::SVGUnitType contentUnits = toSVGMaskElement(element())->maskContentUnits()->currentValue()->enumValue();
    if (contentUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        contentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(contentTransformation);
    }

    if (!m_maskContentDisplayList) {
        SubtreeContentTransformScope contentTransformScope(contentTransformation);
        createDisplayList(context);
    }
    ASSERT(m_maskContentDisplayList);
    context->drawDisplayList(m_maskContentDisplayList.get());
}

void RenderSVGResourceMasker::createDisplayList(GraphicsContext* context)
{
    ASSERT(context);

    // Using strokeBoundingBox (instead of paintInvalidationRectInLocalCoordinates) to avoid the intersection
    // with local clips/mask, which may yield incorrect results when mixing objectBoundingBox and
    // userSpaceOnUse units (http://crbug.com/294900).
    FloatRect bounds = strokeBoundingBox();
    context->beginRecording(bounds);
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        RenderObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        SVGRenderingContext::renderSubtree(context, renderer);
    }
    m_maskContentDisplayList = context->endRecording();
}

void RenderSVGResourceMasker::calculateMaskContentPaintInvalidationRect()
{
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        RenderObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        m_maskContentBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->paintInvalidationRectInLocalCoordinates()));
    }
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const RenderObject* object)
{
    SVGMaskElement* maskElement = toSVGMaskElement(element());
    ASSERT(maskElement);

    FloatRect objectBoundingBox = object->objectBoundingBox();
    FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(maskElement, maskElement->maskUnits()->currentValue()->enumValue(), objectBoundingBox);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskContentBoundaries.isEmpty())
        calculateMaskContentPaintInvalidationRect();

    FloatRect maskRect = m_maskContentBoundaries;
    if (maskElement->maskContentUnits()->currentValue()->value() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        maskRect = transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
