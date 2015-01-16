/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkPath.h"
#include "SkSurface.h"
#include "SkPicture.h"

static void draw_content(SkCanvas* canvas) {
    SkImageInfo info = canvas->imageInfo();
    SkPaint paint;
    paint.setAntiAlias(true);
    canvas->drawCircle(SkScalarHalf(info.width()), SkScalarHalf(info.height()),
                       SkScalarHalf(info.width()), paint);
}

class PeekPixelsGM : public skiagm::GM {
public:
    PeekPixelsGM() {}

protected:
    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("peekpixels");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(360, 120);
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
        SkAutoTUnref<SkSurface> surface(canvas->newSurface(info));
        if (surface.get()) {
            SkCanvas* surfCanvas = surface->getCanvas();

            draw_content(surfCanvas);
            SkBitmap bitmap;

            // test peekPixels
            {
                SkImageInfo info;
                size_t rowBytes;
                const void* addr = surfCanvas->peekPixels(&info, &rowBytes);
                if (addr && bitmap.installPixels(info, const_cast<void*>(addr), rowBytes)) {
                    canvas->drawBitmap(bitmap, 0, 0, NULL);
                }
            }

            // test ROCanvasPixels
            canvas->translate(120, 0);
            SkAutoROCanvasPixels ropixels(surfCanvas);
            if (ropixels.asROBitmap(&bitmap)) {
                canvas->drawBitmap(bitmap, 0, 0, NULL);
            }

            // test Surface
            canvas->translate(120, 0);
            surface->draw(canvas, 0, 0, NULL);
        }
    }

    virtual uint32_t onGetFlags() const {
        // we explicitly test peekPixels and readPixels, neither of which
        // return something for a picture-backed canvas, so we skip that test.
        return kSkipPicture_Flag;
    }

private:
    typedef skiagm::GM INHERITED;
};

DEF_GM( return SkNEW(PeekPixelsGM); )
