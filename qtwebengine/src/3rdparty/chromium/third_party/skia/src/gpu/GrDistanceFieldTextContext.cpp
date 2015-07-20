/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrDistanceFieldTextContext.h"
#include "GrAtlas.h"
#include "GrBitmapTextContext.h"
#include "GrDrawTarget.h"
#include "GrDrawTargetCaps.h"
#include "GrFontScaler.h"
#include "GrGpu.h"
#include "GrIndexBuffer.h"
#include "GrStrokeInfo.h"
#include "GrTexturePriv.h"
#include "GrTextStrike.h"
#include "GrTextStrike_impl.h"

#include "SkAutoKern.h"
#include "SkColorFilter.h"
#include "SkDistanceFieldGen.h"
#include "SkDraw.h"
#include "SkGlyphCache.h"
#include "SkGpuDevice.h"
#include "SkPath.h"
#include "SkRTConf.h"
#include "SkStrokeRec.h"
#include "effects/GrDistanceFieldTextureEffect.h"

SK_CONF_DECLARE(bool, c_DumpFontCache, "gpu.dumpFontCache", false,
                "Dump the contents of the font cache before every purge.");

static const int kSmallDFFontSize = 32;
static const int kSmallDFFontLimit = 32;
static const int kMediumDFFontSize = 78;
static const int kMediumDFFontLimit = 78;
static const int kLargeDFFontSize = 192;

namespace {
// position + texture coord
extern const GrVertexAttrib gTextVertexAttribs[] = {
    {kVec2f_GrVertexAttribType, 0,                kPosition_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType, sizeof(SkPoint) , kGeometryProcessor_GrVertexAttribBinding}
};

static const size_t kTextVASize = 2 * sizeof(SkPoint); 

// position + color + texture coord
extern const GrVertexAttrib gTextVertexWithColorAttribs[] = {
    {kVec2f_GrVertexAttribType,  0,                                 kPosition_GrVertexAttribBinding},
    {kVec4ub_GrVertexAttribType, sizeof(SkPoint),                   kColor_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType,  sizeof(SkPoint) + sizeof(GrColor), kGeometryProcessor_GrVertexAttribBinding}
};
    
static const size_t kTextVAColorSize = 2 * sizeof(SkPoint) + sizeof(GrColor); 

static const int kVerticesPerGlyph = 4;
static const int kIndicesPerGlyph = 6;
};

GrDistanceFieldTextContext::GrDistanceFieldTextContext(GrContext* context,
                                                       const SkDeviceProperties& properties,
                                                       bool enable)
                                                    : GrTextContext(context, properties) {
#if SK_FORCE_DISTANCEFIELD_FONTS
    fEnableDFRendering = true;
#else
    fEnableDFRendering = enable;
#endif
    fStrike = NULL;
    fGammaTexture = NULL;

    fEffectTextureUniqueID = SK_InvalidUniqueID;
    fEffectColor = GrColor_ILLEGAL;
    fEffectFlags = kInvalid_DistanceFieldEffectFlag;

    fVertices = NULL;
    fCurrVertex = 0;
    fAllocVertexCount = 0;
    fTotalVertexCount = 0;
    fCurrTexture = NULL;

    fVertexBounds.setLargestInverted();
}

GrDistanceFieldTextContext* GrDistanceFieldTextContext::Create(GrContext* context,
                                                               const SkDeviceProperties& props,
                                                               bool enable) {
    GrDistanceFieldTextContext* textContext = SkNEW_ARGS(GrDistanceFieldTextContext, 
                                                         (context, props, enable));
    textContext->fFallbackTextContext = GrBitmapTextContext::Create(context, props);

    return textContext;
}

GrDistanceFieldTextContext::~GrDistanceFieldTextContext() {
    this->finish();
    SkSafeSetNull(fGammaTexture);
}

