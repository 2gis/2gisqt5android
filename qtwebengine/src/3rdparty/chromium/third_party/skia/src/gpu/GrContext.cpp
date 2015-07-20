
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrContext.h"

#include "effects/GrConfigConversionEffect.h"
#include "effects/GrDashingEffect.h"
#include "effects/GrSingleTextureEffect.h"

#include "GrAARectRenderer.h"
#include "GrBufferAllocPool.h"
#include "GrGpu.h"
#include "GrDistanceFieldTextContext.h"
#include "GrDrawTargetCaps.h"
#include "GrIndexBuffer.h"
#include "GrInOrderDrawBuffer.h"
#include "GrLayerCache.h"
#include "GrOvalRenderer.h"
#include "GrPathRenderer.h"
#include "GrPathUtils.h"
#include "GrResourceCache.h"
#include "GrResourceCache2.h"
#include "GrSoftwarePathRenderer.h"
#include "GrStencilBuffer.h"
#include "GrStencilAndCoverTextContext.h"
#include "GrStrokeInfo.h"
#include "GrSurfacePriv.h"
#include "GrTextStrike.h"
#include "GrTexturePriv.h"
#include "GrTraceMarker.h"
#include "GrTracing.h"
#include "SkDashPathPriv.h"
#include "SkConfig8888.h"
#include "SkGr.h"
#include "SkRRect.h"
#include "SkStrokeRec.h"
#include "SkTLazy.h"
#include "SkTLS.h"
#include "SkTraceEvent.h"

#ifdef SK_DEBUG
    // change this to a 1 to see notifications when partial coverage fails
    #define GR_DEBUG_PARTIAL_COVERAGE_CHECK 0
#else
    #define GR_DEBUG_PARTIAL_COVERAGE_CHECK 0
#endif

static const size_t MAX_RESOURCE_CACHE_COUNT = GR_DEFAULT_RESOURCE_CACHE_COUNT_LIMIT;
static const size_t MAX_RESOURCE_CACHE_BYTES = GR_DEFAULT_RESOURCE_CACHE_MB_LIMIT * 1024 * 1024;

static const size_t DRAW_BUFFER_VBPOOL_BUFFER_SIZE = 1 << 15;
static const int DRAW_BUFFER_VBPOOL_PREALLOC_BUFFERS = 4;

static const size_t DRAW_BUFFER_IBPOOL_BUFFER_SIZE = 1 << 11;
static const int DRAW_BUFFER_IBPOOL_PREALLOC_BUFFERS = 4;

#define ASSERT_OWNED_RESOURCE(R) SkASSERT(!(R) || (R)->getContext() == this)

// Glorified typedef to avoid including GrDrawState.h in GrContext.h
class GrContext::AutoRestoreEffects : public GrDrawState::AutoRestoreEffects {};

class GrContext::AutoCheckFlush {
public:
    AutoCheckFlush(GrContext* context) : fContext(context) { SkASSERT(context); }

    ~AutoCheckFlush() {
        if (fContext->fFlushToReduceCacheSize) {
            fContext->flush();
        }
    }

private:
    GrContext* fContext;
};

GrContext* GrContext::Create(GrBackend backend, GrBackendContext backendContext,
                             const Options* opts) {
    GrContext* context;
    if (NULL == opts) {
        context = SkNEW_ARGS(GrContext, (Options()));
    } else {
        context = SkNEW_ARGS(GrContext, (*opts));
    }

    if (context->init(backend, backendContext)) {
        return context;
    } else {
        context->unref();
        return NULL;
    }
}

GrContext::GrContext(const Options& opts) : fOptions(opts) {
    fDrawState = NULL;
    fGpu = NULL;
    fClip = NULL;
    fPathRendererChain = NULL;
    fSoftwarePathRenderer = NULL;
    fResourceCache = NULL;
    fResourceCache2 = NULL;
    fFontCache = NULL;
    fDrawBuffer = NULL;
    fDrawBufferVBAllocPool = NULL;
    fDrawBufferIBAllocPool = NULL;
    fFlushToReduceCacheSize = false;
    fAARectRenderer = NULL;
    fOvalRenderer = NULL;
    fViewMatrix.reset();
    fMaxTextureSizeOverride = 1 << 20;
}

bool GrContext::init(GrBackend backend, GrBackendContext backendContext) {
    SkASSERT(NULL == fGpu);

    fGpu = GrGpu::Create(backend, backendContext, this);
    if (NULL == fGpu) {
        return false;
    }
    this->initCommon();
    return true;
}

void GrContext::initCommon() {
    fDrawState = SkNEW(GrDrawState);
    fGpu->setDrawState(fDrawState);

    fResourceCache = SkNEW_ARGS(GrResourceCache, (fGpu->caps(),
                                                  MAX_RESOURCE_CACHE_COUNT,
                                                  MAX_RESOURCE_CACHE_BYTES));
    fResourceCache->setOverbudgetCallback(OverbudgetCB, this);
    fResourceCache2 = SkNEW(GrResourceCache2);

    fFontCache = SkNEW_ARGS(GrFontCache, (fGpu));

    fLayerCache.reset(SkNEW_ARGS(GrLayerCache, (this)));

    fAARectRenderer = SkNEW_ARGS(GrAARectRenderer, (fGpu));
    fOvalRenderer = SkNEW(GrOvalRenderer);

    fDidTestPMConversions = false;

    this->setupDrawBuffer();
}

GrContext::~GrContext() {
    if (NULL == fGpu) {
        return;
    }

    this->flush();

    for (int i = 0; i < fCleanUpData.count(); ++i) {
        (*fCleanUpData[i].fFunc)(this, fCleanUpData[i].fInfo);
    }

    SkDELETE(fResourceCache2);
    fResourceCache2 = NULL;
    SkDELETE(fResourceCache);
    fResourceCache = NULL;
    SkDELETE(fFontCache);
    SkDELETE(fDrawBuffer);
    SkDELETE(fDrawBufferVBAllocPool);
    SkDELETE(fDrawBufferIBAllocPool);

    fAARectRenderer->unref();
    fOvalRenderer->unref();

    fGpu->unref();
    SkSafeUnref(fPathRendererChain);
    SkSafeUnref(fSoftwarePathRenderer);
    fDrawState->unref();
}

void GrContext::abandonContext() {
    // abandon first to so destructors
    // don't try to free the resources in the API.
    fResourceCache2->abandonAll();

    fGpu->contextAbandoned();

    // a path renderer may be holding onto resources that
    // are now unusable
    SkSafeSetNull(fPathRendererChain);
    SkSafeSetNull(fSoftwarePathRenderer);

    delete fDrawBuffer;
    fDrawBuffer = NULL;

    delete fDrawBufferVBAllocPool;
    fDrawBufferVBAllocPool = NULL;

    delete fDrawBufferIBAllocPool;
    fDrawBufferIBAllocPool = NULL;

    fAARectRenderer->reset();
    fOvalRenderer->reset();

    fResourceCache->purgeAllUnlocked();

    fFontCache->freeAll();
    fLayerCache->freeAll();
}

void GrContext::resetContext(uint32_t state) {
    fGpu->markContextDirty(state);
}

void GrContext::freeGpuResources() {
    this->flush();

    fGpu->purgeResources();
    if (fDrawBuffer) {
        fDrawBuffer->purgeResources();
    }

    fAARectRenderer->reset();
    fOvalRenderer->reset();

    fResourceCache->purgeAllUnlocked();
    fFontCache->freeAll();
    fLayerCache->freeAll();
    // a path renderer may be holding onto resources
    SkSafeSetNull(fPathRendererChain);
    SkSafeSetNull(fSoftwarePathRenderer);
}

void GrContext::getResourceCacheUsage(int* resourceCount, size_t* resourceBytes) const {
  if (resourceCount) {
    *resourceCount = fResourceCache->getCachedResourceCount();
  }
  if (resourceBytes) {
    *resourceBytes = fResourceCache->getCachedResourceBytes();
  }
}

GrTextContext* GrContext::createTextContext(GrRenderTarget* renderTarget,
                                            const SkDeviceProperties&
                                            leakyProperties,
                                            bool enableDistanceFieldFonts) {
    if (fGpu->caps()->pathRenderingSupport() && renderTarget->getStencilBuffer() && 
                                                renderTarget->isMultisampled()) {
        return GrStencilAndCoverTextContext::Create(this, leakyProperties);
    } 

    return GrDistanceFieldTextContext::Create(this, leakyProperties, enableDistanceFieldFonts);
}

