/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrBitmapTextContext_DEFINED
#define GrBitmapTextContext_DEFINED

#include "GrTextContext.h"

class GrGeometryProcessor;
class GrTextStrike;

/*
 * This class implements GrTextContext using standard bitmap fonts
 */
class GrBitmapTextContext : public GrTextContext {
public:
    static GrBitmapTextContext* Create(GrContext*, const SkDeviceProperties&);

    virtual ~GrBitmapTextContext();

private:
    GrTextStrike*                     fStrike;
    void*                             fVertices;
    int                               fCurrVertex;
    int                               fAllocVertexCount;
    int                               fTotalVertexCount;
    SkRect                            fVertexBounds;
    GrTexture*                        fCurrTexture;
    GrMaskFormat                      fCurrMaskFormat;
    SkAutoTUnref<GrGeometryProcessor> fCachedGeometryProcessor;
    // Used to check whether fCachedEffect is still valid.
    uint32_t                          fEffectTextureUniqueID;

    GrBitmapTextContext(GrContext*, const SkDeviceProperties&);

    virtual bool canDraw(const SkPaint& paint) SK_OVERRIDE;

    virtual void onDrawText(const GrPaint&, const SkPaint&, const char text[], size_t byteLength,
                            SkScalar x, SkScalar y) SK_OVERRIDE;
    virtual void onDrawPosText(const GrPaint&, const SkPaint&,
                               const char text[], size_t byteLength,
                               const SkScalar pos[], int scalarsPerPosition,
                               const SkPoint& offset) SK_OVERRIDE;

    void init(const GrPaint&, const SkPaint&);
    void appendGlyph(GrGlyph::PackedID, SkFixed left, SkFixed top, GrFontScaler*);
    void flush();                 // automatically called by destructor
    void finish();

};

#endif
