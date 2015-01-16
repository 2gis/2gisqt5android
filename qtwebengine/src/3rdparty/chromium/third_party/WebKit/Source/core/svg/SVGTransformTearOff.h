/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
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

#ifndef SVGTransformTearOff_h
#define SVGTransformTearOff_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGMatrixTearOff.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace WebCore {

class SVGTransformTearOff FINAL : public SVGPropertyTearOff<SVGTransform>, public ScriptWrappable {
public:
    enum SVGTransformType {
        SVG_TRANSFORM_UNKNOWN = WebCore::SVG_TRANSFORM_UNKNOWN,
        SVG_TRANSFORM_MATRIX = WebCore::SVG_TRANSFORM_MATRIX,
        SVG_TRANSFORM_TRANSLATE = WebCore::SVG_TRANSFORM_TRANSLATE,
        SVG_TRANSFORM_SCALE = WebCore::SVG_TRANSFORM_SCALE,
        SVG_TRANSFORM_ROTATE = WebCore::SVG_TRANSFORM_ROTATE,
        SVG_TRANSFORM_SKEWX = WebCore::SVG_TRANSFORM_SKEWX,
        SVG_TRANSFORM_SKEWY = WebCore::SVG_TRANSFORM_SKEWY,
    };

    static PassRefPtr<SVGTransformTearOff> create(PassRefPtr<SVGTransform> target, SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName = QualifiedName::null())
    {
        return adoptRef(new SVGTransformTearOff(target, contextElement, propertyIsAnimVal, attributeName));
    }

    virtual ~SVGTransformTearOff();

    unsigned short transformType() { return target()->transformType(); }
    SVGMatrixTearOff* matrix();
    float angle() { return target()->angle(); }

    void setMatrix(PassRefPtr<SVGMatrixTearOff>, ExceptionState&);
    void setTranslate(float tx, float ty, ExceptionState&);
    void setScale(float sx, float sy, ExceptionState&);
    void setRotate(float angle, float cx, float cy, ExceptionState&);
    void setSkewX(float, ExceptionState&);
    void setSkewY(float, ExceptionState&);

private:
    SVGTransformTearOff(PassRefPtr<SVGTransform>, SVGElement* contextElement, PropertyIsAnimValType, const QualifiedName& attributeName);

    RefPtr<SVGMatrixTearOff> m_matrixTearoff;
};

} // namespace WebCore

#endif // SVGTransformTearOff_h_