////////////////////////////////////////////////////////////////////////////////

GrTexture* GrContext::findAndRefTexture(const GrSurfaceDesc& desc,
                                        const GrCacheID& cacheID,
                                        const GrTextureParams* params) {
    GrResourceKey resourceKey = GrTexturePriv::ComputeKey(fGpu, params, desc, cacheID);
    GrGpuResource* resource = fResourceCache->find(resourceKey);
    if (resource) {
        resource->ref();
        return static_cast<GrSurface*>(resource)->asTexture();
    } else {
        return NULL;
    }
}

bool GrContext::isTextureInCache(const GrSurfaceDesc& desc,
                                 const GrCacheID& cacheID,
                                 const GrTextureParams* params) const {
    GrResourceKey resourceKey = GrTexturePriv::ComputeKey(fGpu, params, desc, cacheID);
    return fResourceCache->hasKey(resourceKey);
}

void GrContext::addStencilBuffer(GrStencilBuffer* sb) {
    ASSERT_OWNED_RESOURCE(sb);

    GrResourceKey resourceKey = GrStencilBuffer::ComputeKey(sb->width(),
                                                            sb->height(),
                                                            sb->numSamples());
    fResourceCache->addResource(resourceKey, sb);
}

GrStencilBuffer* GrContext::findStencilBuffer(int width, int height,
                                              int sampleCnt) {
    GrResourceKey resourceKey = GrStencilBuffer::ComputeKey(width,
                                                            height,
                                                            sampleCnt);
    GrGpuResource* resource = fResourceCache->find(resourceKey);
    return static_cast<GrStencilBuffer*>(resource);
}

static void stretch_image(void* dst,
                          int dstW,
                          int dstH,
                          const void* src,
                          int srcW,
                          int srcH,
                          size_t bpp) {
    SkFixed dx = (srcW << 16) / dstW;
    SkFixed dy = (srcH << 16) / dstH;

    SkFixed y = dy >> 1;

    size_t dstXLimit = dstW*bpp;
    for (int j = 0; j < dstH; ++j) {
        SkFixed x = dx >> 1;
        const uint8_t* srcRow = reinterpret_cast<const uint8_t *>(src) + (y>>16)*srcW*bpp;
        uint8_t* dstRow = reinterpret_cast<uint8_t *>(dst) + j*dstW*bpp;
        for (size_t i = 0; i < dstXLimit; i += bpp) {
            memcpy(dstRow + i, srcRow + (x>>16)*bpp, bpp);
            x += dx;
        }
        y += dy;
    }
}

namespace {

// position + local coordinate
extern const GrVertexAttrib gVertexAttribs[] = {
    {kVec2f_GrVertexAttribType, 0,               kPosition_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType, sizeof(SkPoint), kLocalCoord_GrVertexAttribBinding}
};

};

// The desired texture is NPOT and tiled but that isn't supported by
// the current hardware. Resize the texture to be a POT
GrTexture* GrContext::createResizedTexture(const GrSurfaceDesc& desc,
                                           const GrCacheID& cacheID,
                                           const void* srcData,
                                           size_t rowBytes,
                                           bool filter) {
    SkAutoTUnref<GrTexture> clampedTexture(this->findAndRefTexture(desc, cacheID, NULL));
    if (NULL == clampedTexture) {
        clampedTexture.reset(this->createTexture(NULL, desc, cacheID, srcData, rowBytes));

        if (NULL == clampedTexture) {
            return NULL;
        }
    }

    GrSurfaceDesc rtDesc = desc;
    rtDesc.fFlags =  rtDesc.fFlags |
                     kRenderTarget_GrSurfaceFlag |
                     kNoStencil_GrSurfaceFlag;
    rtDesc.fWidth  = GrNextPow2(desc.fWidth);
    rtDesc.fHeight = GrNextPow2(desc.fHeight);

    GrTexture* texture = fGpu->createTexture(rtDesc, NULL, 0);

    if (texture) {
        GrDrawTarget::AutoStateRestore asr(fDrawBuffer, GrDrawTarget::kReset_ASRInit);
        GrDrawState* drawState = fDrawBuffer->drawState();
        drawState->setRenderTarget(texture->asRenderTarget());

        // if filtering is not desired then we want to ensure all
        // texels in the resampled image are copies of texels from
        // the original.
        GrTextureParams params(SkShader::kClamp_TileMode,
                               filter ? GrTextureParams::kBilerp_FilterMode :
                                        GrTextureParams::kNone_FilterMode);
        drawState->addColorTextureProcessor(clampedTexture, SkMatrix::I(), params);

        drawState->setVertexAttribs<gVertexAttribs>(SK_ARRAY_COUNT(gVertexAttribs),
                                                    2 * sizeof(SkPoint));

        GrDrawTarget::AutoReleaseGeometry arg(fDrawBuffer, 4, 0);

        if (arg.succeeded()) {
            SkPoint* verts = (SkPoint*) arg.vertices();
            verts[0].setIRectFan(0, 0, texture->width(), texture->height(), 2 * sizeof(SkPoint));
            verts[1].setIRectFan(0, 0, 1, 1, 2 * sizeof(SkPoint));
            fDrawBuffer->drawNonIndexed(kTriangleFan_GrPrimitiveType, 0, 4);
        }
    } else {
        // TODO: Our CPU stretch doesn't filter. But we create separate
        // stretched textures when the texture params is either filtered or
        // not. Either implement filtered stretch blit on CPU or just create
        // one when FBO case fails.

        rtDesc.fFlags = kNone_GrSurfaceFlags;
        // no longer need to clamp at min RT size.
        rtDesc.fWidth  = GrNextPow2(desc.fWidth);
        rtDesc.fHeight = GrNextPow2(desc.fHeight);

        // We shouldn't be resizing a compressed texture.
        SkASSERT(!GrPixelConfigIsCompressed(desc.fConfig));

        size_t bpp = GrBytesPerPixel(desc.fConfig);
        GrAutoMalloc<128*128*4> stretchedPixels(bpp * rtDesc.fWidth * rtDesc.fHeight);
        stretch_image(stretchedPixels.get(), rtDesc.fWidth, rtDesc.fHeight,
                      srcData, desc.fWidth, desc.fHeight, bpp);

        size_t stretchedRowBytes = rtDesc.fWidth * bpp;

        texture = fGpu->createTexture(rtDesc, stretchedPixels.get(), stretchedRowBytes);
        SkASSERT(texture);
    }

    return texture;
}

GrTexture* GrContext::createTexture(const GrTextureParams* params,
                                    const GrSurfaceDesc& desc,
                                    const GrCacheID& cacheID,
                                    const void* srcData,
                                    size_t rowBytes,
                                    GrResourceKey* cacheKey) {
    GrResourceKey resourceKey = GrTexturePriv::ComputeKey(fGpu, params, desc, cacheID);

    GrTexture* texture;
    if (GrTexturePriv::NeedsResizing(resourceKey)) {
        // We do not know how to resize compressed textures.
        SkASSERT(!GrPixelConfigIsCompressed(desc.fConfig));

        texture = this->createResizedTexture(desc, cacheID,
                                             srcData, rowBytes,
                                             GrTexturePriv::NeedsBilerp(resourceKey));
    } else {
        texture = fGpu->createTexture(desc, srcData, rowBytes);
    }

    if (texture) {
        fResourceCache->addResource(resourceKey, texture);

        if (cacheKey) {
            *cacheKey = resourceKey;
        }
    }

    return texture;
}

GrTexture* GrContext::createNewScratchTexture(const GrSurfaceDesc& desc) {
    GrTexture* texture = fGpu->createTexture(desc, NULL, 0);
    if (!texture) {
        return NULL;
    }
    fResourceCache->addResource(texture->getScratchKey(), texture);
    return texture;
}

