/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkPath.h"

class ClipStrokeRectGM : public skiagm::GM {
public:
    ClipStrokeRectGM() {

    }

protected:
    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("clip_strokerect");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(200, 400);
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkPaint p;
        p.setColor(SK_ColorRED);
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(22);

        SkRect r = SkRect::MakeXYWH(20, 20, 100, 100);
        // setting the height of this to 19 causes failure
        SkRect rect = SkRect::MakeXYWH(20, 0, 100, 20);

        canvas->save();
        canvas->clipRect(rect, SkRegion::kReplace_Op, true);
        canvas->drawRect(r, p);
        canvas->restore();

        p.setColor(SK_ColorBLUE);
        p.setStrokeWidth(2);
        canvas->drawRect(rect, p);

        p.setColor(SK_ColorRED);
        p.setAntiAlias(true);
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(22);

        SkRect r2 = SkRect::MakeXYWH(20, 140, 100, 100);
        // setting the height of this to 19 causes failure
        SkRect rect2 = SkRect::MakeXYWH(20, 120, 100, 19);

        canvas->save();
        canvas->clipRect(rect2, SkRegion::kReplace_Op, true);
        canvas->drawRect(r2, p);
        canvas->restore();

        p.setColor(SK_ColorBLUE);
        p.setStrokeWidth(2);
        canvas->drawRect(rect2, p);
    }

    virtual uint32_t onGetFlags() const { return kSkipPipe_Flag; }

private:
    typedef skiagm::GM INHERITED;
};

DEF_GM( return SkNEW(ClipStrokeRectGM); )