bool GrDistanceFieldTextContext::canDraw(const SkPaint& paint) {
    if (!fEnableDFRendering && !paint.isDistanceFieldTextTEMP()) {
        return false;
    }

    // rasterizers and mask filters modify alpha, which doesn't
    // translate well to distance
    if (paint.getRasterizer() || paint.getMaskFilter() ||
        !fContext->getTextTarget()->caps()->shaderDerivativeSupport()) {
        return false;
    }

    // TODO: add some stroking support
    if (paint.getStyle() != SkPaint::kFill_Style) {
        return false;
    }

    // TODO: choose an appropriate maximum scale for distance fields and
    //       enable perspective
    if (SkDraw::ShouldDrawTextAsPaths(paint, fContext->getMatrix())) {
        return false;
    }

    return true;
}

inline void GrDistanceFieldTextContext::init(const GrPaint& paint, const SkPaint& skPaint) {
    GrTextContext::init(paint, skPaint);

    fStrike = NULL;

    const SkMatrix& ctm = fContext->getMatrix();

    // getMaxScale doesn't support perspective, so neither do we at the moment
    SkASSERT(!ctm.hasPerspective());
    SkScalar maxScale = ctm.getMaxScale();
    SkScalar textSize = fSkPaint.getTextSize();
    SkScalar scaledTextSize = textSize;
    // if we have non-unity scale, we need to choose our base text size
    // based on the SkPaint's text size multiplied by the max scale factor
    // TODO: do we need to do this if we're scaling down (i.e. maxScale < 1)?
    if (maxScale > 0 && !SkScalarNearlyEqual(maxScale, SK_Scalar1)) {
        scaledTextSize *= maxScale;
    }

    fVertices = NULL;
    fCurrVertex = 0;
    fAllocVertexCount = 0;
    fTotalVertexCount = 0;

    if (scaledTextSize <= kSmallDFFontLimit) {
        fTextRatio = textSize / kSmallDFFontSize;
        fSkPaint.setTextSize(SkIntToScalar(kSmallDFFontSize));
#if DEBUG_TEXT_SIZE
        fSkPaint.setColor(SkColorSetARGB(0xFF, 0x00, 0x00, 0xFF));
        fPaint.setColor(GrColorPackRGBA(0x00, 0x00, 0xFF, 0xFF));
#endif
    } else if (scaledTextSize <= kMediumDFFontLimit) {
        fTextRatio = textSize / kMediumDFFontSize;
        fSkPaint.setTextSize(SkIntToScalar(kMediumDFFontSize));
#if DEBUG_TEXT_SIZE
        fSkPaint.setColor(SkColorSetARGB(0xFF, 0x00, 0xFF, 0x00));
        fPaint.setColor(GrColorPackRGBA(0x00, 0xFF, 0x00, 0xFF));
#endif
    } else {
        fTextRatio = textSize / kLargeDFFontSize;
        fSkPaint.setTextSize(SkIntToScalar(kLargeDFFontSize));
#if DEBUG_TEXT_SIZE
        fSkPaint.setColor(SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00));
        fPaint.setColor(GrColorPackRGBA(0xFF, 0x00, 0x00, 0xFF));
#endif
    }

    fUseLCDText = fSkPaint.isLCDRenderText();

    fSkPaint.setLCDRenderText(false);
    fSkPaint.setAutohinted(false);
    fSkPaint.setHinting(SkPaint::kNormal_Hinting);
    fSkPaint.setSubpixelText(true);
}

static void setup_gamma_texture(GrContext* context, const SkGlyphCache* cache,
                                const SkDeviceProperties& deviceProperties,
                                GrTexture** gammaTexture) {
    if (NULL == *gammaTexture) {
        int width, height;
        size_t size;

#ifdef SK_GAMMA_CONTRAST
        SkScalar contrast = SK_GAMMA_CONTRAST;
#else
        SkScalar contrast = 0.5f;
#endif
        SkScalar paintGamma = deviceProperties.gamma();
        SkScalar deviceGamma = deviceProperties.gamma();

        size = SkScalerContext::GetGammaLUTSize(contrast, paintGamma, deviceGamma,
                                                &width, &height);

        SkAutoTArray<uint8_t> data((int)size);
        SkScalerContext::GetGammaLUTData(contrast, paintGamma, deviceGamma, data.get());

        // TODO: Update this to use the cache rather than directly creating a texture.
        GrSurfaceDesc desc;
        desc.fFlags = kNone_GrSurfaceFlags;
        desc.fWidth = width;
        desc.fHeight = height;
        desc.fConfig = kAlpha_8_GrPixelConfig;

        *gammaTexture = context->getGpu()->createTexture(desc, NULL, 0);
        if (NULL == *gammaTexture) {
            return;
        }

        (*gammaTexture)->writePixels(0, 0, width, height,
                                     (*gammaTexture)->config(), data.get(), 0,
                                     GrContext::kDontFlush_PixelOpsFlag);
    }
}