GrTexture* GrContext::refScratchTexture(const GrSurfaceDesc& inDesc, ScratchTexMatch match,
                                        bool calledDuringFlush) {
    // kNoStencil has no meaning if kRT isn't set.
    SkASSERT((inDesc.fFlags & kRenderTarget_GrSurfaceFlag) ||
             !(inDesc.fFlags & kNoStencil_GrSurfaceFlag));

    // Make sure caller has checked for renderability if kRT is set.
    SkASSERT(!(inDesc.fFlags & kRenderTarget_GrSurfaceFlag) ||
             this->isConfigRenderable(inDesc.fConfig, inDesc.fSampleCnt > 0));

    SkTCopyOnFirstWrite<GrSurfaceDesc> desc(inDesc);

    if (fGpu->caps()->reuseScratchTextures() || (desc->fFlags & kRenderTarget_GrSurfaceFlag)) {
        GrSurfaceFlags origFlags = desc->fFlags;
        if (kApprox_ScratchTexMatch == match) {
            // bin by pow2 with a reasonable min
            static const int MIN_SIZE = 16;
            GrSurfaceDesc* wdesc = desc.writable();
            wdesc->fWidth  = SkTMax(MIN_SIZE, GrNextPow2(desc->fWidth));
            wdesc->fHeight = SkTMax(MIN_SIZE, GrNextPow2(desc->fHeight));
        }

        do {
            GrResourceKey key = GrTexturePriv::ComputeScratchKey(*desc);
            uint32_t scratchFlags = 0;
            if (calledDuringFlush) {
                scratchFlags = GrResourceCache2::kRequireNoPendingIO_ScratchFlag;
            } else  if (!(desc->fFlags & kRenderTarget_GrSurfaceFlag)) {
                // If it is not a render target then it will most likely be populated by
                // writePixels() which will trigger a flush if the texture has pending IO.
                scratchFlags = GrResourceCache2::kPreferNoPendingIO_ScratchFlag;
            }
            GrGpuResource* resource = fResourceCache2->findAndRefScratchResource(key, scratchFlags);
            if (resource) {
                fResourceCache->makeResourceMRU(resource);
                return static_cast<GrSurface*>(resource)->asTexture();
            }

            if (kExact_ScratchTexMatch == match) {
                break;
            }
            // We had a cache miss and we are in approx mode, relax the fit of the flags.

            // We no longer try to reuse textures that were previously used as render targets in
            // situations where no RT is needed; doing otherwise can confuse the video driver and
            // cause significant performance problems in some cases.
            if (desc->fFlags & kNoStencil_GrSurfaceFlag) {
                desc.writable()->fFlags = desc->fFlags & ~kNoStencil_GrSurfaceFlag;
            } else {
                break;
            }

        } while (true);

        desc.writable()->fFlags = origFlags;
    }

    GrTexture* texture = this->createNewScratchTexture(*desc);
    SkASSERT(NULL == texture || 
             texture->getScratchKey() == GrTexturePriv::ComputeScratchKey(*desc));
    return texture;
}

bool GrContext::OverbudgetCB(void* data) {
    SkASSERT(data);

    GrContext* context = reinterpret_cast<GrContext*>(data);

    // Flush the InOrderDrawBuffer to possibly free up some textures
    context->fFlushToReduceCacheSize = true;

    return true;
}


GrTexture* GrContext::createUncachedTexture(const GrSurfaceDesc& descIn,
                                            void* srcData,
                                            size_t rowBytes) {
    GrSurfaceDesc descCopy = descIn;
    return fGpu->createTexture(descCopy, srcData, rowBytes);
}

void GrContext::getResourceCacheLimits(int* maxTextures, size_t* maxTextureBytes) const {
    fResourceCache->getLimits(maxTextures, maxTextureBytes);
}

void GrContext::setResourceCacheLimits(int maxTextures, size_t maxTextureBytes) {
    fResourceCache->setLimits(maxTextures, maxTextureBytes);
}

int GrContext::getMaxTextureSize() const {
    return SkTMin(fGpu->caps()->maxTextureSize(), fMaxTextureSizeOverride);
}

int GrContext::getMaxRenderTargetSize() const {
    return fGpu->caps()->maxRenderTargetSize();
}

int GrContext::getMaxSampleCount() const {
    return fGpu->caps()->maxSampleCount();
}

///////////////////////////////////////////////////////////////////////////////

GrTexture* GrContext::wrapBackendTexture(const GrBackendTextureDesc& desc) {
    return fGpu->wrapBackendTexture(desc);
}

GrRenderTarget* GrContext::wrapBackendRenderTarget(const GrBackendRenderTargetDesc& desc) {
    return fGpu->wrapBackendRenderTarget(desc);
}

///////////////////////////////////////////////////////////////////////////////

