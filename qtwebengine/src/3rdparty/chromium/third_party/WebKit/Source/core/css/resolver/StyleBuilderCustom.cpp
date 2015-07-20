/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/StyleBuilderFunctions.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/Counter.h"
#include "core/css/Pair.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        return false;
    }
}

} // namespace

void StyleBuilder::applyProperty(CSSPropertyID id, StyleResolverState& state, CSSValue* value)
{
    ASSERT_WITH_MESSAGE(!isShorthandProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSPropertyMetadata::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    StyleBuilder::applyProperty(id, state, value, isInitial, isInherit);
}

void StyleBuilderFunctions::applyInitialCSSPropertyColor(StyleResolverState& state)
{
    Color color = RenderStyle::initialColor();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyInheritCSSPropertyColor(StyleResolverState& state)
{
    Color color = state.parentStyle()->color();
    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(color);
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(color);
}

void StyleBuilderFunctions::applyValueCSSPropertyColor(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    // As per the spec, 'color: currentColor' is treated as 'color: inherit'
    if (primitiveValue->getValueID() == CSSValueCurrentcolor) {
        applyInheritCSSPropertyColor(state);
        return;
    }

    if (state.applyPropertyToRegularStyle())
        state.style()->setColor(StyleBuilderConverter::convertColor(state, value));
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(StyleBuilderConverter::convertColor(state, value, true));
}

void StyleBuilderFunctions::applyInitialCSSPropertyJustifyItems(StyleResolverState& state)
{
    state.style()->setJustifyItems(RenderStyle::initialJustifyItems());
    state.style()->setJustifyItemsOverflowAlignment(RenderStyle::initialJustifyItemsOverflowAlignment());
    state.style()->setJustifyItemsPositionType(RenderStyle::initialJustifyItemsPositionType());
}

void StyleBuilderFunctions::applyInheritCSSPropertyJustifyItems(StyleResolverState& state)
{
    state.style()->setJustifyItems(state.parentStyle()->justifyItems());
    state.style()->setJustifyItemsOverflowAlignment(state.parentStyle()->justifyItemsOverflowAlignment());
    state.style()->setJustifyItemsPositionType(state.parentStyle()->justifyItemsPositionType());
}

void StyleBuilderFunctions::applyValueCSSPropertyJustifyItems(StyleResolverState& state, CSSValue* value)
{

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (Pair* pairValue = primitiveValue->getPairValue()) {
        if (pairValue->first()->getValueID() == CSSValueLegacy) {
            state.style()->setJustifyItemsPositionType(LegacyPosition);
            state.style()->setJustifyItems(*pairValue->second());
        } else {
            state.style()->setJustifyItems(*pairValue->first());
            state.style()->setJustifyItemsOverflowAlignment(*pairValue->second());
        }
    } else {
        state.style()->setJustifyItems(*primitiveValue);
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyJustifyContent(StyleResolverState& state)
{
    state.style()->setJustifyContent(RenderStyle::initialJustifyContent());
    state.style()->setJustifyContentOverflowAlignment(RenderStyle::initialJustifyContentOverflowAlignment());
    state.style()->setJustifyContentDistribution(RenderStyle::initialJustifyContentDistribution());
}

void StyleBuilderFunctions::applyInheritCSSPropertyJustifyContent(StyleResolverState& state)
{
    state.style()->setJustifyContent(state.parentStyle()->justifyContent());
    state.style()->setJustifyContentOverflowAlignment(state.parentStyle()->justifyContentOverflowAlignment());
    state.style()->setJustifyContentDistribution(state.parentStyle()->justifyContentDistribution());
}

void StyleBuilderFunctions::applyValueCSSPropertyJustifyContent(StyleResolverState& state, CSSValue* value)
{
    CSSContentDistributionValue* contentValue = toCSSContentDistributionValue(value);
    if (contentValue->distribution()->getValueID() != CSSValueInvalid)
        state.style()->setJustifyContentDistribution(*contentValue->distribution());
    if (contentValue->position()->getValueID() != CSSValueInvalid)
        state.style()->setJustifyContent(*contentValue->position());
    if (contentValue->overflow()->getValueID() != CSSValueInvalid)
        state.style()->setJustifyContentOverflowAlignment(*contentValue->overflow());
}

void StyleBuilderFunctions::applyInitialCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->clearCursorList();
    state.style()->setCursor(RenderStyle::initialCursor());
}

void StyleBuilderFunctions::applyInheritCSSPropertyCursor(StyleResolverState& state)
{
    state.style()->setCursor(state.parentStyle()->cursor());
    state.style()->setCursorList(state.parentStyle()->cursors());
}

void StyleBuilderFunctions::applyValueCSSPropertyCursor(StyleResolverState& state, CSSValue* value)
{
    state.style()->clearCursorList();
    if (value->isValueList()) {
        CSSValueList* list = toCSSValueList(value);
        int len = list->length();
        state.style()->setCursor(CURSOR_AUTO);
        for (int i = 0; i < len; i++) {
            CSSValue* item = list->item(i);
            if (item->isCursorImageValue()) {
                CSSCursorImageValue* image = toCSSCursorImageValue(item);
                if (image->updateIfSVGCursorIsUsed(state.element())) // Elements with SVG cursors are not allowed to share style.
                    state.style()->setUnique();
                state.style()->addCursor(state.styleImage(CSSPropertyCursor, image), image->hotSpot());
            } else {
                state.style()->setCursor(*toCSSPrimitiveValue(item));
            }
        }
    } else {
        state.style()->setCursor(*toCSSPrimitiveValue(value));
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyDirection(StyleResolverState& state, CSSValue* value)
{
    state.style()->setDirection(*toCSSPrimitiveValue(value));
    Element* element = state.element();
    if (element && element == element->document().documentElement())
        element->document().setDirectionSetOnDocumentElement(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyGlyphOrientationVertical(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueAuto)
        state.style()->accessSVGStyle().setGlyphOrientationVertical(GO_AUTO);
    else
        state.style()->accessSVGStyle().setGlyphOrientationVertical(StyleBuilderConverter::convertGlyphOrientation(state, value));
}

void StyleBuilderFunctions::applyInitialCSSPropertyGridTemplateAreas(StyleResolverState& state)
{
    state.style()->setNamedGridArea(RenderStyle::initialNamedGridArea());
    state.style()->setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
    state.style()->setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
}

void StyleBuilderFunctions::applyInheritCSSPropertyGridTemplateAreas(StyleResolverState& state)
{
    state.style()->setNamedGridArea(state.parentStyle()->namedGridArea());
    state.style()->setNamedGridAreaRowCount(state.parentStyle()->namedGridAreaRowCount());
    state.style()->setNamedGridAreaColumnCount(state.parentStyle()->namedGridAreaColumnCount());
}

void StyleBuilderFunctions::applyValueCSSPropertyGridTemplateAreas(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        // FIXME: Shouldn't we clear the grid-area values
        ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
        return;
    }

    CSSGridTemplateAreasValue* gridTemplateAreasValue = toCSSGridTemplateAreasValue(value);
    const NamedGridAreaMap& newNamedGridAreas = gridTemplateAreasValue->gridAreaMap();

    NamedGridLinesMap namedGridColumnLines = state.style()->namedGridColumnLines();
    NamedGridLinesMap namedGridRowLines = state.style()->namedGridRowLines();
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridColumnLines, ForColumns);
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridRowLines, ForRows);
    state.style()->setNamedGridColumnLines(namedGridColumnLines);
    state.style()->setNamedGridRowLines(namedGridRowLines);

    state.style()->setNamedGridArea(newNamedGridAreas);
    state.style()->setNamedGridAreaRowCount(gridTemplateAreasValue->rowCount());
    state.style()->setNamedGridAreaColumnCount(gridTemplateAreasValue->columnCount());
}

void StyleBuilderFunctions::applyValueCSSPropertyLineHeight(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Length lineHeight;

    if (primitiveValue->getValueID() == CSSValueNormal) {
        lineHeight = RenderStyle::initialLineHeight();
    } else if (primitiveValue->isLength()) {
        float multiplier = state.style()->effectiveZoom();
        if (LocalFrame* frame = state.document().frame())
            multiplier *= frame->textZoomFactor();
        lineHeight = primitiveValue->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(multiplier));
    } else if (primitiveValue->isPercentage()) {
        lineHeight = Length((state.style()->computedFontSize() * primitiveValue->getIntValue()) / 100.0, Fixed);
    } else if (primitiveValue->isNumber()) {
        lineHeight = Length(primitiveValue->getDoubleValue() * 100.0, Percent);
    } else if (primitiveValue->isCalculated()) {
        double multiplier = state.style()->effectiveZoom();
        if (LocalFrame* frame = state.document().frame())
            multiplier *= frame->textZoomFactor();
        Length zoomedLength = Length(primitiveValue->cssCalcValue()->toCalcValue(state.cssToLengthConversionData().copyWithAdjustedZoom(multiplier)));
        lineHeight = Length(valueForLength(zoomedLength, state.style()->fontSize()), Fixed);
    } else {
        return;
    }
    state.style()->setLineHeight(lineHeight);
}

void StyleBuilderFunctions::applyValueCSSPropertyListStyleImage(StyleResolverState& state, CSSValue* value)
{
    state.style()->setListStyleImage(state.styleImage(CSSPropertyListStyleImage, value));
}

void StyleBuilderFunctions::applyInitialCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(RenderStyle::initialOutlineStyleIsAuto());
    state.style()->setOutlineStyle(RenderStyle::initialBorderStyle());
}

void StyleBuilderFunctions::applyInheritCSSPropertyOutlineStyle(StyleResolverState& state)
{
    state.style()->setOutlineStyleIsAuto(state.parentStyle()->outlineStyleIsAuto());
    state.style()->setOutlineStyle(state.parentStyle()->outlineStyle());
}

void StyleBuilderFunctions::applyValueCSSPropertyOutlineStyle(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    state.style()->setOutlineStyleIsAuto(*primitiveValue);
    state.style()->setOutlineStyle(*primitiveValue);
}

void StyleBuilderFunctions::applyValueCSSPropertyResize(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    EResize r = RESIZE_NONE;
    if (primitiveValue->getValueID() == CSSValueAuto) {
        if (Settings* settings = state.document().settings())
            r = settings->textAreasAreResizable() ? RESIZE_BOTH : RESIZE_NONE;
    } else {
        r = *primitiveValue;
    }
    state.style()->setResize(r);
}

static Length mmLength(double mm) { return Length(mm * cssPixelsPerMillimeter, Fixed); }
static Length inchLength(double inch) { return Length(inch * cssPixelsPerInch, Fixed); }
static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
{
    DEFINE_STATIC_LOCAL(Length, a5Width, (mmLength(148)));
    DEFINE_STATIC_LOCAL(Length, a5Height, (mmLength(210)));
    DEFINE_STATIC_LOCAL(Length, a4Width, (mmLength(210)));
    DEFINE_STATIC_LOCAL(Length, a4Height, (mmLength(297)));
    DEFINE_STATIC_LOCAL(Length, a3Width, (mmLength(297)));
    DEFINE_STATIC_LOCAL(Length, a3Height, (mmLength(420)));
    DEFINE_STATIC_LOCAL(Length, b5Width, (mmLength(176)));
    DEFINE_STATIC_LOCAL(Length, b5Height, (mmLength(250)));
    DEFINE_STATIC_LOCAL(Length, b4Width, (mmLength(250)));
    DEFINE_STATIC_LOCAL(Length, b4Height, (mmLength(353)));
    DEFINE_STATIC_LOCAL(Length, letterWidth, (inchLength(8.5)));
    DEFINE_STATIC_LOCAL(Length, letterHeight, (inchLength(11)));
    DEFINE_STATIC_LOCAL(Length, legalWidth, (inchLength(8.5)));
    DEFINE_STATIC_LOCAL(Length, legalHeight, (inchLength(14)));
    DEFINE_STATIC_LOCAL(Length, ledgerWidth, (inchLength(11)));
    DEFINE_STATIC_LOCAL(Length, ledgerHeight, (inchLength(17)));

    if (!pageSizeName)
        return false;

    switch (pageSizeName->getValueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        return false;
    }

    if (pageOrientation) {
        switch (pageOrientation->getValueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            return false;
        }
    }
    return true;
}

void StyleBuilderFunctions::applyInitialCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyInheritCSSPropertySize(StyleResolverState&) { }
void StyleBuilderFunctions::applyValueCSSPropertySize(StyleResolverState& state, CSSValue* value)
{
    state.style()->resetPageSizeType();
    Length width;
    Length height;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;
    CSSValueList* list = toCSSValueList(value);
    switch (list->length()) {
    case 2: {
        // <length>{2} | <page-size> <orientation>
        CSSPrimitiveValue* first = toCSSPrimitiveValue(list->item(0));
        CSSPrimitiveValue* second = toCSSPrimitiveValue(list->item(1));
        if (first->isLength()) {
            // <length>{2}
            if (!second->isLength())
                return;
            width = first->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            height = second->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See BisonCSSParser::parseSizeParameter.
            if (!getPageSizeFromName(first, second, width, height))
                return;
        }
        pageSizeType = PAGE_SIZE_RESOLVED;
        break;
    }
    case 1: {
        // <length> | auto | <page-size> | [ portrait | landscape]
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(list->item(0));
        if (primitiveValue->isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            width = height = primitiveValue->computeLength<Length>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
        } else {
            switch (primitiveValue->getValueID()) {
            case 0:
                return;
            case CSSValueAuto:
                pageSizeType = PAGE_SIZE_AUTO;
                break;
            case CSSValuePortrait:
                pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                break;
            case CSSValueLandscape:
                pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                break;
            default:
                // <page-size>
                pageSizeType = PAGE_SIZE_RESOLVED;
                if (!getPageSizeFromName(primitiveValue, 0, width, height))
                    return;
            }
        }
        break;
    }
    default:
        return;
    }
    state.style()->setPageSizeType(pageSizeType);
    state.style()->setPageSize(LengthSize(width, height));
}

void StyleBuilderFunctions::applyValueCSSPropertyTextAlign(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    // FIXME : Per http://www.w3.org/TR/css3-text/#text-align0 can now take <string> but this is not implemented in the
    // rendering code.
    if (primitiveValue->isString())
        return;

    if (primitiveValue->isValueID() && primitiveValue->getValueID() != CSSValueWebkitMatchParent)
        state.style()->setTextAlign(*primitiveValue);
    else if (state.parentStyle()->textAlign() == TASTART)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? LEFT : RIGHT);
    else if (state.parentStyle()->textAlign() == TAEND)
        state.style()->setTextAlign(state.parentStyle()->isLeftToRightDirection() ? RIGHT : LEFT);
    else
        state.style()->setTextAlign(state.parentStyle()->textAlign());
}