void GrDistanceFieldTextContext::onDrawText(const GrPaint& paint, const SkPaint& skPaint,
                                          const char text[], size_t byteLength,
                                          SkScalar x, SkScalar y) {
    SkASSERT(byteLength == 0 || text != NULL);

    // nothing to draw
    if (text == NULL || byteLength == 0) {
        return;
    }

    SkDrawCacheProc          glyphCacheProc = skPaint.getDrawCacheProc();
    SkAutoGlyphCache         autoCache(skPaint, &fDeviceProperties, NULL);
    SkGlyphCache*            cache = autoCache.getCache();

    SkTArray<SkScalar> positions;

    const char* textPtr = text;
    SkFixed stopX = 0;
    SkFixed stopY = 0;
    SkFixed origin;
    switch (skPaint.getTextAlign()) {
        case SkPaint::kRight_Align: origin = SK_Fixed1; break;
        case SkPaint::kCenter_Align: origin = SK_FixedHalf; break;
        case SkPaint::kLeft_Align: origin = 0; break;
        default: SkFAIL("Invalid paint origin"); return;
    }

    SkAutoKern autokern;
    const char* stop = text + byteLength;
    while (textPtr < stop) {
        // don't need x, y here, since all subpixel variants will have the
        // same advance
        const SkGlyph& glyph = glyphCacheProc(cache, &textPtr, 0, 0);

        SkFixed width = glyph.fAdvanceX + autokern.adjust(glyph);
        positions.push_back(SkFixedToScalar(stopX + SkFixedMul_portable(origin, width)));

        SkFixed height = glyph.fAdvanceY;
        positions.push_back(SkFixedToScalar(stopY + SkFixedMul_portable(origin, height)));

        stopX += width;
        stopY += height;
    }
    SkASSERT(textPtr == stop);

    // now adjust starting point depending on alignment
    SkScalar alignX = SkFixedToScalar(stopX);
    SkScalar alignY = SkFixedToScalar(stopY);
    if (skPaint.getTextAlign() == SkPaint::kCenter_Align) {
        alignX = SkScalarHalf(alignX);
        alignY = SkScalarHalf(alignY);
    } else if (skPaint.getTextAlign() == SkPaint::kLeft_Align) {
        alignX = 0;
        alignY = 0;
    }
    x -= alignX;
    y -= alignY;
    SkPoint offset = SkPoint::Make(x, y);

    this->drawPosText(paint, skPaint, text, byteLength, positions.begin(), 2, offset);
}