bool GrContext::supportsIndex8PixelConfig(const GrTextureParams* params,
                                          int width, int height) const {
    const GrDrawTargetCaps* caps = fGpu->caps();
    if (!caps->isConfigTexturable(kIndex_8_GrPixelConfig)) {
        return false;
    }

    bool isPow2 = SkIsPow2(width) && SkIsPow2(height);

    if (!isPow2) {
        bool tiled = params && params->isTiled();
        if (tiled && !caps->npotTextureTileSupport()) {
            return false;
        }
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////

void GrContext::clear(const SkIRect* rect,
                      const GrColor color,
                      bool canIgnoreRect,
                      GrRenderTarget* renderTarget) {
    ASSERT_OWNED_RESOURCE(renderTarget);
    SkASSERT(renderTarget);

    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GR_CREATE_TRACE_MARKER_CONTEXT("GrContext::clear", this);
    GrDrawTarget* target = this->prepareToDraw(NULL, &are, &acf);
    if (NULL == target) {
        return;
    }
    target->clear(rect, color, canIgnoreRect, renderTarget);
}

void GrContext::drawPaint(const GrPaint& origPaint) {
    // set rect to be big enough to fill the space, but not super-huge, so we
    // don't overflow fixed-point implementations
    SkRect r;
    r.setLTRB(0, 0,
              SkIntToScalar(getRenderTarget()->width()),
              SkIntToScalar(getRenderTarget()->height()));
    SkMatrix inverse;
    SkTCopyOnFirstWrite<GrPaint> paint(origPaint);
    AutoMatrix am;
    GR_CREATE_TRACE_MARKER_CONTEXT("GrContext::drawPaint", this);

    // We attempt to map r by the inverse matrix and draw that. mapRect will
    // map the four corners and bound them with a new rect. This will not
    // produce a correct result for some perspective matrices.
    if (!this->getMatrix().hasPerspective()) {
        if (!fViewMatrix.invert(&inverse)) {
            SkDebugf("Could not invert matrix\n");
            return;
        }
        inverse.mapRect(&r);
    } else {
        if (!am.setIdentity(this, paint.writable())) {
            SkDebugf("Could not invert matrix\n");
            return;
        }
    }
    // by definition this fills the entire clip, no need for AA
    if (paint->isAntiAlias()) {
        paint.writable()->setAntiAlias(false);
    }
    this->drawRect(*paint, r);
}

#ifdef SK_DEVELOPER
void GrContext::dumpFontCache() const {
    fFontCache->dump();
}
#endif

////////////////////////////////////////////////////////////////////////////////

/*  create a triangle strip that strokes the specified triangle. There are 8
 unique vertices, but we repreat the last 2 to close up. Alternatively we
 could use an indices array, and then only send 8 verts, but not sure that
 would be faster.
 */
static void setStrokeRectStrip(SkPoint verts[10], SkRect rect,
                               SkScalar width) {
    const SkScalar rad = SkScalarHalf(width);
    rect.sort();

    verts[0].set(rect.fLeft + rad, rect.fTop + rad);
    verts[1].set(rect.fLeft - rad, rect.fTop - rad);
    verts[2].set(rect.fRight - rad, rect.fTop + rad);
    verts[3].set(rect.fRight + rad, rect.fTop - rad);
    verts[4].set(rect.fRight - rad, rect.fBottom - rad);
    verts[5].set(rect.fRight + rad, rect.fBottom + rad);
    verts[6].set(rect.fLeft + rad, rect.fBottom - rad);
    verts[7].set(rect.fLeft - rad, rect.fBottom + rad);
    verts[8] = verts[0];
    verts[9] = verts[1];
}

static inline bool is_irect(const SkRect& r) {
  return SkScalarIsInt(r.fLeft)  && SkScalarIsInt(r.fTop) &&
         SkScalarIsInt(r.fRight) && SkScalarIsInt(r.fBottom);
}

static bool apply_aa_to_rect(GrDrawTarget* target,
                             const SkRect& rect,
                             SkScalar strokeWidth,
                             const SkMatrix& combinedMatrix,
                             SkRect* devBoundRect) {
    if (!target->getDrawState().canTweakAlphaForCoverage() &&
        target->shouldDisableCoverageAAForBlend()) {
#ifdef SK_DEBUG
        //SkDebugf("Turning off AA to correctly apply blend.\n");
#endif
        return false;
    }
    const GrDrawState& drawState = target->getDrawState();
    if (drawState.getRenderTarget()->isMultisampled()) {
        return false;
    }

#if defined(SHADER_AA_FILL_RECT) || !defined(IGNORE_ROT_AA_RECT_OPT)
    if (strokeWidth >= 0) {
#endif
        if (!combinedMatrix.preservesAxisAlignment()) {
            return false;
        }

#if defined(SHADER_AA_FILL_RECT) || !defined(IGNORE_ROT_AA_RECT_OPT)
    } else {
        if (!combinedMatrix.preservesRightAngles()) {
            return false;
        }
    }
#endif

    combinedMatrix.mapRect(devBoundRect, rect);
    if (strokeWidth < 0) {
        return !is_irect(*devBoundRect);
    }

    return true;
}

static inline bool rect_contains_inclusive(const SkRect& rect, const SkPoint& point) {
    return point.fX >= rect.fLeft && point.fX <= rect.fRight &&
           point.fY >= rect.fTop && point.fY <= rect.fBottom;
}

void GrContext::drawRect(const GrPaint& paint,
                         const SkRect& rect,
                         const GrStrokeInfo* strokeInfo) {
    if (strokeInfo && strokeInfo->isDashed()) {
        SkPath path;
        path.addRect(rect);
        this->drawPath(paint, path, *strokeInfo);
        return;
    }

    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }

    GR_CREATE_TRACE_MARKER("GrContext::drawRect", target);
    SkScalar width = NULL == strokeInfo ? -1 : strokeInfo->getStrokeRec().getWidth();
    SkMatrix matrix = target->drawState()->getViewMatrix();

    // Check if this is a full RT draw and can be replaced with a clear. We don't bother checking
    // cases where the RT is fully inside a stroke.
    if (width < 0) {
        SkRect rtRect;
        target->getDrawState().getRenderTarget()->getBoundsRect(&rtRect);
        SkRect clipSpaceRTRect = rtRect;
        bool checkClip = false;
        if (this->getClip()) {
            checkClip = true;
            clipSpaceRTRect.offset(SkIntToScalar(this->getClip()->fOrigin.fX),
                                   SkIntToScalar(this->getClip()->fOrigin.fY));
        }
        // Does the clip contain the entire RT?
        if (!checkClip || target->getClip()->fClipStack->quickContains(clipSpaceRTRect)) {
            SkMatrix invM;
            if (!matrix.invert(&invM)) {
                return;
            }
            // Does the rect bound the RT?
            SkPoint srcSpaceRTQuad[4];
            invM.mapRectToQuad(srcSpaceRTQuad, rtRect);
            if (rect_contains_inclusive(rect, srcSpaceRTQuad[0]) &&
                rect_contains_inclusive(rect, srcSpaceRTQuad[1]) &&
                rect_contains_inclusive(rect, srcSpaceRTQuad[2]) &&
                rect_contains_inclusive(rect, srcSpaceRTQuad[3])) {
                // Will it blend?
                GrColor clearColor;
                if (paint.isOpaqueAndConstantColor(&clearColor)) {
                    target->clear(NULL, clearColor, true, fRenderTarget);
                    return;
                }
            }
        }
    }

    SkRect devBoundRect;
    bool needAA = paint.isAntiAlias() &&
                  !target->getDrawState().getRenderTarget()->isMultisampled();
    bool doAA = needAA && apply_aa_to_rect(target, rect, width, matrix, &devBoundRect);

    const SkStrokeRec& strokeRec = strokeInfo->getStrokeRec();

    if (doAA) {
        GrDrawState::AutoViewMatrixRestore avmr;
        if (!avmr.setIdentity(target->drawState())) {
            return;
        }
        if (width >= 0) {
            fAARectRenderer->strokeAARect(target, rect,
                                          matrix, devBoundRect,
                                          strokeRec);
        } else {
            // filled AA rect
            fAARectRenderer->fillAARect(target,
                                        rect, matrix, devBoundRect);
        }
        return;
    }

    if (width >= 0) {
        // TODO: consider making static vertex buffers for these cases.
        // Hairline could be done by just adding closing vertex to
        // unitSquareVertexBuffer()

        static const int worstCaseVertCount = 10;
        target->drawState()->setDefaultVertexAttribs();
        GrDrawTarget::AutoReleaseGeometry geo(target, worstCaseVertCount, 0);

        if (!geo.succeeded()) {
            SkDebugf("Failed to get space for vertices!\n");
            return;
        }

        GrPrimitiveType primType;
        int vertCount;
        SkPoint* vertex = geo.positions();

        if (width > 0) {
            vertCount = 10;
            primType = kTriangleStrip_GrPrimitiveType;
            setStrokeRectStrip(vertex, rect, width);
        } else {
            // hairline
            vertCount = 5;
            primType = kLineStrip_GrPrimitiveType;
            vertex[0].set(rect.fLeft, rect.fTop);
            vertex[1].set(rect.fRight, rect.fTop);
            vertex[2].set(rect.fRight, rect.fBottom);
            vertex[3].set(rect.fLeft, rect.fBottom);
            vertex[4].set(rect.fLeft, rect.fTop);
        }

        target->drawNonIndexed(primType, 0, vertCount);
    } else {
        // filled BW rect
        target->drawSimpleRect(rect);
    }
}

void GrContext::drawRectToRect(const GrPaint& paint,
                               const SkRect& dstRect,
                               const SkRect& localRect,
                               const SkMatrix* localMatrix) {
    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }

    GR_CREATE_TRACE_MARKER("GrContext::drawRectToRect", target);

    target->drawRect(dstRect, &localRect, localMatrix);
}

namespace {

extern const GrVertexAttrib gPosUVColorAttribs[] = {
    {kVec2f_GrVertexAttribType,  0, kPosition_GrVertexAttribBinding },
    {kVec2f_GrVertexAttribType,  sizeof(SkPoint), kLocalCoord_GrVertexAttribBinding },
    {kVec4ub_GrVertexAttribType, 2*sizeof(SkPoint), kColor_GrVertexAttribBinding}
};

static const size_t kPosUVAttribsSize = 2 * sizeof(SkPoint);
static const size_t kPosUVColorAttribsSize = 2 * sizeof(SkPoint) + sizeof(GrColor);

extern const GrVertexAttrib gPosColorAttribs[] = {
    {kVec2f_GrVertexAttribType,  0, kPosition_GrVertexAttribBinding},
    {kVec4ub_GrVertexAttribType, sizeof(SkPoint), kColor_GrVertexAttribBinding},
};

static const size_t kPosAttribsSize = sizeof(SkPoint);
static const size_t kPosColorAttribsSize = sizeof(SkPoint) + sizeof(GrColor);

static void set_vertex_attributes(GrDrawState* drawState,
                                  const SkPoint* texCoords,
                                  const GrColor* colors,
                                  int* colorOffset,
                                  int* texOffset) {
    *texOffset = -1;
    *colorOffset = -1;

    if (texCoords && colors) {
        *texOffset = sizeof(SkPoint);
        *colorOffset = 2*sizeof(SkPoint);
        drawState->setVertexAttribs<gPosUVColorAttribs>(3, kPosUVColorAttribsSize);
    } else if (texCoords) {
        *texOffset = sizeof(SkPoint);
        drawState->setVertexAttribs<gPosUVColorAttribs>(2, kPosUVAttribsSize);
    } else if (colors) {
        *colorOffset = sizeof(SkPoint);
        drawState->setVertexAttribs<gPosColorAttribs>(2, kPosColorAttribsSize);
    } else {
        drawState->setVertexAttribs<gPosColorAttribs>(1, kPosAttribsSize);
    }
}

};

