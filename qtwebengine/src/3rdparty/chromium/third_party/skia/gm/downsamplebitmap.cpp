/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"

#include "Resources.h"
#include "SkGradientShader.h"
#include "SkTypeface.h"
#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkPaint.h"

static void setTypeface(SkPaint* paint, const char name[], SkTypeface::Style style) {
    sk_tool_utils::set_portable_typeface(paint, name, style);
}

class DownsampleBitmapGM : public skiagm::GM {
public:
    SkBitmap    fBM;
    SkString    fName;
    bool        fBitmapMade;
    SkPaint::FilterLevel fFilterLevel;

    DownsampleBitmapGM(SkPaint::FilterLevel filterLevel)
        : fFilterLevel(filterLevel)
    {
        this->setBGColor(0xFFDDDDDD);
        fBitmapMade = false;
    }

    const char* filterLevelToString() {
        static const char *filterLevelNames[] = {
            "none", "low", "medium", "high"
        };
        return filterLevelNames[fFilterLevel];
    }

protected:
    virtual uint32_t onGetFlags() const SK_OVERRIDE {
        return kSkipTiled_Flag;
    }

    virtual SkString onShortName() SK_OVERRIDE {
        return fName;
    }

    virtual SkISize onISize() SK_OVERRIDE {
        make_bitmap_wrapper();
        return SkISize::Make(fBM.width(), 4 * fBM.height());
    }

    void make_bitmap_wrapper() {
        if (!fBitmapMade) {
            fBitmapMade = true;
            make_bitmap();
        }
    }

    virtual void make_bitmap() = 0;

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        make_bitmap_wrapper();

        int curY = 0;
        int curHeight;
        float curScale = 1;
        do {

            SkMatrix matrix;
            matrix.setScale( curScale, curScale );

            SkPaint paint;
            paint.setFilterLevel(fFilterLevel);

            canvas->save();
            canvas->translate(0, (SkScalar)curY);
            canvas->drawBitmapMatrix( fBM, matrix, &paint );
            canvas->restore();

            curHeight = (int) (fBM.height() * curScale + 2);
            curY += curHeight;
            curScale *= 0.75f;
        } while (curHeight >= 2 && curY < 4*fBM.height());
    }

private:
    typedef skiagm::GM INHERITED;
};

class DownsampleBitmapTextGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapTextGM(float textSize, SkPaint::FilterLevel filterLevel)
      : INHERITED(filterLevel), fTextSize(textSize)
        {
            fName.printf("downsamplebitmap_text_%s_%.2fpt", this->filterLevelToString(), fTextSize);
        }

  protected:
      float fTextSize;

      virtual void make_bitmap() SK_OVERRIDE {
          fBM.allocN32Pixels(int(fTextSize * 8), int(fTextSize * 6));
          SkCanvas canvas(fBM);
          canvas.drawColor(SK_ColorWHITE);

          SkPaint paint;
          paint.setAntiAlias(true);
          paint.setSubpixelText(true);
          paint.setTextSize(fTextSize);

          setTypeface(&paint, "Times", SkTypeface::kNormal);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 1.2f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kBold);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 2.4f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kItalic);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 3.6f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kBoldItalic);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 4.8f*fTextSize, paint);
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

class DownsampleBitmapCheckerboardGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapCheckerboardGM(int size, int numChecks, SkPaint::FilterLevel filterLevel)
      : INHERITED(filterLevel), fSize(size), fNumChecks(numChecks)
        {
            fName.printf("downsamplebitmap_checkerboard_%s_%d_%d", this->filterLevelToString(), fSize, fNumChecks);
        }

  protected:
      int fSize;
      int fNumChecks;

      virtual void make_bitmap() SK_OVERRIDE {
          fBM.allocN32Pixels(fSize, fSize);
          for (int y = 0; y < fSize; ++y) {
              for (int x = 0; x < fSize; ++x) {
                  SkPMColor* s = fBM.getAddr32(x, y);
                  int cx = (x * fNumChecks) / fSize;
                  int cy = (y * fNumChecks) / fSize;
                  if ((cx+cy)%2) {
                      *s = 0xFFFFFFFF;
                  } else {
                      *s = 0xFF000000;
                  }
              }
          }
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

class DownsampleBitmapImageGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapImageGM(const char filename[], SkPaint::FilterLevel filterLevel)
      : INHERITED(filterLevel), fFilename(filename)
        {
            fName.printf("downsamplebitmap_image_%s_%s", this->filterLevelToString(), filename);
        }

  protected:
      SkString fFilename;
      int fSize;

      virtual void make_bitmap() SK_OVERRIDE {
          SkImageDecoder* codec = NULL;
          SkString resourcePath = GetResourcePath(fFilename.c_str());
          SkFILEStream stream(resourcePath.c_str());
          if (stream.isValid()) {
              codec = SkImageDecoder::Factory(&stream);
          }
          if (codec) {
              stream.rewind();
              codec->decode(&stream, &fBM, kN32_SkColorType, SkImageDecoder::kDecodePixels_Mode);
              SkDELETE(codec);
          } else {
              fBM.allocN32Pixels(1, 1);
              *(fBM.getAddr32(0,0)) = 0xFF0000FF; // red == bad
          }
          fSize = fBM.height();
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM( return new DownsampleBitmapTextGM(72, SkPaint::kHigh_FilterLevel); )
DEF_GM( return new DownsampleBitmapCheckerboardGM(512,256, SkPaint::kHigh_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_512.png", SkPaint::kHigh_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_132x132_12x12.astc",
                                            SkPaint::kHigh_FilterLevel); )

DEF_GM( return new DownsampleBitmapTextGM(72, SkPaint::kMedium_FilterLevel); )
DEF_GM( return new DownsampleBitmapCheckerboardGM(512,256, SkPaint::kMedium_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_512.png", SkPaint::kMedium_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_132x132_12x12.astc",
                                           SkPaint::kMedium_FilterLevel); )

DEF_GM( return new DownsampleBitmapTextGM(72, SkPaint::kLow_FilterLevel); )
DEF_GM( return new DownsampleBitmapCheckerboardGM(512,256, SkPaint::kLow_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_512.png", SkPaint::kLow_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_132x132_12x12.astc",
                                           SkPaint::kLow_FilterLevel); )

DEF_GM( return new DownsampleBitmapTextGM(72, SkPaint::kNone_FilterLevel); )
DEF_GM( return new DownsampleBitmapCheckerboardGM(512,256, SkPaint::kNone_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_512.png", SkPaint::kNone_FilterLevel); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_132x132_12x12.astc",
                                           SkPaint::kNone_FilterLevel); )