void GrDistanceFieldTextContext::onDrawPosText(const GrPaint& paint, const SkPaint& skPaint,
                                             const char text[], size_t byteLength,
                                             const SkScalar pos[], int scalarsPerPosition,
                                             const SkPoint& offset) {

    SkASSERT(byteLength == 0 || text != NULL);
    SkASSERT(1 == scalarsPerPosition || 2 == scalarsPerPosition);

    // nothing to draw
    if (text == NULL || byteLength == 0 /* no raster clip? || fRC->isEmpty()*/) {
        return;
    }

    this->init(paint, skPaint);

    SkDrawCacheProc glyphCacheProc = fSkPaint.getDrawCacheProc();

    SkAutoGlyphCacheNoGamma    autoCache(fSkPaint, &fDeviceProperties, NULL);
    SkGlyphCache*              cache = autoCache.getCache();
    GrFontScaler*              fontScaler = GetGrFontScaler(cache);

    setup_gamma_texture(fContext, cache, fDeviceProperties, &fGammaTexture);

    int numGlyphs = fSkPaint.textToGlyphs(text, byteLength, NULL);
    fTotalVertexCount = kVerticesPerGlyph*numGlyphs;

    const char*        stop = text + byteLength;
    SkTArray<char>     fallbackTxt;
    SkTArray<SkScalar> fallbackPos;

    if (SkPaint::kLeft_Align == fSkPaint.getTextAlign()) {
        while (text < stop) {
            const char* lastText = text;
            // the last 2 parameters are ignored
            const SkGlyph& glyph = glyphCacheProc(cache, &text, 0, 0);

            if (glyph.fWidth) {
                SkScalar x = offset.x() + pos[0];
                SkScalar y = offset.y() + (2 == scalarsPerPosition ? pos[1] : 0);

                if (!this->appendGlyph(GrGlyph::Pack(glyph.getGlyphID(),
                                                     glyph.getSubXFixed(),
                                                     glyph.getSubYFixed()),
                                       x, y, fontScaler)) {
                    // couldn't append, send to fallback
                    fallbackTxt.push_back_n(text-lastText, lastText);
                    fallbackPos.push_back(pos[0]);
                    if (2 == scalarsPerPosition) {
                        fallbackPos.push_back(pos[1]);
                    }
                }
            }
            pos += scalarsPerPosition;
        }
    } else {
        SkScalar alignMul = SkPaint::kCenter_Align == fSkPaint.getTextAlign() ? SK_ScalarHalf
                                                                              : SK_Scalar1;
        while (text < stop) {
            const char* lastText = text;
            // the last 2 parameters are ignored
            const SkGlyph& glyph = glyphCacheProc(cache, &text, 0, 0);

            if (glyph.fWidth) {
                SkScalar x = offset.x() + pos[0];
                SkScalar y = offset.y() + (2 == scalarsPerPosition ? pos[1] : 0);

                SkScalar advanceX = SkFixedToScalar(glyph.fAdvanceX)*alignMul*fTextRatio;
                SkScalar advanceY = SkFixedToScalar(glyph.fAdvanceY)*alignMul*fTextRatio;

                if (!this->appendGlyph(GrGlyph::Pack(glyph.getGlyphID(),
                                                     glyph.getSubXFixed(),
                                                     glyph.getSubYFixed()),
                                       x - advanceX, y - advanceY, fontScaler)) {
                    // couldn't append, send to fallback
                    fallbackTxt.push_back_n(text-lastText, lastText);
                    fallbackPos.push_back(pos[0]);
                    if (2 == scalarsPerPosition) {
                        fallbackPos.push_back(pos[1]);
                    }
                }
            }
            pos += scalarsPerPosition;
        }
    }

    this->finish();
    
    if (fallbackTxt.count() > 0) {
        fFallbackTextContext->drawPosText(paint, skPaint, fallbackTxt.begin(), fallbackTxt.count(),
                                          fallbackPos.begin(), scalarsPerPosition, offset);
    }
}

static inline GrColor skcolor_to_grcolor_nopremultiply(SkColor c) {
    unsigned r = SkColorGetR(c);
    unsigned g = SkColorGetG(c);
    unsigned b = SkColorGetB(c);
    return GrColorPackRGBA(r, g, b, 0xff);
}

static void* alloc_vertices(GrDrawTarget* drawTarget, int numVertices, bool useColorVerts) {
    if (numVertices <= 0) {
        return NULL;
    }

    // set up attributes
    if (useColorVerts) {
        drawTarget->drawState()->setVertexAttribs<gTextVertexWithColorAttribs>(
                                    SK_ARRAY_COUNT(gTextVertexWithColorAttribs), kTextVAColorSize);
    } else {
        drawTarget->drawState()->setVertexAttribs<gTextVertexAttribs>(
                                    SK_ARRAY_COUNT(gTextVertexAttribs), kTextVASize);
    }
    void* vertices = NULL;
    bool success = drawTarget->reserveVertexAndIndexSpace(numVertices,
                                                          0,
                                                          &vertices,
                                                          NULL);
    GrAlwaysAssert(success);
    return vertices;
}