void GrContext::drawVertices(const GrPaint& paint,
                             GrPrimitiveType primitiveType,
                             int vertexCount,
                             const SkPoint positions[],
                             const SkPoint texCoords[],
                             const GrColor colors[],
                             const uint16_t indices[],
                             int indexCount) {
    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget::AutoReleaseGeometry geo; // must be inside AutoCheckFlush scope

    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }
    GrDrawState* drawState = target->drawState();

    GR_CREATE_TRACE_MARKER("GrContext::drawVertices", target);

    int colorOffset = -1, texOffset = -1;
    set_vertex_attributes(drawState, texCoords, colors, &colorOffset, &texOffset);

    size_t VertexStride = drawState->getVertexStride();
    if (!geo.set(target, vertexCount, indexCount)) {
        SkDebugf("Failed to get space for vertices!\n");
        return;
    }
    void* curVertex = geo.vertices();

    for (int i = 0; i < vertexCount; ++i) {
        *((SkPoint*)curVertex) = positions[i];

        if (texOffset >= 0) {
            *(SkPoint*)((intptr_t)curVertex + texOffset) = texCoords[i];
        }
        if (colorOffset >= 0) {
            *(GrColor*)((intptr_t)curVertex + colorOffset) = colors[i];
        }
        curVertex = (void*)((intptr_t)curVertex + VertexStride);
    }

    // we don't currently apply offscreen AA to this path. Need improved
    // management of GrDrawTarget's geometry to avoid copying points per-tile.
    if (indices) {
        uint16_t* curIndex = (uint16_t*)geo.indices();
        for (int i = 0; i < indexCount; ++i) {
            curIndex[i] = indices[i];
        }
        target->drawIndexed(primitiveType, 0, 0, vertexCount, indexCount);
    } else {
        target->drawNonIndexed(primitiveType, 0, vertexCount);
    }
}

///////////////////////////////////////////////////////////////////////////////

void GrContext::drawRRect(const GrPaint& paint,
                          const SkRRect& rrect,
                          const GrStrokeInfo& strokeInfo) {
    if (rrect.isEmpty()) {
       return;
    }

    if (strokeInfo.isDashed()) {
        SkPath path;
        path.addRRect(rrect);
        this->drawPath(paint, path, strokeInfo);
        return;
    }

    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }

    GR_CREATE_TRACE_MARKER("GrContext::drawRRect", target);

    const SkStrokeRec& strokeRec = strokeInfo.getStrokeRec();

    if (!fOvalRenderer->drawRRect(target, this, paint.isAntiAlias(), rrect, strokeRec)) {
        SkPath path;
        path.addRRect(rrect);
        this->internalDrawPath(target, paint.isAntiAlias(), path, strokeInfo);
    }
}

///////////////////////////////////////////////////////////////////////////////

void GrContext::drawDRRect(const GrPaint& paint,
                           const SkRRect& outer,
                           const SkRRect& inner) {
    if (outer.isEmpty()) {
       return;
    }

    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);

    GR_CREATE_TRACE_MARKER("GrContext::drawDRRect", target);

    if (!fOvalRenderer->drawDRRect(target, this, paint.isAntiAlias(), outer, inner)) {
        SkPath path;
        path.addRRect(inner);
        path.addRRect(outer);
        path.setFillType(SkPath::kEvenOdd_FillType);

        GrStrokeInfo fillRec(SkStrokeRec::kFill_InitStyle);
        this->internalDrawPath(target, paint.isAntiAlias(), path, fillRec);
    }
}

///////////////////////////////////////////////////////////////////////////////

void GrContext::drawOval(const GrPaint& paint,
                         const SkRect& oval,
                         const GrStrokeInfo& strokeInfo) {
    if (oval.isEmpty()) {
       return;
    }

    if (strokeInfo.isDashed()) {
        SkPath path;
        path.addOval(oval);
        this->drawPath(paint, path, strokeInfo);
        return;
    }

    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }

    GR_CREATE_TRACE_MARKER("GrContext::drawOval", target);

    const SkStrokeRec& strokeRec = strokeInfo.getStrokeRec();


    if (!fOvalRenderer->drawOval(target, this, paint.isAntiAlias(), oval, strokeRec)) {
        SkPath path;
        path.addOval(oval);
        this->internalDrawPath(target, paint.isAntiAlias(), path, strokeInfo);
    }
}

// Can 'path' be drawn as a pair of filled nested rectangles?
static bool is_nested_rects(GrDrawTarget* target,
                            const SkPath& path,
                            const SkStrokeRec& stroke,
                            SkRect rects[2]) {
    SkASSERT(stroke.isFillStyle());

    if (path.isInverseFillType()) {
        return false;
    }

    const GrDrawState& drawState = target->getDrawState();

    // TODO: this restriction could be lifted if we were willing to apply
    // the matrix to all the points individually rather than just to the rect
    if (!drawState.getViewMatrix().preservesAxisAlignment()) {
        return false;
    }

    if (!target->getDrawState().canTweakAlphaForCoverage() &&
        target->shouldDisableCoverageAAForBlend()) {
        return false;
    }

    SkPath::Direction dirs[2];
    if (!path.isNestedRects(rects, dirs)) {
        return false;
    }

    if (SkPath::kWinding_FillType == path.getFillType() && dirs[0] == dirs[1]) {
        // The two rects need to be wound opposite to each other
        return false;
    }

    // Right now, nested rects where the margin is not the same width
    // all around do not render correctly
    const SkScalar* outer = rects[0].asScalars();
    const SkScalar* inner = rects[1].asScalars();

    bool allEq = true;

    SkScalar margin = SkScalarAbs(outer[0] - inner[0]);
    bool allGoE1 = margin >= SK_Scalar1;

    for (int i = 1; i < 4; ++i) {
        SkScalar temp = SkScalarAbs(outer[i] - inner[i]);
        if (temp < SK_Scalar1) {
            allGoE1 = false;
        }
        if (!SkScalarNearlyEqual(margin, temp)) {
            allEq = false;
        }
    }

    return allEq || allGoE1;
}

void GrContext::drawPath(const GrPaint& paint, const SkPath& path, const GrStrokeInfo& strokeInfo) {

    if (path.isEmpty()) {
       if (path.isInverseFillType()) {
           this->drawPaint(paint);
       }
       return;
    }

    if (strokeInfo.isDashed()) {
        SkPoint pts[2];
        if (path.isLine(pts)) {
            AutoRestoreEffects are;
            AutoCheckFlush acf(this);
            GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
            if (NULL == target) {
                return;
            }
            GrDrawState* drawState = target->drawState();

            SkMatrix origViewMatrix = drawState->getViewMatrix();
            GrDrawState::AutoViewMatrixRestore avmr;
            if (avmr.setIdentity(target->drawState())) {
                if (GrDashingEffect::DrawDashLine(pts, paint, strokeInfo, fGpu, target,
                                                  origViewMatrix)) {
                    return;
                }
            }
        }

        // Filter dashed path into new path with the dashing applied
        const SkPathEffect::DashInfo& info = strokeInfo.getDashInfo();
        SkTLazy<SkPath> effectPath;
        GrStrokeInfo newStrokeInfo(strokeInfo, false);
        SkStrokeRec* stroke = newStrokeInfo.getStrokeRecPtr();
        if (SkDashPath::FilterDashPath(effectPath.init(), path, stroke, NULL, info)) {
            this->drawPath(paint, *effectPath.get(), newStrokeInfo);
            return;
        }

        this->drawPath(paint, path, newStrokeInfo);
        return;
    }

    // Note that internalDrawPath may sw-rasterize the path into a scratch texture.
    // Scratch textures can be recycled after they are returned to the texture
    // cache. This presents a potential hazard for buffered drawing. However,
    // the writePixels that uploads to the scratch will perform a flush so we're
    // OK.
    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(&paint, &are, &acf);
    if (NULL == target) {
        return;
    }
    GrDrawState* drawState = target->drawState();

    GR_CREATE_TRACE_MARKER1("GrContext::drawPath", target, "Is Convex", path.isConvex());

    const SkStrokeRec& strokeRec = strokeInfo.getStrokeRec();

    bool useCoverageAA = paint.isAntiAlias() && !drawState->getRenderTarget()->isMultisampled();

    if (useCoverageAA && strokeRec.getWidth() < 0 && !path.isConvex()) {
        // Concave AA paths are expensive - try to avoid them for special cases
        SkRect rects[2];

        if (is_nested_rects(target, path, strokeRec, rects)) {
            SkMatrix origViewMatrix = drawState->getViewMatrix();
            GrDrawState::AutoViewMatrixRestore avmr;
            if (!avmr.setIdentity(target->drawState())) {
                return;
            }

            fAARectRenderer->fillAANestedRects(target, rects, origViewMatrix);
            return;
        }
    }

    SkRect ovalRect;
    bool isOval = path.isOval(&ovalRect);

    if (!isOval || path.isInverseFillType()
        || !fOvalRenderer->drawOval(target, this, paint.isAntiAlias(), ovalRect, strokeRec)) {
        this->internalDrawPath(target, paint.isAntiAlias(), path, strokeInfo);
    }
}

