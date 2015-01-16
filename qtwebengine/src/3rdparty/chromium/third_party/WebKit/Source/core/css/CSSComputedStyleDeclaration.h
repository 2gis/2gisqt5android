/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef CSSComputedStyleDeclaration_h
#define CSSComputedStyleDeclaration_h

#include "core/css/CSSStyleDeclaration.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSPrimitiveValue;
class CSSValueList;
class ExceptionState;
class MutableStylePropertySet;
class Node;
class RenderObject;
class RenderStyle;
class SVGPaint;
class ShadowData;
class ShadowList;
class StyleColor;
class StylePropertySet;
class StylePropertyShorthand;

enum EUpdateLayout { DoNotUpdateLayout = false, UpdateLayout = true };

class CSSComputedStyleDeclaration FINAL : public CSSStyleDeclaration {
public:
    static PassRefPtrWillBeRawPtr<CSSComputedStyleDeclaration> create(PassRefPtrWillBeRawPtr<Node> node, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRefWillBeNoop(new CSSComputedStyleDeclaration(node, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

#if !ENABLE(OILPAN)
    virtual void ref() OVERRIDE;
    virtual void deref() OVERRIDE;
#endif

    String getPropertyValue(CSSPropertyID) const;
    bool getPropertyPriority(CSSPropertyID) const;

    virtual PassRefPtrWillBeRawPtr<MutableStylePropertySet> copyProperties() const OVERRIDE;

    PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValue(CSSPropertyID, EUpdateLayout = UpdateLayout) const;
    PassRefPtrWillBeRawPtr<CSSValue> getFontSizeCSSValuePreferringKeyword() const;
    bool useFixedFontDefaultSize() const;
    PassRefPtrWillBeRawPtr<CSSValue> getSVGPropertyCSSValue(CSSPropertyID, EUpdateLayout) const;

    PassRefPtrWillBeRawPtr<MutableStylePropertySet> copyPropertiesInSet(const Vector<CSSPropertyID>&) const;

    virtual void trace(Visitor*) OVERRIDE;

private:
    CSSComputedStyleDeclaration(PassRefPtrWillBeRawPtr<Node>, bool allowVisitedStyle, const String&);

    // The styled node is either the node passed into getComputedStyle, or the
    // PseudoElement for :before and :after if they exist.
    // FIXME: This should be styledElement since in JS getComputedStyle only works
    // on Elements, but right now editing creates these for text nodes. We should fix
    // that.
    Node* styledNode() const;

    // CSSOM functions. Don't make these public.
    virtual CSSRule* parentRule() const OVERRIDE;
    virtual unsigned length() const OVERRIDE;
    virtual String item(unsigned index) const OVERRIDE;
    PassRefPtr<RenderStyle> computeRenderStyle(CSSPropertyID) const;
    virtual PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValue(const String& propertyName) OVERRIDE;
    virtual String getPropertyValue(const String& propertyName) OVERRIDE;
    virtual String getPropertyPriority(const String& propertyName) OVERRIDE;
    virtual String getPropertyShorthand(const String& propertyName) OVERRIDE;
    virtual bool isPropertyImplicit(const String& propertyName) OVERRIDE;
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&) OVERRIDE;
    virtual String removeProperty(const String& propertyName, ExceptionState&) OVERRIDE;
    virtual String cssText() const OVERRIDE;
    virtual void setCSSText(const String&, ExceptionState&) OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) OVERRIDE;
    virtual String getPropertyValueInternal(CSSPropertyID) OVERRIDE;
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionState&) OVERRIDE;

    virtual bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const OVERRIDE;

    PassRefPtrWillBeRawPtr<CSSValue> valueForShadowData(const ShadowData&, const RenderStyle&, bool useSpread) const;
    PassRefPtrWillBeRawPtr<CSSValue> valueForShadowList(const ShadowList*, const RenderStyle&, bool useSpread) const;
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle&, const StyleColor&) const;
    PassRefPtrWillBeRawPtr<SVGPaint> adjustSVGPaintForCurrentColor(PassRefPtrWillBeRawPtr<SVGPaint>, RenderStyle&) const;

    PassRefPtrWillBeRawPtr<CSSValue> valueForFilter(const RenderObject*, const RenderStyle&) const;

    PassRefPtrWillBeRawPtr<CSSValueList> valuesForShorthandProperty(const StylePropertyShorthand&) const;
    PassRefPtrWillBeRawPtr<CSSValueList> valuesForSidesShorthand(const StylePropertyShorthand&) const;
    PassRefPtrWillBeRawPtr<CSSValueList> valuesForBackgroundShorthand() const;
    PassRefPtrWillBeRawPtr<CSSValueList> valuesForGridShorthand(const StylePropertyShorthand&) const;

    RefPtrWillBeMember<Node> m_node;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
#if !ENABLE(OILPAN)
    unsigned m_refCount;
#endif
};

} // namespace WebCore

#endif // CSSComputedStyleDeclaration_h
