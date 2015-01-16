/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkAlphaThresholdFilter.h"
#include "SkRandom.h"

#define WIDTH 500
#define HEIGHT 500

namespace skiagm {

class ImageAlphaThresholdGM : public GM {
public:
    ImageAlphaThresholdGM() {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    virtual uint32_t onGetFlags() const SK_OVERRIDE {
        return this->INHERITED::onGetFlags() |
               GM::kSkipTiled_Flag |
               GM::kSkipPicture_Flag |
               GM::kSkipPipe_Flag |
               GM::kSkipPipeCrossProcess_Flag;
    }

    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("imagealphathreshold");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(WIDTH, HEIGHT);
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkIRect rects[2];
        rects[0] = SkIRect::MakeXYWH(0, 150, WIDTH, HEIGHT - 300);
        rects[1] = SkIRect::MakeXYWH(150, 0, WIDTH - 300, HEIGHT);
        SkRegion region;
        region.setRects(rects, 2);

        SkMatrix matrix;
        matrix.reset();
        matrix.setTranslate(WIDTH * .1f, HEIGHT * .1f);
        matrix.postScale(.8f, .8f);

        canvas->concat(matrix);

        SkPaint paint;
        paint.setImageFilter(
            SkAlphaThresholdFilter::Create(region, 0.2f, 0.7f))->unref();
        canvas->saveLayer(NULL, &paint);
        paint.setAntiAlias(true);

        SkPaint rect_paint;
        rect_paint.setColor(0xFF0000FF);
        canvas->drawRect(SkRect::MakeXYWH(0, 0, WIDTH / 2, HEIGHT / 2),
                         rect_paint);
        rect_paint.setColor(0xBFFF0000);
        canvas->drawRect(SkRect::MakeXYWH(WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2),
                         rect_paint);
        rect_paint.setColor(0x3F00FF00);
        canvas->drawRect(SkRect::MakeXYWH(0, HEIGHT / 2, WIDTH / 2, HEIGHT / 2),
                         rect_paint);
        rect_paint.setColor(0x00000000);
        canvas->drawRect(SkRect::MakeXYWH(WIDTH / 2, HEIGHT / 2, WIDTH / 2, HEIGHT / 2),
                         rect_paint);

        canvas->restore();
    }

private:
    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static GM* MyFactory(void*) { return new ImageAlphaThresholdGM; }
static GMRegistry reg(MyFactory);

}