void GrContext::internalDrawPath(GrDrawTarget* target, bool useAA, const SkPath& path,
                                 const GrStrokeInfo& strokeInfo) {
    SkASSERT(!path.isEmpty());

    GR_CREATE_TRACE_MARKER("GrContext::internalDrawPath", target);


    // An Assumption here is that path renderer would use some form of tweaking
    // the src color (either the input alpha or in the frag shader) to implement
    // aa. If we have some future driver-mojo path AA that can do the right
    // thing WRT to the blend then we'll need some query on the PR.
    bool useCoverageAA = useAA &&
        !target->getDrawState().getRenderTarget()->isMultisampled() &&
        !target->shouldDisableCoverageAAForBlend();


    GrPathRendererChain::DrawType type =
        useCoverageAA ? GrPathRendererChain::kColorAntiAlias_DrawType :
                           GrPathRendererChain::kColor_DrawType;

    const SkPath* pathPtr = &path;
    SkTLazy<SkPath> tmpPath;
    SkTCopyOnFirstWrite<SkStrokeRec> stroke(strokeInfo.getStrokeRec());

    // Try a 1st time without stroking the path and without allowing the SW renderer
    GrPathRenderer* pr = this->getPathRenderer(*pathPtr, *stroke, target, false, type);

    if (NULL == pr) {
        if (!GrPathRenderer::IsStrokeHairlineOrEquivalent(*stroke, this->getMatrix(), NULL)) {
            // It didn't work the 1st time, so try again with the stroked path
            if (stroke->applyToPath(tmpPath.init(), *pathPtr)) {
                pathPtr = tmpPath.get();
                stroke.writable()->setFillStyle();
                if (pathPtr->isEmpty()) {
                    return;
                }
            }
        }

        // This time, allow SW renderer
        pr = this->getPathRenderer(*pathPtr, *stroke, target, true, type);
    }

    if (NULL == pr) {
#ifdef SK_DEBUG
        SkDebugf("Unable to find path renderer compatible with path.\n");
#endif
        return;
    }

    pr->drawPath(*pathPtr, *stroke, target, useCoverageAA);
}

////////////////////////////////////////////////////////////////////////////////

void GrContext::flush(int flagsBitfield) {
    if (NULL == fDrawBuffer) {
        return;
    }

    if (kDiscard_FlushBit & flagsBitfield) {
        fDrawBuffer->reset();
    } else {
        fDrawBuffer->flush();
    }
    fResourceCache->purgeAsNeeded();
    fFlushToReduceCacheSize = false;
}

bool sw_convert_to_premul(GrPixelConfig srcConfig, int width, int height, size_t inRowBytes,
                          const void* inPixels, size_t outRowBytes, void* outPixels) {
    SkSrcPixelInfo srcPI;
    if (!GrPixelConfig2ColorType(srcConfig, &srcPI.fColorType)) {
        return false;
    }
    srcPI.fAlphaType = kUnpremul_SkAlphaType;
    srcPI.fPixels = inPixels;
    srcPI.fRowBytes = inRowBytes;

    SkDstPixelInfo dstPI;
    dstPI.fColorType = srcPI.fColorType;
    dstPI.fAlphaType = kPremul_SkAlphaType;
    dstPI.fPixels = outPixels;
    dstPI.fRowBytes = outRowBytes;

    return srcPI.convertPixelsTo(&dstPI, width, height);
}

bool GrContext::writeSurfacePixels(GrSurface* surface,
                                   int left, int top, int width, int height,
                                   GrPixelConfig srcConfig, const void* buffer, size_t rowBytes,
                                   uint32_t pixelOpsFlags) {

    {
        GrTexture* texture = NULL;
        if (!(kUnpremul_PixelOpsFlag & pixelOpsFlags) && (texture = surface->asTexture()) &&
            fGpu->canWriteTexturePixels(texture, srcConfig)) {

            if (!(kDontFlush_PixelOpsFlag & pixelOpsFlags) &&
                surface->surfacePriv().hasPendingIO()) {
                this->flush();
            }
            return fGpu->writeTexturePixels(texture, left, top, width, height,
                                            srcConfig, buffer, rowBytes);
            // Don't need to check kFlushWrites_PixelOp here, we just did a direct write so the
            // upload is already flushed.
        }
    }

    // If we didn't do a direct texture write then we upload the pixels to a texture and draw.
    GrRenderTarget* renderTarget = surface->asRenderTarget();
    if (NULL == renderTarget) {
        return false;
    }

    // We ignore the preferred config unless it is a R/B swap of the src config. In that case
    // we will upload the original src data to a scratch texture but we will spoof it as the swapped
    // config. This scratch will then have R and B swapped. We correct for this by swapping again
    // when drawing the scratch to the dst using a conversion effect.
    bool swapRAndB = false;
    GrPixelConfig writeConfig = srcConfig;
    if (GrPixelConfigSwapRAndB(srcConfig) ==
        fGpu->preferredWritePixelsConfig(srcConfig, renderTarget->config())) {
        writeConfig = GrPixelConfigSwapRAndB(srcConfig);
        swapRAndB = true;
    }

    GrSurfaceDesc desc;
    desc.fWidth = width;
    desc.fHeight = height;
    desc.fConfig = writeConfig;
    SkAutoTUnref<GrTexture> texture(this->refScratchTexture(desc, kApprox_ScratchTexMatch));
    if (!texture) {
        return false;
    }

    SkAutoTUnref<const GrFragmentProcessor> fp;
    SkMatrix textureMatrix;
    textureMatrix.setIDiv(texture->width(), texture->height());

    // allocate a tmp buffer and sw convert the pixels to premul
    SkAutoSTMalloc<128 * 128, uint32_t> tmpPixels(0);

    if (kUnpremul_PixelOpsFlag & pixelOpsFlags) {
        if (!GrPixelConfigIs8888(srcConfig)) {
            return false;
        }
        fp.reset(this->createUPMToPMEffect(texture, swapRAndB, textureMatrix));
        // handle the unpremul step on the CPU if we couldn't create an effect to do it.
        if (NULL == fp) {
            size_t tmpRowBytes = 4 * width;
            tmpPixels.reset(width * height);
            if (!sw_convert_to_premul(srcConfig, width, height, rowBytes, buffer, tmpRowBytes,
                                      tmpPixels.get())) {
                return false;
            }
            rowBytes = tmpRowBytes;
            buffer = tmpPixels.get();
        }
    }
    if (NULL == fp) {
        fp.reset(GrConfigConversionEffect::Create(texture,
                                                  swapRAndB,
                                                  GrConfigConversionEffect::kNone_PMConversion,
                                                  textureMatrix));
    }

    // Even if the client told us not to flush, we still flush here. The client may have known that
    // writes to the original surface caused no data hazards, but they can't know that the scratch
    // we just got is safe.
    if (texture->surfacePriv().hasPendingIO()) {
        this->flush();
    }
    if (!fGpu->writeTexturePixels(texture, 0, 0, width, height,
                                  writeConfig, buffer, rowBytes)) {
        return false;
    }

    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(left), SkIntToScalar(top));

    // This function can be called in the midst of drawing another object (e.g., when uploading a
    // SW-rasterized clip while issuing a draw). So we push the current geometry state before
    // drawing a rect to the render target.
    // The bracket ensures we pop the stack if we wind up flushing below.
    {
        GrDrawTarget* drawTarget = this->prepareToDraw(NULL, NULL, NULL);
        GrDrawTarget::AutoGeometryAndStatePush agasp(drawTarget, GrDrawTarget::kReset_ASRInit,
                                                     &matrix);
        GrDrawState* drawState = drawTarget->drawState();
        drawState->addColorProcessor(fp);
        drawState->setRenderTarget(renderTarget);
        drawState->disableState(GrDrawState::kClip_StateBit);
        drawTarget->drawSimpleRect(SkRect::MakeWH(SkIntToScalar(width), SkIntToScalar(height)));
    }

    if (kFlushWrites_PixelOp & pixelOpsFlags) {
        this->flushSurfaceWrites(surface);
    }

    return true;
}

