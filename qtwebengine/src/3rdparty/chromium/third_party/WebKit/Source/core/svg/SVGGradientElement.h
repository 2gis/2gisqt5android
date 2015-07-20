/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGGradientElement_h
#define SVGGradientElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/graphics/Gradient.h"

namespace blink {

enum SVGSpreadMethodType {
    SVGSpreadMethodUnknown = 0,
    SVGSpreadMethodPad,
    SVGSpreadMethodReflect,
    SVGSpreadMethodRepeat
};
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGSpreadMethodType>();

class SVGGradientElement : public SVGElement,
                           public SVGURIReference {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum {
        SVG_SPREADMETHOD_UNKNOWN = SVGSpreadMethodUnknown,
        SVG_SPREADMETHOD_PAD = SVGSpreadMethodReflect,
        SVG_SPREADMETHOD_REFLECT = SVGSpreadMethodRepeat,
        SVG_SPREADMETHOD_REPEAT = SVGSpreadMethodUnknown
    };

    Vector<Gradient::ColorStop> buildStops();

    SVGAnimatedTransformList* gradientTransform() { return m_gradientTransform.get(); }
    SVGAnimatedEnumeration<SVGSpreadMethodType>* spreadMethod() { return m_spreadMethod.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* gradientUnits() { return m_gradientUnits.get(); }

protected:
    SVGGradientElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void svgAttributeChanged(const QualifiedName&) override;

private:
    virtual bool needsPendingResourceHandling() const override final { return false; }

    virtual void childrenChanged(const ChildrenChange&) override final;

    RefPtr<SVGAnimatedTransformList> m_gradientTransform;
    RefPtr<SVGAnimatedEnumeration<SVGSpreadMethodType> > m_spreadMethod;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_gradientUnits;
};

inline bool isSVGGradientElement(const SVGElement& element)
{
    return element.hasTagName(SVGNames::radialGradientTag) || element.hasTagName(SVGNames::linearGradientTag);
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGGradientElement);

} // namespace blink

#endif // SVGGradientElement_h