void GrDistanceFieldTextContext::setupCoverageEffect(const SkColor& filteredColor) {
    GrTextureParams params(SkShader::kRepeat_TileMode, GrTextureParams::kBilerp_FilterMode);
    GrTextureParams gammaParams(SkShader::kClamp_TileMode, GrTextureParams::kNone_FilterMode);
    
    uint32_t textureUniqueID = fCurrTexture->getUniqueID();
    const SkMatrix& ctm = fContext->getMatrix();
    
    // set up any flags
    uint32_t flags = 0;
    flags |= ctm.isSimilarity() ? kSimilarity_DistanceFieldEffectFlag : 0;
    flags |= fUseLCDText ? kUseLCD_DistanceFieldEffectFlag : 0;
    flags |= fUseLCDText && ctm.rectStaysRect() ?
    kRectToRect_DistanceFieldEffectFlag : 0;
    bool useBGR = SkPixelGeometryIsBGR(fDeviceProperties.pixelGeometry());
    flags |= fUseLCDText && useBGR ? kBGR_DistanceFieldEffectFlag : 0;
    
    // see if we need to create a new effect
    if (textureUniqueID != fEffectTextureUniqueID ||
        filteredColor != fEffectColor ||
        flags != fEffectFlags) {
        if (fUseLCDText) {
            GrColor colorNoPreMul = skcolor_to_grcolor_nopremultiply(filteredColor);
            fCachedGeometryProcessor.reset(GrDistanceFieldLCDTextureEffect::Create(fCurrTexture,
                                                                                   params,
                                                                                   fGammaTexture,
                                                                                   gammaParams,
                                                                                   colorNoPreMul,
                                                                                   flags));
        } else {
#ifdef SK_GAMMA_APPLY_TO_A8
            U8CPU lum = SkColorSpaceLuminance::computeLuminance(fDeviceProperties.gamma(),
                                                                filteredColor);
            fCachedGeometryProcessor.reset(GrDistanceFieldTextureEffect::Create(fCurrTexture,
                                                                                params,
                                                                                fGammaTexture,
                                                                                gammaParams,
                                                                                lum/255.f,
                                                                                flags));
#else
            fCachedGeometryProcessor.reset(GrDistanceFieldNoGammaTextureEffect::Create(fCurrTexture,
                                                                                params, flags));
#endif
        }
        fEffectTextureUniqueID = textureUniqueID;
        fEffectColor = filteredColor;
        fEffectFlags = flags;
    }
    
}