// toggles between RGBA and BGRA
static SkColorType toggle_colortype32(SkColorType ct) {
    if (kRGBA_8888_SkColorType == ct) {
        return kBGRA_8888_SkColorType;
    } else {
        SkASSERT(kBGRA_8888_SkColorType == ct);
        return kRGBA_8888_SkColorType;
    }
}

bool GrContext::readRenderTargetPixels(GrRenderTarget* target,
                                       int left, int top, int width, int height,
                                       GrPixelConfig dstConfig, void* buffer, size_t rowBytes,
                                       uint32_t flags) {
    ASSERT_OWNED_RESOURCE(target);
    SkASSERT(target);

    if (!(kDontFlush_PixelOpsFlag & flags) && target->surfacePriv().hasPendingWrite()) {
        this->flush();
    }

    // Determine which conversions have to be applied: flipY, swapRAnd, and/or unpremul.

    // If fGpu->readPixels would incur a y-flip cost then we will read the pixels upside down. We'll
    // either do the flipY by drawing into a scratch with a matrix or on the cpu after the read.
    bool flipY = fGpu->readPixelsWillPayForYFlip(target, left, top,
                                                 width, height, dstConfig,
                                                 rowBytes);
    // We ignore the preferred config if it is different than our config unless it is an R/B swap.
    // In that case we'll perform an R and B swap while drawing to a scratch texture of the swapped
    // config. Then we will call readPixels on the scratch with the swapped config. The swaps during
    // the draw cancels out the fact that we call readPixels with a config that is R/B swapped from
    // dstConfig.
    GrPixelConfig readConfig = dstConfig;
    bool swapRAndB = false;
    if (GrPixelConfigSwapRAndB(dstConfig) ==
        fGpu->preferredReadPixelsConfig(dstConfig, target->config())) {
        readConfig = GrPixelConfigSwapRAndB(readConfig);
        swapRAndB = true;
    }

    bool unpremul = SkToBool(kUnpremul_PixelOpsFlag & flags);

    if (unpremul && !GrPixelConfigIs8888(dstConfig)) {
        // The unpremul flag is only allowed for these two configs.
        return false;
    }

    // If the src is a texture and we would have to do conversions after read pixels, we instead
    // do the conversions by drawing the src to a scratch texture. If we handle any of the
    // conversions in the draw we set the corresponding bool to false so that we don't reapply it
    // on the read back pixels.
    GrTexture* src = target->asTexture();
    if (src && (swapRAndB || unpremul || flipY)) {
        // Make the scratch a render so we can read its pixels.
        GrSurfaceDesc desc;
        desc.fFlags = kRenderTarget_GrSurfaceFlag;
        desc.fWidth = width;
        desc.fHeight = height;
        desc.fConfig = readConfig;
        desc.fOrigin = kTopLeft_GrSurfaceOrigin;

        // When a full read back is faster than a partial we could always make the scratch exactly
        // match the passed rect. However, if we see many different size rectangles we will trash
        // our texture cache and pay the cost of creating and destroying many textures. So, we only
        // request an exact match when the caller is reading an entire RT.
        ScratchTexMatch match = kApprox_ScratchTexMatch;
        if (0 == left &&
            0 == top &&
            target->width() == width &&
            target->height() == height &&
            fGpu->fullReadPixelsIsFasterThanPartial()) {
            match = kExact_ScratchTexMatch;
        }
        SkAutoTUnref<GrTexture> texture(this->refScratchTexture(desc, match));
        if (texture) {
            // compute a matrix to perform the draw
            SkMatrix textureMatrix;
            textureMatrix.setTranslate(SK_Scalar1 *left, SK_Scalar1 *top);
            textureMatrix.postIDiv(src->width(), src->height());

            SkAutoTUnref<const GrFragmentProcessor> fp;
            if (unpremul) {
                fp.reset(this->createPMToUPMEffect(src, swapRAndB, textureMatrix));
                if (fp) {
                    unpremul = false; // we no longer need to do this on CPU after the read back.
                }
            }
            // If we failed to create a PM->UPM effect and have no other conversions to perform then
            // there is no longer any point to using the scratch.
            if (fp || flipY || swapRAndB) {
                if (!fp) {
                    fp.reset(GrConfigConversionEffect::Create(
                            src, swapRAndB, GrConfigConversionEffect::kNone_PMConversion,
                            textureMatrix));
                }
                swapRAndB = false; // we will handle the swap in the draw.

                // We protect the existing geometry here since it may not be
                // clear to the caller that a draw operation (i.e., drawSimpleRect)
                // can be invoked in this method
                {
                    GrDrawTarget::AutoGeometryAndStatePush agasp(fDrawBuffer,
                                                                 GrDrawTarget::kReset_ASRInit);
                    GrDrawState* drawState = fDrawBuffer->drawState();
                    SkASSERT(fp);
                    drawState->addColorProcessor(fp);

                    drawState->setRenderTarget(texture->asRenderTarget());
                    SkRect rect = SkRect::MakeWH(SkIntToScalar(width), SkIntToScalar(height));
                    fDrawBuffer->drawSimpleRect(rect);
                    // we want to read back from the scratch's origin
                    left = 0;
                    top = 0;
                    target = texture->asRenderTarget();
                }
                this->flushSurfaceWrites(target);
            }
        }
    }

    if (!fGpu->readPixels(target,
                          left, top, width, height,
                          readConfig, buffer, rowBytes)) {
        return false;
    }
    // Perform any conversions we weren't able to perform using a scratch texture.
    if (unpremul || swapRAndB) {
        SkDstPixelInfo dstPI;
        if (!GrPixelConfig2ColorType(dstConfig, &dstPI.fColorType)) {
            return false;
        }
        dstPI.fAlphaType = kUnpremul_SkAlphaType;
        dstPI.fPixels = buffer;
        dstPI.fRowBytes = rowBytes;

        SkSrcPixelInfo srcPI;
        srcPI.fColorType = swapRAndB ? toggle_colortype32(dstPI.fColorType) : dstPI.fColorType;
        srcPI.fAlphaType = kPremul_SkAlphaType;
        srcPI.fPixels = buffer;
        srcPI.fRowBytes = rowBytes;

        return srcPI.convertPixelsTo(&dstPI, width, height);
    }
    return true;
}

void GrContext::prepareSurfaceForExternalRead(GrSurface* surface) {
    SkASSERT(surface);
    ASSERT_OWNED_RESOURCE(surface);
    if (surface->surfacePriv().hasPendingIO()) {
        this->flush();
    }
    GrRenderTarget* rt = surface->asRenderTarget();
    if (fGpu && rt) {
        fGpu->resolveRenderTarget(rt);
    }
}

void GrContext::discardRenderTarget(GrRenderTarget* renderTarget) {
    SkASSERT(renderTarget);
    ASSERT_OWNED_RESOURCE(renderTarget);
    AutoRestoreEffects are;
    AutoCheckFlush acf(this);
    GrDrawTarget* target = this->prepareToDraw(NULL, &are, &acf);
    if (NULL == target) {
        return;
    }
    target->discard(renderTarget);
}

void GrContext::copySurface(GrSurface* dst, GrSurface* src, const SkIRect& srcRect,
                            const SkIPoint& dstPoint, uint32_t pixelOpsFlags) {
    if (NULL == src || NULL == dst) {
        return;
    }
    ASSERT_OWNED_RESOURCE(src);
    ASSERT_OWNED_RESOURCE(dst);

    // Since we're going to the draw target and not GPU, no need to check kNoFlush
    // here.

    GrDrawTarget* target = this->prepareToDraw(NULL, NULL, NULL);
    if (NULL == target) {
        return;
    }
    target->copySurface(dst, src, srcRect, dstPoint);

    // always flush, this is not the behavior of TOT Skia but some uses of GrContext copySurface
    // in chrome rely on this behavior
    // see crbug:440671
    this->flush();
}

