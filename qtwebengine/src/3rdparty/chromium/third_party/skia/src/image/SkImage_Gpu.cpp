/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImage_Base.h"
#include "SkImagePriv.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "GrContext.h"
#include "GrTexture.h"

class SkImage_Gpu : public SkImage_Base {
public:
    SK_DECLARE_INST_COUNT(SkImage_Gpu)

    explicit SkImage_Gpu(const SkBitmap&);
    virtual ~SkImage_Gpu();

    virtual void onDraw(SkCanvas*, SkScalar x, SkScalar y, const SkPaint*) const SK_OVERRIDE;
    virtual void onDrawRect(SkCanvas*, const SkRect* src, const SkRect& dst,
                                  const SkPaint*) const SK_OVERRIDE;
    virtual GrTexture* onGetTexture() const SK_OVERRIDE;
    virtual bool getROPixels(SkBitmap*) const SK_OVERRIDE;

    GrTexture* getTexture() const { return fBitmap.getTexture(); }

    virtual SkShader* onNewShader(SkShader::TileMode,
                                  SkShader::TileMode,
                                  const SkMatrix* localMatrix) const SK_OVERRIDE;

    virtual bool isOpaque() const SK_OVERRIDE;

private:
    SkBitmap    fBitmap;

    typedef SkImage_Base INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

SkImage_Gpu::SkImage_Gpu(const SkBitmap& bitmap)
    : INHERITED(bitmap.width(), bitmap.height())
    , fBitmap(bitmap) {
    SkASSERT(fBitmap.getTexture());
}

SkImage_Gpu::~SkImage_Gpu() {
}

SkShader* SkImage_Gpu::onNewShader(SkShader::TileMode tileX,
                                   SkShader::TileMode tileY,
                                   const SkMatrix* localMatrix) const
{
    return SkShader::CreateBitmapShader(fBitmap, tileX, tileY, localMatrix);
}

void SkImage_Gpu::onDraw(SkCanvas* canvas, SkScalar x, SkScalar y, const SkPaint* paint) const {
    canvas->drawBitmap(fBitmap, x, y, paint);
}

void SkImage_Gpu::onDrawRect(SkCanvas* canvas, const SkRect* src, const SkRect& dst,
                                   const SkPaint* paint) const {
    canvas->drawBitmapRectToRect(fBitmap, src, dst, paint);
}

GrTexture* SkImage_Gpu::onGetTexture() const {
    return fBitmap.getTexture();
}

bool SkImage_Gpu::getROPixels(SkBitmap* dst) const {
    return fBitmap.copyTo(dst, kN32_SkColorType);
}

bool SkImage_Gpu::isOpaque() const {
    return fBitmap.isOpaque();
}

///////////////////////////////////////////////////////////////////////////////

SkImage* SkImage::NewTexture(const SkBitmap& bitmap) {
    if (NULL == bitmap.getTexture()) {
        return NULL;
    }

    return SkNEW_ARGS(SkImage_Gpu, (bitmap));
}

GrTexture* SkTextureImageGetTexture(SkImage* image) {
    return ((SkImage_Gpu*)image)->getTexture();
}
