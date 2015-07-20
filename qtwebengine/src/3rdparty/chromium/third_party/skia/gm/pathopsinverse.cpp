/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkBitmap.h"
#include "SkPath.h"
#include "SkPathOps.h"
#include "SkRect.h"

namespace skiagm {

class PathOpsInverseGM : public GM {
public:
    PathOpsInverseGM() {
    }

protected:
    virtual void onOnceBeforeDraw() SK_OVERRIDE {
        const unsigned oneColor = 0xFF8080FF;
        const unsigned twoColor = 0x807F1f1f;
        SkColor blendColor = blend(oneColor, twoColor);
        makePaint(&fOnePaint, oneColor);
        makePaint(&fTwoPaint, twoColor);
        makePaint(&fOpPaint[kDifference_PathOp], oneColor);
        makePaint(&fOpPaint[kIntersect_PathOp], blendColor);
        makePaint(&fOpPaint[kUnion_PathOp], 0xFFc0FFc0);
        makePaint(&fOpPaint[kReverseDifference_PathOp], twoColor);
        makePaint(&fOpPaint[kXOR_PathOp], 0xFFa0FFe0);
        makePaint(&fOutlinePaint, 0xFF000000);
        fOutlinePaint.setStyle(SkPaint::kStroke_Style);
    }

    SkColor blend(SkColor one, SkColor two) {
        SkBitmap temp;
        temp.allocN32Pixels(1, 1);
        SkCanvas canvas(temp);
        canvas.drawColor(one);
        canvas.drawColor(two);
        void* pixels = temp.getPixels();
        return *(SkColor*) pixels;
    }

    void makePaint(SkPaint* paint, SkColor color) {
        paint->setAntiAlias(true);
        paint->setStyle(SkPaint::kFill_Style);
        paint->setColor(color);
    }

    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("pathopsinverse");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(1200, 900);
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkPath one, two;
        int yPos = 0;
        for (int oneFill = 0; oneFill <= 1; ++oneFill) {
            SkPath::FillType oneF = oneFill ? SkPath::kInverseEvenOdd_FillType
                    : SkPath::kEvenOdd_FillType;
            for (int twoFill = 0; twoFill <= 1; ++twoFill) {
                SkPath::FillType twoF = twoFill ? SkPath::kInverseEvenOdd_FillType
                        : SkPath::kEvenOdd_FillType;
                one.reset();
                one.setFillType(oneF);
                one.addRect(10, 10, 70, 70);
                two.reset();
                two.setFillType(twoF);
                two.addRect(40, 40, 100, 100);
                canvas->save();
                canvas->translate(0, SkIntToScalar(yPos));
                canvas->clipRect(SkRect::MakeWH(110, 110), SkRegion::kIntersect_Op, true);
                canvas->drawPath(one, fOnePaint);
                canvas->drawPath(one, fOutlinePaint);
                canvas->drawPath(two, fTwoPaint);
                canvas->drawPath(two, fOutlinePaint);
                canvas->restore();
                int xPos = 150;
                for (int op = kDifference_PathOp; op <= kReverseDifference_PathOp; ++op) {
                    SkPath result;
                    Op(one, two, (SkPathOp) op, &result);
                    canvas->save();
                    canvas->translate(SkIntToScalar(xPos), SkIntToScalar(yPos));
                    canvas->clipRect(SkRect::MakeWH(110, 110), SkRegion::kIntersect_Op, true);
                    canvas->drawPath(result, fOpPaint[op]);
                    canvas->drawPath(result, fOutlinePaint);
                    canvas->restore();
                    xPos += 150;
                }
                yPos += 150;
            }
        }
    }

private:
    SkPaint fOnePaint;
    SkPaint fTwoPaint;
    SkPaint fOutlinePaint;
    SkPaint fOpPaint[kReverseDifference_PathOp - kDifference_PathOp + 1];
    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static GM* MyFactory(void*) { return new PathOpsInverseGM; }
static GMRegistry reg(MyFactory);

}