// Returns true if this method handled the glyph, false if needs to be passed to fallback
//
bool GrDistanceFieldTextContext::appendGlyph(GrGlyph::PackedID packed,
                                             SkScalar sx, SkScalar sy,
                                             GrFontScaler* scaler) {
    if (NULL == fDrawTarget) {
        return true;
    }

    if (NULL == fStrike) {
        fStrike = fContext->getFontCache()->getStrike(scaler, true);
    }

    GrGlyph* glyph = fStrike->getGlyph(packed, scaler);
    if (NULL == glyph || glyph->fBounds.isEmpty()) {
        return true;
    }

    // fallback to color glyph support
    if (kA8_GrMaskFormat != glyph->fMaskFormat) {
        return false;
    }

    SkScalar dx = SkIntToScalar(glyph->fBounds.fLeft + SK_DistanceFieldInset);
    SkScalar dy = SkIntToScalar(glyph->fBounds.fTop + SK_DistanceFieldInset);
    SkScalar width = SkIntToScalar(glyph->fBounds.width() - 2*SK_DistanceFieldInset);
    SkScalar height = SkIntToScalar(glyph->fBounds.height() - 2*SK_DistanceFieldInset);

    SkScalar scale = fTextRatio;
    dx *= scale;
    dy *= scale;
    sx += dx;
    sy += dy;
    width *= scale;
    height *= scale;
    SkRect glyphRect = SkRect::MakeXYWH(sx, sy, width, height);

    // check if we clipped out
    SkRect dstRect;
    const SkMatrix& ctm = fContext->getMatrix();
    (void) ctm.mapRect(&dstRect, glyphRect);
    if (fClipRect.quickReject(SkScalarTruncToInt(dstRect.left()),
                              SkScalarTruncToInt(dstRect.top()),
                              SkScalarTruncToInt(dstRect.right()),
                              SkScalarTruncToInt(dstRect.bottom()))) {
//            SkCLZ(3);    // so we can set a break-point in the debugger
        return true;
    }

    if (NULL == glyph->fPlot) {
        if (!fStrike->glyphTooLargeForAtlas(glyph)) {
            if (fStrike->addGlyphToAtlas(glyph, scaler)) {
                goto HAS_ATLAS;
            }

            // try to clear out an unused plot before we flush
            if (fContext->getFontCache()->freeUnusedPlot(fStrike, glyph) &&
                fStrike->addGlyphToAtlas(glyph, scaler)) {
                goto HAS_ATLAS;
            }

            if (c_DumpFontCache) {
#ifdef SK_DEVELOPER
                fContext->getFontCache()->dump();
#endif
            }

            // before we purge the cache, we must flush any accumulated draws
            this->flush();
            fContext->flush();

            // we should have an unused plot now
            if (fContext->getFontCache()->freeUnusedPlot(fStrike, glyph) &&
                fStrike->addGlyphToAtlas(glyph, scaler)) {
                goto HAS_ATLAS;
            }
        }

        if (NULL == glyph->fPath) {
            SkPath* path = SkNEW(SkPath);
            if (!scaler->getGlyphPath(glyph->glyphID(), path)) {
                // flag the glyph as being dead?
                delete path;
                return true;
            }
            glyph->fPath = path;
        }

        // flush any accumulated draws before drawing this glyph as a path.
        this->flush();

        GrContext::AutoMatrix am;
        SkMatrix ctm;
        ctm.setScale(fTextRatio, fTextRatio);
        ctm.postTranslate(sx - dx, sy - dy);
        GrPaint tmpPaint(fPaint);
        am.setPreConcat(fContext, ctm, &tmpPaint);
        GrStrokeInfo strokeInfo(SkStrokeRec::kFill_InitStyle);
        fContext->drawPath(tmpPaint, *glyph->fPath, strokeInfo);

        // remove this glyph from the vertices we need to allocate
        fTotalVertexCount -= kVerticesPerGlyph;
        return true;
    }

HAS_ATLAS:
    SkASSERT(glyph->fPlot);
    GrDrawTarget::DrawToken drawToken = fDrawTarget->getCurrentDrawToken();
    glyph->fPlot->setDrawToken(drawToken);

    GrTexture* texture = glyph->fPlot->texture();
    SkASSERT(texture);

    if (fCurrTexture != texture || fCurrVertex + kVerticesPerGlyph > fTotalVertexCount) {
        this->flush();
        fCurrTexture = texture;
        fCurrTexture->ref();
    }

    bool useColorVerts = !fUseLCDText;

    if (NULL == fVertices) {
        int maxQuadVertices = kVerticesPerGlyph * fContext->getQuadIndexBuffer()->maxQuads();
        fAllocVertexCount = SkMin32(fTotalVertexCount, maxQuadVertices);
        fVertices = alloc_vertices(fDrawTarget, fAllocVertexCount, useColorVerts);
    }

    SkFixed tx = SkIntToFixed(glyph->fAtlasLocation.fX + SK_DistanceFieldInset);
    SkFixed ty = SkIntToFixed(glyph->fAtlasLocation.fY + SK_DistanceFieldInset);
    SkFixed tw = SkIntToFixed(glyph->fBounds.width() - 2*SK_DistanceFieldInset);
    SkFixed th = SkIntToFixed(glyph->fBounds.height() - 2*SK_DistanceFieldInset);

    fVertexBounds.joinNonEmptyArg(glyphRect);

    size_t vertSize = fUseLCDText ? (2 * sizeof(SkPoint))
                                  : (2 * sizeof(SkPoint) + sizeof(GrColor));
    
    SkASSERT(vertSize == fDrawTarget->getDrawState().getVertexStride());

    SkPoint* positions = reinterpret_cast<SkPoint*>(
                               reinterpret_cast<intptr_t>(fVertices) + vertSize * fCurrVertex);
    positions->setRectFan(glyphRect.fLeft, glyphRect.fTop, glyphRect.fRight, glyphRect.fBottom,
                          vertSize);

    // The texture coords are last in both the with and without color vertex layouts.
    SkPoint* textureCoords = reinterpret_cast<SkPoint*>(
                               reinterpret_cast<intptr_t>(positions) + vertSize  - sizeof(SkPoint));
    textureCoords->setRectFan(SkFixedToFloat(texture->texturePriv().normalizeFixedX(tx)),
                              SkFixedToFloat(texture->texturePriv().normalizeFixedY(ty)),
                              SkFixedToFloat(texture->texturePriv().normalizeFixedX(tx + tw)),
                              SkFixedToFloat(texture->texturePriv().normalizeFixedY(ty + th)),
                              vertSize);
    if (useColorVerts) {
        if (0xFF == GrColorUnpackA(fPaint.getColor())) {
            fDrawTarget->drawState()->setHint(GrDrawState::kVertexColorsAreOpaque_Hint, true);
        }
        // color comes after position.
        GrColor* colors = reinterpret_cast<GrColor*>(positions + 1);
        for (int i = 0; i < 4; ++i) {
            *colors = fPaint.getColor();
            colors = reinterpret_cast<GrColor*>(reinterpret_cast<intptr_t>(colors) + vertSize);
        }
    }

    fCurrVertex += 4;
    
    return true;
}

