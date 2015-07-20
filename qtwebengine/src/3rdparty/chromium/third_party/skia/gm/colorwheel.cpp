/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Resources.h"
#include "SkData.h"
#include "SkDecodingImageGenerator.h"
#include "gm.h"

static void checkerboard(
        SkCanvas* canvas, int w, int h, int size, SkColor c1, SkColor c2) {
    SkAutoCanvasRestore autoCanvasRestore(canvas, true);
    canvas->clipRect(SkRect::MakeWH(SkIntToScalar(w), SkIntToScalar(h)));
    canvas->drawColor(c1);
    SkPaint paint;
    paint.setColor(c2);
    SkScalar s = SkIntToScalar(size);
    for (int y = 0; y < h; y += size) {
        SkScalar ty = SkIntToScalar(y);
        bool oddRow = (y % (2 * size)) != 0;
        for (int x = oddRow ? size : 0; x < w; x += (2 * size)) {
            SkScalar tx = SkIntToScalar(x);
            canvas->drawRect(SkRect::MakeXYWH(tx, ty, s, s), paint);
        }
    }
}

static void draw_bitmap(SkCanvas* canvas, const char* resource, int x, int y) {
    SkBitmap bitmap;
    if (GetResourceAsBitmap(resource, &bitmap)) {
        canvas->drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y));
    } else {
        SkDebugf("\nCould not decode file '%s'. Did you forget"
                 " to set the resourcePath?\n", resource);
    }
}

/*
  This GM tests whether the image decoders properly decode each color
  channel.  Four copies of the same image should appear in the GM, and
  the letter R should be red, B should be blue, G green, C cyan, M
  magenta, Y yellow, and K black. In all but the JPEG version of the
  image, the letters should be on a white disc on a transparent
  background (rendered as a checkerboard).  The JPEG image has a grey
  background and compression artifacts.
 */
DEF_SIMPLE_GM(colorwheel, canvas, 256, 256) {
    canvas->clear(SK_ColorWHITE);
    checkerboard(canvas, 256, 556, 8, 0xFF999999, 0xFF666666);
    draw_bitmap(canvas, "color_wheel.png", 0, 0);  // top left
    draw_bitmap(canvas, "color_wheel.gif", 128, 0);  // top right
    draw_bitmap(canvas, "color_wheel.webp", 0, 128);  // bottom left
    draw_bitmap(canvas, "color_wheel.jpg", 128, 128);  // bottom right
}
