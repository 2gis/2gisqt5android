/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTextContext_DEFINED
#define GrTextContext_DEFINED

#include "GrGlyph.h"
#include "GrPaint.h"
#include "SkDeviceProperties.h"

#include "SkPostConfig.h"

class GrContext;
class GrDrawTarget;
class GrFontScaler;

/*
 * This class wraps the state for a single text render
 */
class GrTextContext {
public:
    virtual ~GrTextContext();

    bool drawText(const GrPaint&, const SkPaint&, const char text[], size_t byteLength,
                  SkScalar x, SkScalar y);
    bool drawPosText(const GrPaint&, const SkPaint&,
                     const char text[], size_t byteLength,
                     const SkScalar pos[], int scalarsPerPosition,
                     const SkPoint& offset);

protected:
    GrTextContext*     fFallbackTextContext;
    GrContext*         fContext;
    SkDeviceProperties fDeviceProperties;

    GrDrawTarget*      fDrawTarget;
    SkIRect            fClipRect;
    GrPaint            fPaint;
    SkPaint            fSkPaint;

    GrTextContext(GrContext*, const SkDeviceProperties&);

    virtual bool canDraw(const SkPaint& paint) = 0;

    virtual void onDrawText(const GrPaint&, const SkPaint&, const char text[], size_t byteLength,
                            SkScalar x, SkScalar y) = 0;
    virtual void onDrawPosText(const GrPaint&, const SkPaint&,
                               const char text[], size_t byteLength,
                               const SkScalar pos[], int scalarsPerPosition,
                               const SkPoint& offset) = 0;

    void init(const GrPaint&, const SkPaint&);
    void finish() { fDrawTarget = NULL; }

    static GrFontScaler* GetGrFontScaler(SkGlyphCache* cache);
    // sets extent in stopVector and returns glyph count
    static int MeasureText(SkGlyphCache* cache, SkDrawCacheProc glyphCacheProc,
                           const char text[], size_t byteLength, SkVector* stopVector);
};

#endif