void StyleBuilderFunctions::applyInheritCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(state.parentStyle()->textIndent());
    state.style()->setTextIndentLine(state.parentStyle()->textIndentLine());
    state.style()->setTextIndentType(state.parentStyle()->textIndentType());
}

void StyleBuilderFunctions::applyInitialCSSPropertyTextIndent(StyleResolverState& state)
{
    state.style()->setTextIndent(RenderStyle::initialTextIndent());
    state.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
    state.style()->setTextIndentType(RenderStyle::initialTextIndentType());
}

void StyleBuilderFunctions::applyValueCSSPropertyTextIndent(StyleResolverState& state, CSSValue* value)
{
    if (!value->isValueList())
        return;

    Length lengthOrPercentageValue;
    TextIndentLine textIndentLineValue = RenderStyle::initialTextIndentLine();
    TextIndentType textIndentTypeValue = RenderStyle::initialTextIndentType();

    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(i.value());
        if (!primitiveValue->getValueID())
            lengthOrPercentageValue = primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
        else if (primitiveValue->getValueID() == CSSValueEachLine)
            textIndentLineValue = TextIndentEachLine;
        else if (primitiveValue->getValueID() == CSSValueHanging)
            textIndentTypeValue = TextIndentHanging;
        else
            ASSERT_NOT_REACHED();
    }

    state.style()->setTextIndent(lengthOrPercentageValue);
    state.style()->setTextIndentLine(textIndentLineValue);
    state.style()->setTextIndentType(textIndentTypeValue);
}

