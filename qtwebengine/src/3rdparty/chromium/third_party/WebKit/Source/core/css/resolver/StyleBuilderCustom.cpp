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
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSProperty.h"
#include "core/css/Counter.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
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

namespace WebCore {

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
    ASSERT_WITH_MESSAGE(!isExpandedShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;
    if (primitiveValue && primitiveValue->getValueID() == CSSValueCurrentcolor)
        state.style()->setHasCurrentColor();

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    StyleBuilder::applyProperty(id, state, value, isInitial, isInherit);
}

static Length clipConvertToLength(StyleResolverState& state, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedConversion | PercentConversion | AutoConversion>(state.cssToLengthConversionData());
}

void StyleBuilderFunctions::applyInitialCSSPropertyClip(StyleResolverState& state)
{
    state.style()->setClip(Length(), Length(), Length(), Length());
    state.style()->setHasClip(false);
}

void StyleBuilderFunctions::applyInheritCSSPropertyClip(StyleResolverState& state)
{
    RenderStyle* parentStyle = state.parentStyle();
    if (!parentStyle->hasClip())
        return applyInitialCSSPropertyClip(state);
    state.style()->setClip(parentStyle->clipTop(), parentStyle->clipRight(), parentStyle->clipBottom(), parentStyle->clipLeft());
    state.style()->setHasClip(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyClip(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);

    if (primitiveValue->getValueID() == CSSValueAuto) {
        state.style()->setClip(Length(), Length(), Length(), Length());
        state.style()->setHasClip(false);
        return;
    }

    Rect* rect = primitiveValue->getRectValue();
    Length top = clipConvertToLength(state, rect->top());
    Length right = clipConvertToLength(state, rect->right());
    Length bottom = clipConvertToLength(state, rect->bottom());
    Length left = clipConvertToLength(state, rect->left());
    state.style()->setClip(top, right, bottom, left);
    state.style()->setHasClip(true);
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
        state.style()->setColor(state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color()));
    if (state.applyPropertyToVisitedLinkStyle())
        state.style()->setVisitedLinkColor(state.document().textLinkColors().colorFromPrimitiveValue(primitiveValue, state.style()->color(), true));
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
            CSSValue* item = list->itemWithoutBoundsCheck(i);
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

static inline bool isValidDisplayValue(StyleResolverState& state, EDisplay displayPropertyValue)
{
    if (state.element() && state.element()->isSVGElement() && state.style()->styleType() == NOPSEUDO)
        return (displayPropertyValue == INLINE || displayPropertyValue == BLOCK || displayPropertyValue == NONE);
    return true;
}

void StyleBuilderFunctions::applyInheritCSSPropertyDisplay(StyleResolverState& state)
{
    EDisplay display = state.parentStyle()->display();
    if (!isValidDisplayValue(state, display))
        return;
    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyValueCSSPropertyDisplay(StyleResolverState& state, CSSValue* value)
{
    EDisplay display = *toCSSPrimitiveValue(value);
    if (!isValidDisplayValue(state, display))
        return;
    state.style()->setDisplay(display);
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontFamily(StyleResolverState& state)
{
    state.fontBuilder().setFontFamilyInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontFamily(StyleResolverState& state)
{
    state.fontBuilder().setFontFamilyInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontFamily(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontFamilyValue(value);
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontSize(StyleResolverState& state)
{
    state.fontBuilder().setFontSizeInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontSize(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontSizeValue(value, state.parentStyle(), state.rootElementStyle());
}

void StyleBuilderFunctions::applyInitialCSSPropertyFontWeight(StyleResolverState& state)
{
    state.fontBuilder().setWeight(FontWeightNormal);
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontWeight(StyleResolverState& state)
{
    state.fontBuilder().setWeight(state.parentFontDescription().weight());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontWeight(StyleResolverState& state, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueInvalid:
        ASSERT_NOT_REACHED();
        break;
    case CSSValueBolder:
        state.fontBuilder().setWeight(state.parentStyle()->fontDescription().weight());
        state.fontBuilder().setWeightBolder();
        break;
    case CSSValueLighter:
        state.fontBuilder().setWeight(state.parentStyle()->fontDescription().weight());
        state.fontBuilder().setWeightLighter();
        break;
    default:
        state.fontBuilder().setWeight(*primitiveValue);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyGlyphOrientationVertical(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueAuto)
        state.style()->accessSVGStyle()->setGlyphOrientationVertical(GO_AUTO);
    else
        state.style()->accessSVGStyle()->setGlyphOrientationVertical(StyleBuilderConverter::convertGlyphOrientation(state, value));
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
    CSSValueListInspector inspector(value);
    switch (inspector.length()) {
    case 2: {
        // <length>{2} | <page-size> <orientation>
        if (!inspector.first()->isPrimitiveValue() || !inspector.second()->isPrimitiveValue())
            return;
        CSSPrimitiveValue* first = toCSSPrimitiveValue(inspector.first());
        CSSPrimitiveValue* second = toCSSPrimitiveValue(inspector.second());
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
        if (!inspector.first()->isPrimitiveValue())
            return;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(inspector.first());
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

void StyleBuilderFunctions::applyValueCSSPropertyTextDecoration(StyleResolverState& state, CSSValue* value)
{
    TextDecoration t = RenderStyle::initialTextDecoration();
    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        t |= *toCSSPrimitiveValue(item);
    }
    state.style()->setTextDecoration(t);
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

void StyleBuilderFunctions::applyInitialCSSPropertyTransformOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitTransformOriginX(state);
    applyInitialCSSPropertyWebkitTransformOriginY(state);
    applyInitialCSSPropertyWebkitTransformOriginZ(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyTransformOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitTransformOriginX(state);
    applyInheritCSSPropertyWebkitTransformOriginY(state);
    applyInheritCSSPropertyWebkitTransformOriginZ(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyTransformOrigin(StyleResolverState& state, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);
    ASSERT(list->length() == 3);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(list->item(0));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueLeft:
            state.style()->setTransformOriginX(Length(0, Percent));
            break;
        case CSSValueRight:
            state.style()->setTransformOriginX(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setTransformOriginX(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setTransformOriginX(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(1));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueTop:
            state.style()->setTransformOriginY(Length(0, Percent));
            break;
        case CSSValueBottom:
            state.style()->setTransformOriginY(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setTransformOriginY(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setTransformOriginY(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(2));
    state.style()->setTransformOriginZ(StyleBuilderConverter::convertComputedLength<float>(state, primitiveValue));
}

void StyleBuilderFunctions::applyInitialCSSPropertyPerspectiveOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitPerspectiveOriginX(state);
    applyInitialCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyPerspectiveOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitPerspectiveOriginX(state);
    applyInheritCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyPerspectiveOrigin(StyleResolverState& state, CSSValue* value)
{
    CSSValueList* list = toCSSValueList(value);
    ASSERT(list->length() == 2);
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(list->item(0));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueLeft:
            state.style()->setPerspectiveOriginX(Length(0, Percent));
            break;
        case CSSValueRight:
            state.style()->setPerspectiveOriginX(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setPerspectiveOriginX(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setPerspectiveOriginX(StyleBuilderConverter::convertLength(state, primitiveValue));
    }

    primitiveValue = toCSSPrimitiveValue(list->item(1));
    if (primitiveValue->isValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueTop:
            state.style()->setPerspectiveOriginY(Length(0, Percent));
            break;
        case CSSValueBottom:
            state.style()->setPerspectiveOriginY(Length(100, Percent));
            break;
        case CSSValueCenter:
            state.style()->setPerspectiveOriginY(Length(50, Percent));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        state.style()->setPerspectiveOriginY(StyleBuilderConverter::convertLength(state, primitiveValue));
    }
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

void StyleBuilderFunctions::applyValueCSSPropertyTouchAction(StyleResolverState& state, CSSValue* value)
{
    TouchAction action = RenderStyle::initialTouchAction();
    for (CSSValueListIterator i(value); i.hasMore(); i.advance())
        action |= *toCSSPrimitiveValue(i.value());

    state.style()->setTouchAction(action);
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

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitAspectRatio(StyleResolverState& state)
{
    state.style()->setHasAspectRatio(RenderStyle::initialHasAspectRatio());
    state.style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
    state.style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitAspectRatio(StyleResolverState& state)
{
    if (!state.parentStyle()->hasAspectRatio())
        return;
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(state.parentStyle()->aspectRatioDenominator());
    state.style()->setAspectRatioNumerator(state.parentStyle()->aspectRatioNumerator());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitAspectRatio(StyleResolverState& state, CSSValue* value)
{
    if (!value->isAspectRatioValue()) {
        state.style()->setHasAspectRatio(false);
        return;
    }
    CSSAspectRatioValue* aspectRatioValue = toCSSAspectRatioValue(value);
    state.style()->setHasAspectRatio(true);
    state.style()->setAspectRatioDenominator(aspectRatioValue->denominatorValue());
    state.style()->setAspectRatioNumerator(aspectRatioValue->numeratorValue());
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

void StyleBuilderFunctions::applyInitialCSSPropertyFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInitial();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFontVariantLigatures(StyleResolverState& state)
{
    state.fontBuilder().setFontVariantLigaturesInherit(state.parentFontDescription());
}

void StyleBuilderFunctions::applyValueCSSPropertyFontVariantLigatures(StyleResolverState& state, CSSValue* value)
{
    state.fontBuilder().setFontVariantLigaturesValue(value);
}

void StyleBuilderFunctions::applyValueCSSPropertyInternalMarqueeIncrement(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueSmall:
            state.style()->setMarqueeIncrement(Length(1, Fixed)); // 1px.
            break;
        case CSSValueNormal:
            state.style()->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
            break;
        case CSSValueLarge:
            state.style()->setMarqueeIncrement(Length(36, Fixed)); // 36px.
            break;
        default:
            break;
        }
    } else {
        Length marqueeLength = primitiveValue->convertToLength<FixedConversion | PercentConversion>(state.cssToLengthConversionData());
        state.style()->setMarqueeIncrement(marqueeLength);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyInternalMarqueeSpeed(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (CSSValueID valueID = primitiveValue->getValueID()) {
        switch (valueID) {
        case CSSValueSlow:
            state.style()->setMarqueeSpeed(500); // 500 msec.
            break;
        case CSSValueNormal:
            state.style()->setMarqueeSpeed(85); // 85msec. The WinIE default.
            break;
        case CSSValueFast:
            state.style()->setMarqueeSpeed(10); // 10msec. Super fast.
            break;
        default:
            break;
        }
    } else if (primitiveValue->isTime()) {
        state.style()->setMarqueeSpeed(primitiveValue->computeTime<int, CSSPrimitiveValue::Milliseconds>());
    } else if (primitiveValue->isNumber()) { // For scrollamount support.
        state.style()->setMarqueeSpeed(primitiveValue->getIntValue());
    }
}

// FIXME: We should use the same system for this as the rest of the pseudo-shorthands (e.g. background-position)
void StyleBuilderFunctions::applyInitialCSSPropertyWebkitPerspectiveOrigin(StyleResolverState& state)
{
    applyInitialCSSPropertyWebkitPerspectiveOriginX(state);
    applyInitialCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitPerspectiveOrigin(StyleResolverState& state)
{
    applyInheritCSSPropertyWebkitPerspectiveOriginX(state);
    applyInheritCSSPropertyWebkitPerspectiveOriginY(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitPerspectiveOrigin(StyleResolverState&, CSSValue* value)
{
    // This is expanded in the parser
    ASSERT_NOT_REACHED();
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTapHighlightColor(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    Color color = state.document().textLinkColors().colorFromPrimitiveValue(toCSSPrimitiveValue(value), state.style()->color());
    state.style()->setTapHighlightColor(color);
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
            CSSValue* item = list->itemWithoutBoundsCheck(i);
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

void StyleBuilderFunctions::applyValueCSSPropertyTextUnderlinePosition(StyleResolverState& state, CSSValue* value)
{
    // This is true if value is 'auto' or 'alphabetic'.
    if (value->isPrimitiveValue()) {
        TextUnderlinePosition t = *toCSSPrimitiveValue(value);
        state.style()->setTextUnderlinePosition(t);
        return;
    }

    unsigned t = 0;
    for (CSSValueListIterator i(value); i.hasMore(); i.advance()) {
        CSSValue* item = i.value();
        TextUnderlinePosition t2 = *toCSSPrimitiveValue(item);
        t |= t2;
    }
    state.style()->setTextUnderlinePosition(static_cast<TextUnderlinePosition>(t));
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

void StyleBuilderFunctions::applyInitialCSSPropertyFont(StyleResolverState&)
{
    ASSERT_NOT_REACHED();
}

void StyleBuilderFunctions::applyInheritCSSPropertyFont(StyleResolverState&)
{
    ASSERT_NOT_REACHED();
}

void StyleBuilderFunctions::applyValueCSSPropertyFont(StyleResolverState& state, CSSValue* value)
{
    // Only System Font identifiers should come through this method
    // all other values should have been handled when the shorthand
    // was expanded by the parser.
    // FIXME: System Font identifiers should not hijack this
    // short-hand CSSProperty like this (crbug.com/353932)
    state.style()->setLineHeight(RenderStyle::initialLineHeight());
    state.setLineHeightValue(0);
    state.fontBuilder().fromSystemFont(toCSSPrimitiveValue(value)->getValueID(), state.style()->effectiveZoom());
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

void StyleBuilderFunctions::applyInitialCSSPropertyWebkitPerspective(StyleResolverState& state)
{
    applyInitialCSSPropertyPerspective(state);
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitPerspective(StyleResolverState& state)
{
    applyInheritCSSPropertyPerspective(state);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitPerspective(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->isNumber()) {
        float perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLength<float>(state.cssToLengthConversionData());
        if (perspectiveValue >= 0.0f)
            state.style()->setPerspective(perspectiveValue);
    } else {
        applyValueCSSPropertyPerspective(state, value);
    }
}

void StyleBuilderFunctions::applyValueCSSPropertyPerspective(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueNone) {
        state.style()->setPerspective(0);
        return;
    }

    if (!primitiveValue->isLength())
        return;
    float perspectiveValue = primitiveValue->computeLength<float>(state.cssToLengthConversionData());
    if (perspectiveValue >= 0.0f)
        state.style()->setPerspective(perspectiveValue);
}

void StyleBuilderFunctions::applyInitialCSSPropertyInternalCallback(StyleResolverState& state)
{
}

void StyleBuilderFunctions::applyInheritCSSPropertyInternalCallback(StyleResolverState& state)
{
}

void StyleBuilderFunctions::applyValueCSSPropertyInternalCallback(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueInternalPresence)
        state.style()->addCallbackSelector(state.currentRule()->selectorList().selectorsText());
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitWritingMode(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue())
        state.setWritingMode(*toCSSPrimitiveValue(value));

    // FIXME: It is not ok to modify document state while applying style.
    if (state.element() && state.element() == state.document().documentElement())
        state.document().setWritingModeSetOnDocumentElement(true);
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitTextOrientation(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue())
        state.setTextOrientation(*toCSSPrimitiveValue(value));
}

// FIXME: We should handle initial and inherit for font-feature-settings
void StyleBuilderFunctions::applyInitialCSSPropertyWebkitFontFeatureSettings(StyleResolverState& state)
{
}

void StyleBuilderFunctions::applyInheritCSSPropertyWebkitFontFeatureSettings(StyleResolverState& state)
{
}

void StyleBuilderFunctions::applyValueCSSPropertyWebkitFontFeatureSettings(StyleResolverState& state, CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->getValueID() == CSSValueNormal) {
        state.fontBuilder().setFeatureSettingsNormal();
        return;
    }

    if (value->isValueList())
        state.fontBuilder().setFeatureSettingsValue(value);
}


void StyleBuilderFunctions::applyValueCSSPropertyBaselineShift(StyleResolverState& state, CSSValue* value)
{
    if (!value->isPrimitiveValue())
        return;

    SVGRenderStyle* svgStyle = state.style()->accessSVGStyle();
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID()) {
        switch (primitiveValue->getValueID()) {
        case CSSValueBaseline:
            svgStyle->setBaselineShift(BS_BASELINE);
            break;
        case CSSValueSub:
            svgStyle->setBaselineShift(BS_SUB);
            break;
        case CSSValueSuper:
            svgStyle->setBaselineShift(BS_SUPER);
            break;
        default:
            break;
        }
    } else {
        svgStyle->setBaselineShift(BS_LENGTH);
        svgStyle->setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
    }
}

} // namespace WebCore