void GrDistanceFieldTextContext::flush() {
    if (NULL == fDrawTarget) {
        return;
    }

    GrDrawState* drawState = fDrawTarget->drawState();
    GrDrawState::AutoRestoreEffects are(drawState);

    drawState->setFromPaint(fPaint, fContext->getMatrix(), fContext->getRenderTarget());

    if (fCurrVertex > 0) {
        // setup our sampler state for our text texture/atlas
        SkASSERT(SkIsAlign4(fCurrVertex));

        // get our current color
        SkColor filteredColor;
        SkColorFilter* colorFilter = fSkPaint.getColorFilter();
        if (colorFilter) {
            filteredColor = colorFilter->filterColor(fSkPaint.getColor());
        } else {
            filteredColor = fSkPaint.getColor();
        }
        this->setupCoverageEffect(filteredColor);

        // Effects could be stored with one of the cache objects (atlas?)
        drawState->setGeometryProcessor(fCachedGeometryProcessor.get());

        // Set draw state
        if (fUseLCDText) {
            GrColor colorNoPreMul = skcolor_to_grcolor_nopremultiply(filteredColor);
            if (kOne_GrBlendCoeff != fPaint.getSrcBlendCoeff() ||
                kISA_GrBlendCoeff != fPaint.getDstBlendCoeff() ||
                fPaint.numColorStages()) {
                SkDebugf("LCD Text will not draw correctly.\n");
            }
            SkASSERT(!drawState->hasColorVertexAttribute());
            // We don't use the GrPaint's color in this case because it's been premultiplied by
            // alpha. Instead we feed in a non-premultiplied color, and multiply its alpha by
            // the mask texture color. The end result is that we get
            //            mask*paintAlpha*paintColor + (1-mask*paintAlpha)*dstColor
            int a = SkColorGetA(fSkPaint.getColor());
            // paintAlpha
            drawState->setColor(SkColorSetARGB(a, a, a, a));
            // paintColor
            drawState->setBlendConstant(colorNoPreMul);
            drawState->setBlendFunc(kConstC_GrBlendCoeff, kISC_GrBlendCoeff);
        } else {
            // set back to normal in case we took LCD path previously.
            drawState->setBlendFunc(fPaint.getSrcBlendCoeff(), fPaint.getDstBlendCoeff());
            // We're using per-vertex color.
            SkASSERT(drawState->hasColorVertexAttribute());
        }
        int nGlyphs = fCurrVertex / kVerticesPerGlyph;
        fDrawTarget->setIndexSourceToBuffer(fContext->getQuadIndexBuffer());
        fDrawTarget->drawIndexedInstances(kTriangles_GrPrimitiveType,
                                          nGlyphs,
                                          kVerticesPerGlyph, kIndicesPerGlyph, &fVertexBounds);
        fDrawTarget->resetVertexSource();
        fVertices = NULL;
        fTotalVertexCount -= fCurrVertex;
        fCurrVertex = 0;
        SkSafeSetNull(fCurrTexture);
        fVertexBounds.setLargestInverted();
    }
}

inline void GrDistanceFieldTextContext::finish() {
    this->flush();
    fTotalVertexCount = 0;

    GrTextContext::finish();
}