void StyleBuilderFunctions::applyValueCSSPropertyTransform(StyleResolverState& state, CSSValue* value)
{
    TransformOperations operations;
    TransformBuilder::createTransformOperations(value, state.cssToLengthConversionData(), operations);
    state.style()->setTransform(operations);
}

void StyleBuilderFunctions::applyInheritCSSPropertyVerticalAlign(StyleResolverState& state)
{
    EVerticalAlign verticalAlign = state.parentStyle()->verticalAlign();
    state.style()->setVerticalAlign(verticalAlign);
    if (verticalAlign == LENGTH)
        state.style()->setVerticalAlignLength(state.parentStyle()->verticalAlignLength());
}

void StyleBuilderFunctions::applyValueCSSPropertyVerticalAlign(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID()) {
        state.style()->setVerticalAlign(*primitiveValue);
        return;
    }

    state.style()->setVerticalAlignLength(primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData()));
}

static void resetEffectiveZoom(StyleResolverState& state)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    state.setEffectiveZoom(state.parentStyle() ? state.parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInitialCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(RenderStyle::initialZoom());
}

void StyleBuilderFunctions::applyInheritCSSPropertyZoom(StyleResolverState& state)
{
    resetEffectiveZoom(state);
    state.setZoom(state.parentStyle()->zoom());
}

