/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkGr.h"
#include "SkColorFilter.h"
#include "SkConfig8888.h"
#include "SkData.h"
#include "SkMessageBus.h"
#include "SkPixelRef.h"
#include "SkTextureCompressor.h"
#include "GrResourceCache.h"
#include "GrGpu.h"
#include "effects/GrDitherEffect.h"
#include "GrDrawTargetCaps.h"
#include "effects/GrYUVtoRGBEffect.h"

#ifndef SK_IGNORE_ETC1_SUPPORT
#  include "ktx.h"
#  include "etc1.h"
#endif

/*  Fill out buffer with the compressed format Ganesh expects from a colortable
 based bitmap. [palette (colortable) + indices].

 At the moment Ganesh only supports 8bit version. If Ganesh allowed we others
 we could detect that the colortable.count is <= 16, and then repack the
 indices as nibbles to save RAM, but it would take more time (i.e. a lot
 slower than memcpy), so skipping that for now.

 Ganesh wants a full 256 palette entry, even though Skia's ctable is only as big
 as the colortable.count says it is.
 */
static void build_index8_data(void* buffer, const SkBitmap& bitmap) {
    SkASSERT(kIndex_8_SkColorType == bitmap.colorType());

    SkAutoLockPixels alp(bitmap);
    if (!bitmap.readyToDraw()) {
        SkDEBUGFAIL("bitmap not ready to draw!");
        return;
    }

    SkColorTable* ctable = bitmap.getColorTable();
    char* dst = (char*)buffer;

    const int count = ctable->count();

    SkDstPixelInfo dstPI;
    dstPI.fColorType = kRGBA_8888_SkColorType;
    dstPI.fAlphaType = kPremul_SkAlphaType;
    dstPI.fPixels = buffer;
    dstPI.fRowBytes = count * sizeof(SkPMColor);

    SkSrcPixelInfo srcPI;
    srcPI.fColorType = kN32_SkColorType;
    srcPI.fAlphaType = kPremul_SkAlphaType;
    srcPI.fPixels = ctable->lockColors();
    srcPI.fRowBytes = count * sizeof(SkPMColor);

    srcPI.convertPixelsTo(&dstPI, count, 1);

    ctable->unlockColors();

    // always skip a full 256 number of entries, even if we memcpy'd fewer
    dst += 256 * sizeof(GrColor);

    if ((unsigned)bitmap.width() == bitmap.rowBytes()) {
        memcpy(dst, bitmap.getPixels(), bitmap.getSize());
    } else {
        // need to trim off the extra bytes per row
        size_t width = bitmap.width();
        size_t rowBytes = bitmap.rowBytes();
        const char* src = (const char*)bitmap.getPixels();
        for (int y = 0; y < bitmap.height(); y++) {
            memcpy(dst, src, width);
            src += rowBytes;
            dst += width;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static void generate_bitmap_cache_id(const SkBitmap& bitmap, GrCacheID* id) {
    // Our id includes the offset, width, and height so that bitmaps created by extractSubset()
    // are unique.
    uint32_t genID = bitmap.getGenerationID();
    SkIPoint origin = bitmap.pixelRefOrigin();
    int16_t width = SkToS16(bitmap.width());
    int16_t height = SkToS16(bitmap.height());

    GrCacheID::Key key;
    memcpy(key.fData8 +  0, &genID,     4);
    memcpy(key.fData8 +  4, &origin.fX, 4);
    memcpy(key.fData8 +  8, &origin.fY, 4);
    memcpy(key.fData8 + 12, &width,     2);
    memcpy(key.fData8 + 14, &height,    2);
    static const size_t kKeyDataSize = 16;
    memset(key.fData8 + kKeyDataSize, 0, sizeof(key) - kKeyDataSize);
    GR_STATIC_ASSERT(sizeof(key) >= kKeyDataSize);
    static const GrCacheID::Domain gBitmapTextureDomain = GrCacheID::GenerateDomain();
    id->reset(gBitmapTextureDomain, key);
}

static void generate_bitmap_texture_desc(const SkBitmap& bitmap, GrSurfaceDesc* desc) {
    desc->fFlags = kNone_GrSurfaceFlags;
    desc->fWidth = bitmap.width();
    desc->fHeight = bitmap.height();
    desc->fConfig = SkImageInfo2GrPixelConfig(bitmap.info());
    desc->fSampleCnt = 0;
}

namespace {

// When the SkPixelRef genID changes, invalidate a corresponding GrResource described by key.
class GrResourceInvalidator : public SkPixelRef::GenIDChangeListener {
public:
    explicit GrResourceInvalidator(GrResourceKey key) : fKey(key) {}
private:
    GrResourceKey fKey;

    virtual void onChange() SK_OVERRIDE {
        const GrResourceInvalidatedMessage message = { fKey };
        SkMessageBus<GrResourceInvalidatedMessage>::Post(message);
    }
};

}  // namespace

static void add_genID_listener(GrResourceKey key, SkPixelRef* pixelRef) {
    SkASSERT(pixelRef);
    pixelRef->addGenIDChangeListener(SkNEW_ARGS(GrResourceInvalidator, (key)));
}

static GrTexture* sk_gr_allocate_texture(GrContext* ctx,
                                         bool cache,
                                         const GrTextureParams* params,
                                         const SkBitmap& bm,
                                         GrSurfaceDesc desc,
                                         const void* pixels,
                                         size_t rowBytes) {
    GrTexture* result;
    if (cache) {
        // This texture is likely to be used again so leave it in the cache
        GrCacheID cacheID;
        generate_bitmap_cache_id(bm, &cacheID);

        GrResourceKey key;
        result = ctx->createTexture(params, desc, cacheID, pixels, rowBytes, &key);
        if (result) {
            add_genID_listener(key, bm.pixelRef());
        }
   } else {
        // This texture is unlikely to be used again (in its present form) so
        // just use a scratch texture. This will remove the texture from the
        // cache so no one else can find it. Additionally, once unlocked, the
        // scratch texture will go to the end of the list for purging so will
        // likely be available for this volatile bitmap the next time around.
        result = ctx->refScratchTexture(desc, GrContext::kExact_ScratchTexMatch);
        if (pixels) {
            result->writePixels(0, 0, bm.width(), bm.height(), desc.fConfig, pixels, rowBytes);
        }
    }
    return result;
}

#ifndef SK_IGNORE_ETC1_SUPPORT
static GrTexture *load_etc1_texture(GrContext* ctx, bool cache,
                                    const GrTextureParams* params,
                                    const SkBitmap &bm, GrSurfaceDesc desc) {
    SkAutoTUnref<SkData> data(bm.pixelRef()->refEncodedData());

    // Is this even encoded data?
    if (NULL == data) {
        return NULL;
    }

    // Is this a valid PKM encoded data?
    const uint8_t *bytes = data->bytes();
    if (etc1_pkm_is_valid(bytes)) {
        uint32_t encodedWidth = etc1_pkm_get_width(bytes);
        uint32_t encodedHeight = etc1_pkm_get_height(bytes);

        // Does the data match the dimensions of the bitmap? If not,
        // then we don't know how to scale the image to match it...
        if (encodedWidth != static_cast<uint32_t>(bm.width()) ||
            encodedHeight != static_cast<uint32_t>(bm.height())) {
            return NULL;
        }

        // Everything seems good... skip ahead to the data.
        bytes += ETC_PKM_HEADER_SIZE;
        desc.fConfig = kETC1_GrPixelConfig;
    } else if (SkKTXFile::is_ktx(bytes)) {
        SkKTXFile ktx(data);

        // Is it actually an ETC1 texture?
        if (!ktx.isCompressedFormat(SkTextureCompressor::kETC1_Format)) {
            return NULL;
        }

        // Does the data match the dimensions of the bitmap? If not,
        // then we don't know how to scale the image to match it...
        if (ktx.width() != bm.width() || ktx.height() != bm.height()) {
            return NULL;
        }        

        bytes = ktx.pixelData();
        desc.fConfig = kETC1_GrPixelConfig;
    } else {
        return NULL;
    }

    return sk_gr_allocate_texture(ctx, cache, params, bm, desc, bytes, 0);
}
#endif   // SK_IGNORE_ETC1_SUPPORT

static GrTexture *load_yuv_texture(GrContext* ctx, bool cache, const GrTextureParams* params,
                                   const SkBitmap& bm, const GrSurfaceDesc& desc) {
    // Subsets are not supported, the whole pixelRef is loaded when using YUV decoding
    if ((bm.pixelRef()->info().width()  != bm.info().width()) ||
        (bm.pixelRef()->info().height() != bm.info().height())) {
        return NULL;
    }

    SkPixelRef* pixelRef = bm.pixelRef();
    SkISize yuvSizes[3];
    if ((NULL == pixelRef) || !pixelRef->getYUV8Planes(yuvSizes, NULL, NULL, NULL)) {
        return NULL;
    }

    // Allocate the memory for YUV
    size_t totalSize(0);
    size_t sizes[3], rowBytes[3];
    for (int i = 0; i < 3; ++i) {
        rowBytes[i] = yuvSizes[i].fWidth;
        totalSize  += sizes[i] = rowBytes[i] * yuvSizes[i].fHeight;
    }
    SkAutoMalloc storage(totalSize);
    void* planes[3];
    planes[0] = storage.get();
    planes[1] = (uint8_t*)planes[0] + sizes[0];
    planes[2] = (uint8_t*)planes[1] + sizes[1];

    SkYUVColorSpace colorSpace;

    // Get the YUV planes
    if (!pixelRef->getYUV8Planes(yuvSizes, planes, rowBytes, &colorSpace)) {
        return NULL;
    }

    GrSurfaceDesc yuvDesc;
    yuvDesc.fConfig = kAlpha_8_GrPixelConfig;
    SkAutoTUnref<GrTexture> yuvTextures[3];
    for (int i = 0; i < 3; ++i) {
        yuvDesc.fWidth  = yuvSizes[i].fWidth;
        yuvDesc.fHeight = yuvSizes[i].fHeight;
        yuvTextures[i].reset(
            ctx->refScratchTexture(yuvDesc, GrContext::kApprox_ScratchTexMatch));
        if (!yuvTextures[i] ||
            !yuvTextures[i]->writePixels(0, 0, yuvDesc.fWidth, yuvDesc.fHeight,
                                         yuvDesc.fConfig, planes[i], rowBytes[i])) {
            return NULL;
        }
    }

    GrSurfaceDesc rtDesc = desc;
    rtDesc.fFlags = rtDesc.fFlags |
                    kRenderTarget_GrSurfaceFlag |
                    kNoStencil_GrSurfaceFlag;

    GrTexture* result = sk_gr_allocate_texture(ctx, cache, params, bm, rtDesc, NULL, 0);

    GrRenderTarget* renderTarget = result ? result->asRenderTarget() : NULL;
    if (renderTarget) {
        SkAutoTUnref<GrFragmentProcessor> yuvToRgbProcessor(
            GrYUVtoRGBEffect::Create(yuvTextures[0], yuvTextures[1], yuvTextures[2], colorSpace));
        GrPaint paint;
        paint.addColorProcessor(yuvToRgbProcessor);
        SkRect r = SkRect::MakeWH(SkIntToScalar(yuvSizes[0].fWidth),
                                  SkIntToScalar(yuvSizes[0].fHeight));
        GrContext::AutoRenderTarget autoRT(ctx, renderTarget);
        GrContext::AutoMatrix am;
        am.setIdentity(ctx);
        GrContext::AutoClip ac(ctx, GrContext::AutoClip::kWideOpen_InitialClip);
        ctx->drawRect(paint, r);
    } else {
        SkSafeSetNull(result);
    }

    return result;
}

static GrTexture* sk_gr_create_bitmap_texture(GrContext* ctx,
                                              bool cache,
                                              const GrTextureParams* params,
                                              const SkBitmap& origBitmap) {
    SkBitmap tmpBitmap;

    const SkBitmap* bitmap = &origBitmap;

    GrSurfaceDesc desc;
    generate_bitmap_texture_desc(*bitmap, &desc);

    if (kIndex_8_SkColorType == bitmap->colorType()) {
        // build_compressed_data doesn't do npot->pot expansion
        // and paletted textures can't be sub-updated
        if (cache && ctx->supportsIndex8PixelConfig(params, bitmap->width(), bitmap->height())) {
            size_t imageSize = GrCompressedFormatDataSize(kIndex_8_GrPixelConfig,
                                                          bitmap->width(), bitmap->height());
            SkAutoMalloc storage(imageSize);
            build_index8_data(storage.get(), origBitmap);

            // our compressed data will be trimmed, so pass width() for its
            // "rowBytes", since they are the same now.
            return sk_gr_allocate_texture(ctx, cache, params, origBitmap,
                                          desc, storage.get(), bitmap->width());
        } else {
            origBitmap.copyTo(&tmpBitmap, kN32_SkColorType);
            // now bitmap points to our temp, which has been promoted to 32bits
            bitmap = &tmpBitmap;
            desc.fConfig = SkImageInfo2GrPixelConfig(bitmap->info());
        }
    }

    // Is this an ETC1 encoded texture?
#ifndef SK_IGNORE_ETC1_SUPPORT
    else if (
        // We do not support scratch ETC1 textures, hence they should all be at least
        // trying to go to the cache.
        cache
        // Make sure that the underlying device supports ETC1 textures before we go ahead
        // and check the data.
        && ctx->getGpu()->caps()->isConfigTexturable(kETC1_GrPixelConfig)
        // If the bitmap had compressed data and was then uncompressed, it'll still return
        // compressed data on 'refEncodedData' and upload it. Probably not good, since if
        // the bitmap has available pixels, then they might not be what the decompressed
        // data is.
        && !(bitmap->readyToDraw())) {
        GrTexture *texture = load_etc1_texture(ctx, cache, params, *bitmap, desc);
        if (texture) {
            return texture;
        }
    }
#endif   // SK_IGNORE_ETC1_SUPPORT

    else {
        GrTexture *texture = load_yuv_texture(ctx, cache, params, *bitmap, desc);
        if (texture) {
            return texture;
        }
    }
    SkAutoLockPixels alp(*bitmap);
    if (!bitmap->readyToDraw()) {
        return NULL;
    }

    return sk_gr_allocate_texture(ctx, cache, params, origBitmap, desc,
                                  bitmap->getPixels(), bitmap->rowBytes());
}

bool GrIsBitmapInCache(const GrContext* ctx,
                       const SkBitmap& bitmap,
                       const GrTextureParams* params) {
    GrCacheID cacheID;
    generate_bitmap_cache_id(bitmap, &cacheID);

    GrSurfaceDesc desc;
    generate_bitmap_texture_desc(bitmap, &desc);
    return ctx->isTextureInCache(desc, cacheID, params);
}

GrTexture* GrRefCachedBitmapTexture(GrContext* ctx,
                                    const SkBitmap& bitmap,
                                    const GrTextureParams* params) {
    GrTexture* result = NULL;

    bool cache = !bitmap.isVolatile();

    if (cache) {
        // If the bitmap isn't changing try to find a cached copy first.

        GrCacheID cacheID;
        generate_bitmap_cache_id(bitmap, &cacheID);

        GrSurfaceDesc desc;
        generate_bitmap_texture_desc(bitmap, &desc);

        result = ctx->findAndRefTexture(desc, cacheID, params);
    }
    if (NULL == result) {
        result = sk_gr_create_bitmap_texture(ctx, cache, params, bitmap);
    }
    if (NULL == result) {
        SkDebugf("---- failed to create texture for cache [%d %d]\n",
                 bitmap.width(), bitmap.height());
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

// alphatype is ignore for now, but if GrPixelConfig is expanded to encompass
// alpha info, that will be considered.
GrPixelConfig SkImageInfo2GrPixelConfig(SkColorType ct, SkAlphaType) {
    switch (ct) {
        case kUnknown_SkColorType:
            return kUnknown_GrPixelConfig;
        case kAlpha_8_SkColorType:
            return kAlpha_8_GrPixelConfig;
        case kRGB_565_SkColorType:
            return kRGB_565_GrPixelConfig;
        case kARGB_4444_SkColorType:
            return kRGBA_4444_GrPixelConfig;
        case kRGBA_8888_SkColorType:
            return kRGBA_8888_GrPixelConfig;
        case kBGRA_8888_SkColorType:
            return kBGRA_8888_GrPixelConfig;
        case kIndex_8_SkColorType:
            return kIndex_8_GrPixelConfig;
    }
    SkASSERT(0);    // shouldn't get here
    return kUnknown_GrPixelConfig;
}

bool GrPixelConfig2ColorType(GrPixelConfig config, SkColorType* ctOut) {
    SkColorType ct;
    switch (config) {
        case kAlpha_8_GrPixelConfig:
            ct = kAlpha_8_SkColorType;
            break;
        case kIndex_8_GrPixelConfig:
            ct = kIndex_8_SkColorType;
            break;
        case kRGB_565_GrPixelConfig:
            ct = kRGB_565_SkColorType;
            break;
        case kRGBA_4444_GrPixelConfig:
            ct = kARGB_4444_SkColorType;
            break;
        case kRGBA_8888_GrPixelConfig:
            ct = kRGBA_8888_SkColorType;
            break;
        case kBGRA_8888_GrPixelConfig:
            ct = kBGRA_8888_SkColorType;
            break;
        default:
            return false;
    }
    if (ctOut) {
        *ctOut = ct;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

void SkPaint2GrPaintNoShader(GrContext* context, const SkPaint& skPaint, GrColor paintColor,
                             bool constantColor, GrPaint* grPaint) {

    grPaint->setDither(skPaint.isDither());
    grPaint->setAntiAlias(skPaint.isAntiAlias());

    SkXfermode::Coeff sm;
    SkXfermode::Coeff dm;

    SkXfermode* mode = skPaint.getXfermode();
    GrFragmentProcessor* xferProcessor = NULL;
    if (SkXfermode::asFragmentProcessorOrCoeff(mode, &xferProcessor, &sm, &dm)) {
        if (xferProcessor) {
            grPaint->addColorProcessor(xferProcessor)->unref();
            sm = SkXfermode::kOne_Coeff;
            dm = SkXfermode::kZero_Coeff;
        }
    } else {
        //SkDEBUGCODE(SkDebugf("Unsupported xfer mode.\n");)
        // Fall back to src-over
        sm = SkXfermode::kOne_Coeff;
        dm = SkXfermode::kISA_Coeff;
    }
    grPaint->setBlendFunc(sk_blend_to_grblend(sm), sk_blend_to_grblend(dm));
    
    //set the color of the paint to the one of the parameter
    grPaint->setColor(paintColor);

    SkColorFilter* colorFilter = skPaint.getColorFilter();
    if (colorFilter) {
        // if the source color is a constant then apply the filter here once rather than per pixel
        // in a shader.
        if (constantColor) {
            SkColor filtered = colorFilter->filterColor(skPaint.getColor());
            grPaint->setColor(SkColor2GrColor(filtered));
        } else {
            SkAutoTUnref<GrFragmentProcessor> fp(colorFilter->asFragmentProcessor(context));
            if (fp.get()) {
                grPaint->addColorProcessor(fp);
            }
        }
    }

#ifndef SK_IGNORE_GPU_DITHER
    // If the dither flag is set, then we need to see if the underlying context
    // supports it. If not, then install a dither effect.
    if (skPaint.isDither() && grPaint->numColorStages() > 0) {
        // What are we rendering into?
        const GrRenderTarget *target = context->getRenderTarget();
        SkASSERT(target);

        // Suspect the dithering flag has no effect on these configs, otherwise
        // fall back on setting the appropriate state.
        if (target->config() == kRGBA_8888_GrPixelConfig ||
            target->config() == kBGRA_8888_GrPixelConfig) {
            // The dither flag is set and the target is likely
            // not going to be dithered by the GPU.
            SkAutoTUnref<GrFragmentProcessor> fp(GrDitherEffect::Create());
            if (fp.get()) {
                grPaint->addColorProcessor(fp);
                grPaint->setDither(false);
            }
        }
    }
#endif
}

/**
 * Unlike GrContext::AutoMatrix, this doesn't require setting a new matrix. GrContext::AutoMatrix
 * likes to set the new matrix in its constructor because it is usually necessary to simulataneously
 * update a GrPaint. This AutoMatrix is used while initially setting up GrPaint, however.
 */
class AutoMatrix {
public:
    AutoMatrix(GrContext* context) {
        fMatrix = context->getMatrix();
        fContext = context;
    }
    ~AutoMatrix() {
        SkASSERT(fContext);
        fContext->setMatrix(fMatrix);
    }
private:
    GrContext* fContext;
    SkMatrix fMatrix;
};

void SkPaint2GrPaintShader(GrContext* context, const SkPaint& skPaint,
                           bool constantColor, GrPaint* grPaint) {
    SkShader* shader = skPaint.getShader();
    if (NULL == shader) {
        SkPaint2GrPaintNoShader(context, skPaint, SkColor2GrColor(skPaint.getColor()),
                                constantColor, grPaint);
        return;
    }

    GrColor paintColor = SkColor2GrColor(skPaint.getColor());

    // Start a new block here in order to preserve our context state after calling
    // asFragmentProcessor(). Since these calls get passed back to the client, we don't really
    // want them messing around with the context.
    {
        // SkShader::asFragmentProcessor() may do offscreen rendering. Save off the current RT,
        // clip, and matrix. We don't reset the matrix on the context because
        // SkShader::asFragmentProcessor may use GrContext::getMatrix() to know the transformation
        // from local coords to device space.
        GrContext::AutoRenderTarget art(context, NULL);
        GrContext::AutoClip ac(context, GrContext::AutoClip::kWideOpen_InitialClip);
        AutoMatrix am(context);

        // Allow the shader to modify paintColor and also create an effect to be installed as
        // the first color effect on the GrPaint.
        GrFragmentProcessor* fp = NULL;
        if (shader->asFragmentProcessor(context, skPaint, NULL, &paintColor, &fp) && fp) {
            grPaint->addColorProcessor(fp)->unref();
            constantColor = false;
        }
    }

    // The grcolor is automatically set when calling asFragmentProcessor.
    // If the shader can be seen as an effect it returns true and adds its effect to the grpaint.
    SkPaint2GrPaintNoShader(context, skPaint, paintColor, constantColor, grPaint);
}
