
/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "gm.h"

#include "Resources.h"
#include "SkBitmap.h"
#include "SkImageDecoder.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "SkStream.h"


 /***
  *
  * This GM reproduces Skia bug 2904, in which a tiled bitmap shader was failing to draw correctly
  * when fractional image scaling was ignored by the high quality bitmap scaler.
  *
  ***/

namespace skiagm {

class TiledScaledBitmapGM : public GM {
public:

    TiledScaledBitmapGM() {
    }

protected:
    virtual SkString onShortName() {
        return SkString("tiledscaledbitmap");
    }

    virtual SkISize onISize() {
        return SkISize::Make(1016, 616);
    }

    virtual uint32_t onGetFlags() const SK_OVERRIDE {
        return kSkipTiled_Flag;
    }

    static SkBitmap make_bm(int width, int height) { 
        SkBitmap bm;
        bm.allocN32Pixels(width, height);
        bm.eraseColor(SK_ColorTRANSPARENT);
        SkCanvas canvas(bm);
        SkPaint paint;
        paint.setAntiAlias(true);
        canvas.drawCircle(width/2.f, height/2.f, width/4.f, paint);
        return bm; 
    }

    virtual void onOnceBeforeDraw() SK_OVERRIDE {
        fBitmap = make_bm(360, 288);
    }

    virtual void onDraw(SkCanvas* canvas) {
        SkPaint paint;

        paint.setAntiAlias(true);
        paint.setFilterLevel(SkPaint::kHigh_FilterLevel);

        SkMatrix mat;
        mat.setScale(121.f/360.f, 93.f/288.f);
        mat.postTranslate(-72, -72);

        SkShader *shader = SkShader::CreateBitmapShader(fBitmap, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &mat);
        paint.setShader(shader);

        SkSafeUnref(shader);
        canvas->drawRectCoords(8,8,1008, 608, paint);
    }

private:
    SkBitmap fBitmap;

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return SkNEW(TiledScaledBitmapGM);)

}