void StyleBuilderFunctions::applyValueCSSPropertyZoom(StyleResolverState& state, CSSValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value->isPrimitiveValue());
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() == CSSValueNormal) {
        resetEffectiveZoom(state);
        state.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueReset) {
        state.setEffectiveZoom(RenderStyle::initialZoom());
        state.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->getValueID() == CSSValueDocument) {
        float docZoom = state.rootElementStyle() ? state.rootElementStyle()->zoom() : RenderStyle::initialZoom();
        state.setEffectiveZoom(docZoom);
        state.setZoom(docZoom);
    } else if (primitiveValue->isPercentage()) {
        resetEffectiveZoom(state);
        if (float percent = primitiveValue->getFloatValue())
            state.setZoom(percent / 100.0f);
    } else if (primitiveValue->isNumber()) {
        resetEffectiveZoom(state);
        if (float number = primitiveValue->getFloatValue())
            state.setZoom(number);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitBorderImage(StyleResolverState& state, CSSValue* value)
{
    NinePieceImage image;
    state.styleMap().mapNinePieceImage(state.style(), CSSPropertyWebkitBorderImage, value, image);
    state.style()->setBorderImage(image);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitClipPath(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setClipPath(nullptr);
        } else if (primitiveValue->isShape()) {
            state.style()->setClipPath(ShapeClipPathOperation::create(basicShapeForValue(state, primitiveValue->getShapeValue())));
        } else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_URI) {
            String cssURLValue = primitiveValue->getStringValue();
            KURL url = state.document().completeURL(cssURLValue);
            // FIXME: It doesn't work with forward or external SVG references (see https://bugs.webkit.org/show_bug.cgi?id=90405)
            state.style()->setClipPath(ReferenceClipPathOperation::create(cssURLValue, AtomicString(url.fragmentIdentifier())));
        }
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitFilter(StyleResolverState& state, CSSValue* value)
{
    FilterOperations operations;
    if (FilterOperationResolver::createFilterOperations(value, state.cssToLengthConversionData(), operations, state))
        state.style()->setFilter(operations);
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
    state.style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
    state.style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state)
{
    state.style()->setTextEmphasisFill(state.parentStyle()->textEmphasisFill());
    state.style()->setTextEmphasisMark(state.parentStyle()->textEmphasisMark());
    state.style()->setTextEmphasisCustomMark(state.parentStyle()->textEmphasisCustomMark());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextEmphasisStyle(StyleResolverState& state, CSSValue* value)
{
    if (value->isValueList()) {
        CSSValueList* list = toCSSValueList(value);
        ASSERT(list->length() == 2);
        if (list->length() != 2)
            return;
        for (unsigned i = 0; i < 2; ++i) {
            CSSValue* item = list->item(i);
            if (!item->isPrimitiveValue())
                continue;

            CSSPrimitiveValue* value = toCSSPrimitiveValue(item);
            if (value->getValueID() == CSSValueFilled || value->getValueID() == CSSValueOpen)
                state.style()->setTextEmphasisFill(*value);
            else
                state.style()->setTextEmphasisMark(*value);
        }
        state.style()->setTextEmphasisCustomMark(nullAtom);
        return;
    }

    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->isString()) {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(TextEmphasisMarkCustom);
        state.style()->setTextEmphasisCustomMark(AtomicString(primitiveValue->getStringValue()));
        return;
    }

    state.style()->setTextEmphasisCustomMark(nullAtom);

    if (primitiveValue->getValueID() == CSSValueFilled || primitiveValue->getValueID() == CSSValueOpen) {
        state.style()->setTextEmphasisFill(*primitiveValue);
        state.style()->setTextEmphasisMark(TextEmphasisMarkAuto);
    } else {
        state.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        state.style()->setTextEmphasisMark(*primitiveValue);
    }
}

void StyleBuilderFunctions::applyInitialCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(false);
    state.style()->setWillChangeScrollPosition(false);
    state.style()->setWillChangeProperties(Vector<CSSPropertyID>());
    state.style()->setSubtreeWillChangeContents(state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWillChange(StyleResolverState& state)
{
    state.style()->setWillChangeContents(state.parentStyle()->willChangeContents());
    state.style()->setWillChangeScrollPosition(state.parentStyle()->willChangeScrollPosition());
    state.style()->setWillChangeProperties(state.parentStyle()->willChangeProperties());
    state.style()->setSubtreeWillChangeContents(state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyValueCSSPropertyWillChange(StyleResolverState& state, CSSValue* value)
{
    ASSERT(value->isValueList());
    bool willChangeContents = false;
    bool willChangeScrollPosition = false;
    Vector<CSSPropertyID> willChangeProperties;

    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(i.value());
        if (CSSPropertyID propertyID = primitiveValue->getPropertyID())
            willChangeProperties.append(propertyID);
        else if (primitiveValue->getValueID() == CSSValueContents)
            willChangeContents = true;
        else if (primitiveValue->getValueID() == CSSValueScrollPosition)
            willChangeScrollPosition = true;
        else
            ASSERT_NOT_REACHED();
    }
    state.style()->setWillChangeContents(willChangeContents);
    state.style()->setWillChangeScrollPosition(willChangeScrollPosition);
    state.style()->setWillChangeProperties(willChangeProperties);
    state.style()->setSubtreeWillChangeContents(willChangeContents || state.parentStyle()->subtreeWillChangeContents());
}

void StyleBuilderFunctions::applyInitialCSSPropertyContent(StyleResolverState& state)
{
    state.style()->clearContent();
}

void StyleBuilderFunctions::applyInheritCSSPropertyContent(StyleResolverState&)
{
    // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
    // note is a reminder that eventually "inherit" needs to be supported.
}

void StyleBuilderFunctions::applyValueCSSPropertyContent(StyleResolverState& state, CSSValue* value)
{
    // list of string, uri, counter, attr, i

    if (!value->isValueList())
        return;

    bool didSet = false;
    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        if (item->isImageGeneratorValue()) {
            if (item->isGradientValue())
                state.style()->setContent(StyleGeneratedImage::create(toCSSGradientValue(item)->gradientWithStylesResolved(state.document().textLinkColors(), state.style()->color()).get()), didSet);
            else
                state.style()->setContent(StyleGeneratedImage::create(toCSSImageGeneratorValue(item)), didSet);
            didSet = true;
        } else if (item->isImageSetValue()) {
            state.style()->setContent(state.elementStyleResources().setOrPendingFromValue(CSSPropertyContent, toCSSImageSetValue(item)), didSet);
            didSet = true;
        }

        if (item->isImageValue()) {
            state.style()->setContent(state.elementStyleResources().cachedOrPendingFromValue(state.document(), CSSPropertyContent, toCSSImageValue(item)), didSet);
            didSet = true;
            continue;
        }

        if (!item->isPrimitiveValue())
            continue;

        CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);

        if (contentValue->isString()) {
            state.style()->setContent(contentValue->getStringValue().impl(), didSet);
            didSet = true;
        } else if (contentValue->isAttr()) {
            // FIXME: Can a namespace be specified for an attr(foo)?
            if (state.style()->styleType() == NOPSEUDO)
                state.style()->setUnique();
            else
                state.parentStyle()->setUnique();
            QualifiedName attr(nullAtom, AtomicString(contentValue->getStringValue()), nullAtom);
            const AtomicString& value = state.element()->getAttribute(attr);
            state.style()->setContent(value.isNull() ? emptyString() : value.string(), didSet);
            didSet = true;
            // register the fact that the attribute value affects the style
            state.contentAttrValues().append(attr.localName());
        } else if (contentValue->isCounter()) {
            Counter* counterValue = contentValue->getCounterValue();
            EListStyleType listStyleType = NoneListStyle;
            CSSValueID listStyleIdent = counterValue->listStyleIdent();
            if (listStyleIdent != CSSValueNone)
                listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
            OwnPtr<CounterContent> counter = adoptPtr(new CounterContent(AtomicString(counterValue->identifier()), listStyleType, AtomicString(counterValue->separator())));
            state.style()->setContent(counter.release(), didSet);
            didSet = true;
        } else {
            switch (contentValue->getValueID()) {
            case CSSValueOpenQuote:
                state.style()->setContent(OPEN_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueCloseQuote:
                state.style()->setContent(CLOSE_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueNoOpenQuote:
                state.style()->setContent(NO_OPEN_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueNoCloseQuote:
                state.style()->setContent(NO_CLOSE_QUOTE, didSet);
                didSet = true;
                break;
            default:
                // normal and none do not have any effect.
                { }
            }
        }
    }
    if (!didSet)
        state.style()->clearContent();
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitLocale(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueAuto)
        state.style()->setLocale(nullAtom);
    else
        state.style()->setLocale(AtomicString(primitiveValue->getStringValue()));
    state.fontBuilder().setScript(state.style()->locale());
}

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitAppRegion(StyleResolverState&)
{
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitAppRegion(StyleResolverState&)
{
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitAppRegion(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue->getValueID())
        return;
    state.style()->setDraggableRegionMode(primitiveValue->getValueID() == CSSValueDrag ? DraggableRegionDrag : DraggableRegionNoDrag);
    state.document().setHasAnnotatedRegions(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitWritingMode(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        WritingMode writingMode = *toCSSPrimitiveValue(value);
        state.setWritingMode(writingMode);
        if (writingMode != TopToBottomWritingMode)
            state.document().setContainsAnyRareWritingMode(true);
    }

    // FIXME: It is not ok to modify document state while applying style.
    if (state.element() && state.element() == state.document().documentElement())
        state.document().setWritingModeSetOnDocumentElement(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextOrientation(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue())
        state.setTextOrientation(*toCSSPrimitiveValue(value));
}

void StyleBuilderFunctions::applyInheritCSSPropertyBaselineShift(StyleResolverState& state)
{
    const SVGRenderStyle& parentSvgStyle = state.parentStyle()->svgStyle();
    EBaselineShift baselineShift = parentSvgStyle.baselineShift();
    SVGRenderStyle& svgStyle = state.style()->accessSVGStyle();
    svgStyle.setBaselineShift(baselineShift);
    if (baselineShift == BS_LENGTH)
        svgStyle.setBaselineShiftValue(parentSvgStyle.baselineShiftValue());
}

void StyleBuilderFunctions::applyValueCSSPropertyBaselineShift(StyleResolverState& state, CSSValue* value)
{
    SVGRenderStyle& svgStyle = state.style()->accessSVGStyle();
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue->isValueID()) {
        svgStyle.setBaselineShift(BS_LENGTH);
        svgStyle.setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
        return;
    }
    switch (primitiveValue->getValueID()) {
    case CSSValueBaseline:
        svgStyle.setBaselineShift(BS_BASELINE);
        return;
    case CSSValueSub:
        svgStyle.setBaselineShift(BS_SUB);
        return;
    case CSSValueSuper:
        svgStyle.setBaselineShift(BS_SUPER);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace blink