void GrContext::flushSurfaceWrites(GrSurface* surface) {
    if (surface->surfacePriv().hasPendingWrite()) {
        this->flush();
    }
}

////////////////////////////////////////////////////////////////////////////////

GrDrawTarget* GrContext::prepareToDraw(const GrPaint* paint,
                                       AutoRestoreEffects* are,
                                       AutoCheckFlush* acf) {
    // All users of this draw state should be freeing up all effects when they're done.
    // Otherwise effects that own resources may keep those resources alive indefinitely.
    SkASSERT(0 == fDrawState->numColorStages() && 0 == fDrawState->numCoverageStages() &&
             !fDrawState->hasGeometryProcessor());

    if (NULL == fGpu) {
        return NULL;
    }

    ASSERT_OWNED_RESOURCE(fRenderTarget.get());
    if (paint) {
        SkASSERT(are);
        SkASSERT(acf);
        are->set(fDrawState);
        fDrawState->setFromPaint(*paint, fViewMatrix, fRenderTarget.get());
#if GR_DEBUG_PARTIAL_COVERAGE_CHECK
        if ((paint->hasMask() || 0xff != paint->fCoverage) &&
            !fDrawState->couldApplyCoverage(fGpu->caps())) {
            SkDebugf("Partial pixel coverage will be incorrectly blended.\n");
        }
#endif
        // Clear any vertex attributes configured for the previous use of the
        // GrDrawState which can effect which blend optimizations are in effect.
        fDrawState->setDefaultVertexAttribs();
    } else {
        fDrawState->reset(fViewMatrix);
        fDrawState->setRenderTarget(fRenderTarget.get());
    }
    fDrawState->setState(GrDrawState::kClip_StateBit, fClip &&
                                                     !fClip->fClipStack->isWideOpen());
    fDrawBuffer->setClip(fClip);
    SkASSERT(fDrawState == fDrawBuffer->drawState());
    return fDrawBuffer;
}

/*
 * This method finds a path renderer that can draw the specified path on
 * the provided target.
 * Due to its expense, the software path renderer has split out so it can
 * can be individually allowed/disallowed via the "allowSW" boolean.
 */
GrPathRenderer* GrContext::getPathRenderer(const SkPath& path,
                                           const SkStrokeRec& stroke,
                                           const GrDrawTarget* target,
                                           bool allowSW,
                                           GrPathRendererChain::DrawType drawType,
                                           GrPathRendererChain::StencilSupport* stencilSupport) {

    if (NULL == fPathRendererChain) {
        fPathRendererChain = SkNEW_ARGS(GrPathRendererChain, (this));
    }

    GrPathRenderer* pr = fPathRendererChain->getPathRenderer(path,
                                                             stroke,
                                                             target,
                                                             drawType,
                                                             stencilSupport);

    if (NULL == pr && allowSW) {
        if (NULL == fSoftwarePathRenderer) {
            fSoftwarePathRenderer = SkNEW_ARGS(GrSoftwarePathRenderer, (this));
        }
        pr = fSoftwarePathRenderer;
    }

    return pr;
}

////////////////////////////////////////////////////////////////////////////////
bool GrContext::isConfigRenderable(GrPixelConfig config, bool withMSAA) const {
    return fGpu->caps()->isConfigRenderable(config, withMSAA);
}

int GrContext::getRecommendedSampleCount(GrPixelConfig config,
                                         SkScalar dpi) const {
    if (!this->isConfigRenderable(config, true)) {
        return 0;
    }
    int chosenSampleCount = 0;
    if (fGpu->caps()->pathRenderingSupport()) {
        if (dpi >= 250.0f) {
            chosenSampleCount = 4;
        } else {
            chosenSampleCount = 16;
        }
    }
    return chosenSampleCount <= fGpu->caps()->maxSampleCount() ?
        chosenSampleCount : 0;
}

void GrContext::setupDrawBuffer() {
    SkASSERT(NULL == fDrawBuffer);
    SkASSERT(NULL == fDrawBufferVBAllocPool);
    SkASSERT(NULL == fDrawBufferIBAllocPool);

    fDrawBufferVBAllocPool =
        SkNEW_ARGS(GrVertexBufferAllocPool, (fGpu, false,
                                    DRAW_BUFFER_VBPOOL_BUFFER_SIZE,
                                    DRAW_BUFFER_VBPOOL_PREALLOC_BUFFERS));
    fDrawBufferIBAllocPool =
        SkNEW_ARGS(GrIndexBufferAllocPool, (fGpu, false,
                                   DRAW_BUFFER_IBPOOL_BUFFER_SIZE,
                                   DRAW_BUFFER_IBPOOL_PREALLOC_BUFFERS));

    fDrawBuffer = SkNEW_ARGS(GrInOrderDrawBuffer, (fGpu,
                                                   fDrawBufferVBAllocPool,
                                                   fDrawBufferIBAllocPool));

    fDrawBuffer->setDrawState(fDrawState);
}

GrDrawTarget* GrContext::getTextTarget() {
    return this->prepareToDraw(NULL, NULL, NULL);
}

const GrIndexBuffer* GrContext::getQuadIndexBuffer() const {
    return fGpu->getQuadIndexBuffer();
}

namespace {
void test_pm_conversions(GrContext* ctx, int* pmToUPMValue, int* upmToPMValue) {
    GrConfigConversionEffect::PMConversion pmToUPM;
    GrConfigConversionEffect::PMConversion upmToPM;
    GrConfigConversionEffect::TestForPreservingPMConversions(ctx, &pmToUPM, &upmToPM);
    *pmToUPMValue = pmToUPM;
    *upmToPMValue = upmToPM;
}
}

const GrFragmentProcessor* GrContext::createPMToUPMEffect(GrTexture* texture,
                                                          bool swapRAndB,
                                                          const SkMatrix& matrix) {
    if (!fDidTestPMConversions) {
        test_pm_conversions(this, &fPMToUPMConversion, &fUPMToPMConversion);
        fDidTestPMConversions = true;
    }
    GrConfigConversionEffect::PMConversion pmToUPM =
        static_cast<GrConfigConversionEffect::PMConversion>(fPMToUPMConversion);
    if (GrConfigConversionEffect::kNone_PMConversion != pmToUPM) {
        return GrConfigConversionEffect::Create(texture, swapRAndB, pmToUPM, matrix);
    } else {
        return NULL;
    }
}

const GrFragmentProcessor* GrContext::createUPMToPMEffect(GrTexture* texture,
                                                          bool swapRAndB,
                                                          const SkMatrix& matrix) {
    if (!fDidTestPMConversions) {
        test_pm_conversions(this, &fPMToUPMConversion, &fUPMToPMConversion);
        fDidTestPMConversions = true;
    }
    GrConfigConversionEffect::PMConversion upmToPM =
        static_cast<GrConfigConversionEffect::PMConversion>(fUPMToPMConversion);
    if (GrConfigConversionEffect::kNone_PMConversion != upmToPM) {
        return GrConfigConversionEffect::Create(texture, swapRAndB, upmToPM, matrix);
    } else {
        return NULL;
    }
}

void GrContext::addResourceToCache(const GrResourceKey& resourceKey, GrGpuResource* resource) {
    fResourceCache->addResource(resourceKey, resource);
}

GrGpuResource* GrContext::findAndRefCachedResource(const GrResourceKey& resourceKey) {
    GrGpuResource* resource = fResourceCache->find(resourceKey);
    SkSafeRef(resource);
    return resource;
}

void GrContext::addGpuTraceMarker(const GrGpuTraceMarker* marker) {
    fGpu->addGpuTraceMarker(marker);
    if (fDrawBuffer) {
        fDrawBuffer->addGpuTraceMarker(marker);
    }
}

void GrContext::removeGpuTraceMarker(const GrGpuTraceMarker* marker) {
    fGpu->removeGpuTraceMarker(marker);
    if (fDrawBuffer) {
        fDrawBuffer->removeGpuTraceMarker(marker);
    }
}

///////////////////////////////////////////////////////////////////////////////
#if GR_CACHE_STATS
void GrContext::printCacheStats() const {
    fResourceCache->printStats();
}
#endif

#if GR_GPU_STATS
const GrContext::GPUStats* GrContext::gpuStats() const {
    return fGpu->gpuStats();
}
#endif

