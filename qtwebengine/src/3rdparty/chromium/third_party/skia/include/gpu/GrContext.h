/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrContext_DEFINED
#define GrContext_DEFINED

#include "GrClipData.h"
#include "GrColor.h"
#include "GrPaint.h"
#include "GrPathRendererChain.h"
#include "GrRenderTarget.h"
#include "GrTexture.h"
#include "SkMatrix.h"
#include "SkPathEffect.h"
#include "SkTypes.h"

class GrAARectRenderer;
class GrDrawState;
class GrDrawTarget;
class GrFontCache;
class GrFragmentProcessor;
class GrGpu;
class GrGpuTraceMarker;
class GrIndexBuffer;
class GrIndexBufferAllocPool;
class GrInOrderDrawBuffer;
class GrLayerCache;
class GrOvalRenderer;
class GrPath;
class GrPathRenderer;
class GrResourceEntry;
class GrResourceCache;
class GrResourceCache2;
class GrStencilBuffer;
class GrTestTarget;
class GrTextContext;
class GrTextureParams;
class GrVertexBuffer;
class GrVertexBufferAllocPool;
class GrStrokeInfo;
class GrSoftwarePathRenderer;
class SkStrokeRec;

class SK_API GrContext : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(GrContext)

    struct Options {
        Options() : fDrawPathToCompressedTexture(false) { }

        // EXPERIMENTAL
        // May be removed in the future, or may become standard depending
        // on the outcomes of a variety of internal tests.
        bool fDrawPathToCompressedTexture;
    };

    /**
     * Creates a GrContext for a backend context.
     */
    static GrContext* Create(GrBackend, GrBackendContext, const Options* opts = NULL);

    /**
     * Only defined in test apps.
     */
    static GrContext* CreateMockContext();

    virtual ~GrContext();

    /**
     * The GrContext normally assumes that no outsider is setting state
     * within the underlying 3D API's context/device/whatever. This call informs
     * the context that the state was modified and it should resend. Shouldn't
     * be called frequently for good performance.
     * The flag bits, state, is dpendent on which backend is used by the
     * context, either GL or D3D (possible in future).
     */
    void resetContext(uint32_t state = kAll_GrBackendState);

    /**
     * Callback function to allow classes to cleanup on GrContext destruction.
     * The 'info' field is filled in with the 'info' passed to addCleanUp.
     */
    typedef void (*PFCleanUpFunc)(const GrContext* context, void* info);

    /**
     * Add a function to be called from within GrContext's destructor.
     * This gives classes a chance to free resources held on a per context basis.
     * The 'info' parameter will be stored and passed to the callback function.
     */
    void addCleanUp(PFCleanUpFunc cleanUp, void* info) {
        CleanUpData* entry = fCleanUpData.push();

        entry->fFunc = cleanUp;
        entry->fInfo = info;
    }

    /**
     * Abandons all GPU resources and assumes the underlying backend 3D API 
     * context is not longer usable. Call this if you have lost the associated
     * GPU context, and thus internal texture, buffer, etc. references/IDs are
     * now invalid. Should be called even when GrContext is no longer going to
     * be used for two reasons:
     *  1) ~GrContext will not try to free the objects in the 3D API.
     *  2) Any GrGpuResources created by this GrContext that outlive
     *     will be marked as invalid (GrGpuResource::wasDestroyed()) and
     *     when they're destroyed no 3D API calls will be made.
     * Content drawn since the last GrContext::flush() may be lost. After this
     * function is called the only valid action on the GrContext or
     * GrGpuResources it created is to destroy them.
     */
    void abandonContext();
    void contextDestroyed() { this->abandonContext(); }  //  legacy alias

    ///////////////////////////////////////////////////////////////////////////
    // Resource Cache

    /**
     *  Return the current GPU resource cache limits.
     *
     *  @param maxResources If non-null, returns maximum number of resources that
     *                      can be held in the cache.
     *  @param maxResourceBytes If non-null, returns maximum number of bytes of
     *                          video memory that can be held in the cache.
     */
    void getResourceCacheLimits(int* maxResources, size_t* maxResourceBytes) const;

    /**
     *  Gets the current GPU resource cache usage.
     *
     *  @param resourceCount If non-null, returns the number of resources that are held in the
     *                       cache.
     *  @param maxResourceBytes If non-null, returns the total number of bytes of video memory held
     *                          in the cache.
     */
    void getResourceCacheUsage(int* resourceCount, size_t* resourceBytes) const;

    /**
     *  Specify the GPU resource cache limits. If the current cache exceeds either
     *  of these, it will be purged (LRU) to keep the cache within these limits.
     *
     *  @param maxResources The maximum number of resources that can be held in
     *                      the cache.
     *  @param maxResourceBytes The maximum number of bytes of video memory
     *                          that can be held in the cache.
     */
    void setResourceCacheLimits(int maxResources, size_t maxResourceBytes);

    /**
     * Frees GPU created by the context. Can be called to reduce GPU memory
     * pressure.
     */
    void freeGpuResources();

    /**
     * This method should be called whenever a GrResource is unreffed or
     * switched from exclusive to non-exclusive. This
     * gives the resource cache a chance to discard unneeded resources.
     * Note: this entry point will be removed once totally ref-driven
     * cache maintenance is implemented.
     */
    void purgeCache();

    /**
     * Purge all the unlocked resources from the cache.
     * This entry point is mainly meant for timing texture uploads
     * and is not defined in normal builds of Skia.
     */
    void purgeAllUnlockedResources();

    /**
     * Stores a custom resource in the cache, based on the specified key.
     */
    void addResourceToCache(const GrResourceKey&, GrGpuResource*);

    /**
     * Finds a resource in the cache, based on the specified key. This is intended for use in
     * conjunction with addResourceToCache(). The return value will be NULL if not found. The
     * caller must balance with a call to unref().
     */
    GrGpuResource* findAndRefCachedResource(const GrResourceKey&);

    /**
     * Creates a new text rendering context that is optimal for the
     * render target and the context. Caller assumes the ownership
     * of the returned object. The returned object must be deleted
     * before the context is destroyed.
     */
    GrTextContext* createTextContext(GrRenderTarget*,
                                     const SkDeviceProperties&,
                                     bool enableDistanceFieldFonts);

    ///////////////////////////////////////////////////////////////////////////
    // Textures

    /**
     * Creates a new entry, based on the specified key and texture and returns it. The caller owns a
     * ref on the returned texture which must be balanced by a call to unref.
     *
     * @param params    The texture params used to draw a texture may help determine
     *                  the cache entry used. (e.g. different versions may exist
     *                  for different wrap modes on GPUs with limited NPOT
     *                  texture support). NULL implies clamp wrap modes.
     * @param desc      Description of the texture properties.
     * @param cacheID   Cache-specific properties (e.g., texture gen ID)
     * @param srcData   Pointer to the pixel values.
     * @param rowBytes  The number of bytes between rows of the texture. Zero
     *                  implies tightly packed rows. For compressed pixel configs, this
     *                  field is ignored.
     * @param cacheKey  (optional) If non-NULL, we'll write the cache key we used to cacheKey.
     */
    GrTexture* createTexture(const GrTextureParams* params,
                             const GrSurfaceDesc& desc,
                             const GrCacheID& cacheID,
                             const void* srcData,
                             size_t rowBytes,
                             GrResourceKey* cacheKey = NULL);
    /**
     * Search for an entry based on key and dimensions. If found, ref it and return it. The return
     * value will be NULL if not found. The caller must balance with a call to unref.
     *
     *  @param desc     Description of the texture properties.
     *  @param cacheID Cache-specific properties (e.g., texture gen ID)
     *  @param params   The texture params used to draw a texture may help determine
     *                  the cache entry used. (e.g. different versions may exist
     *                  for different wrap modes on GPUs with limited NPOT
     *                  texture support). NULL implies clamp wrap modes.
     */
    GrTexture* findAndRefTexture(const GrSurfaceDesc& desc,
                                 const GrCacheID& cacheID,
                                 const GrTextureParams* params);
    /**
     * Determines whether a texture is in the cache. If the texture is found it
     * will not be locked or returned. This call does not affect the priority of
     * the texture for deletion.
     */
    bool isTextureInCache(const GrSurfaceDesc& desc,
                          const GrCacheID& cacheID,
                          const GrTextureParams* params) const;

    /**
     * Enum that determines how closely a returned scratch texture must match
     * a provided GrSurfaceDesc.
     */
    enum ScratchTexMatch {
        /**
         * Finds a texture that exactly matches the descriptor.
         */
        kExact_ScratchTexMatch,
        /**
         * Finds a texture that approximately matches the descriptor. Will be
         * at least as large in width and height as desc specifies. If desc
         * specifies that texture is a render target then result will be a
         * render target. If desc specifies a render target and doesn't set the
         * no stencil flag then result will have a stencil. Format and aa level
         * will always match.
         */
        kApprox_ScratchTexMatch
    };

    /**
     * Returns a texture matching the desc. It's contents are unknown. The caller
     * owns a ref on the returned texture and must balance with a call to unref.
     * It is guaranteed that the same texture will not be returned in subsequent
     * calls until all refs to the texture are dropped.
     *
     * Textures created by createTexture() hide the complications of
     * tiling non-power-of-two textures on APIs that don't support this (e.g.
     * unextended GLES2). NPOT scratch textures are not tilable on such APIs.
     *
     * internalFlag is a temporary workaround until changes in the internal
     * architecture are complete. Use the default value.
     */
    GrTexture* refScratchTexture(const GrSurfaceDesc&, ScratchTexMatch match,
                                 bool internalFlag = false);

    /**
     * Creates a texture that is outside the cache. Does not count against
     * cache's budget.
     *
     * Textures created by createTexture() hide the complications of
     * tiling non-power-of-two textures on APIs that don't support this (e.g.
     * unextended GLES2). NPOT uncached textures are not tilable on such APIs.
     */
    GrTexture* createUncachedTexture(const GrSurfaceDesc& desc,
                                     void* srcData,
                                     size_t rowBytes);

    /**
     * Returns true if the specified use of an indexed texture is supported.
     * Support may depend upon whether the texture params indicate that the
     * texture will be tiled. Passing NULL for the texture params indicates
     * clamp mode.
     */
    bool supportsIndex8PixelConfig(const GrTextureParams*,
                                   int width,
                                   int height) const;

    /**
     *  Return the max width or height of a texture supported by the current GPU.
     */
    int getMaxTextureSize() const;

    /**
     *  Temporarily override the true max texture size. Note: an override
     *  larger then the true max texture size will have no effect.
     *  This entry point is mainly meant for testing texture size dependent
     *  features and is only available if defined outside of Skia (see
     *  bleed GM.
     */
    void setMaxTextureSizeOverride(int maxTextureSizeOverride);

    ///////////////////////////////////////////////////////////////////////////
    // Render targets

    /**
     * Sets the render target.
     * @param target    the render target to set.
     */
    void setRenderTarget(GrRenderTarget* target) {
        fRenderTarget.reset(SkSafeRef(target));
    }

    /**
     * Gets the current render target.
     * @return the currently bound render target.
     */
    const GrRenderTarget* getRenderTarget() const { return fRenderTarget.get(); }
    GrRenderTarget* getRenderTarget() { return fRenderTarget.get(); }

    /**
     * Can the provided configuration act as a color render target?
     */
    bool isConfigRenderable(GrPixelConfig config, bool withMSAA) const;

    /**
     * Return the max width or height of a render target supported by the
     * current GPU.
     */
    int getMaxRenderTargetSize() const;

    /**
     * Returns the max sample count for a render target. It will be 0 if MSAA
     * is not supported.
     */
    int getMaxSampleCount() const;

    /**
     * Returns the recommended sample count for a render target when using this
     * context.
     *
     * @param  config the configuration of the render target.
     * @param  dpi the display density in dots per inch.
     *
     * @return sample count that should be perform well and have good enough
     *         rendering quality for the display. Alternatively returns 0 if
     *         MSAA is not supported or recommended to be used by default.
     */
    int getRecommendedSampleCount(GrPixelConfig config, SkScalar dpi) const;

    ///////////////////////////////////////////////////////////////////////////
    // Backend Surfaces

    /**
     * Wraps an existing texture with a GrTexture object.
     *
     * OpenGL: if the object is a texture Gr may change its GL texture params
     *         when it is drawn.
     *
     * @param  desc     description of the object to create.
     *
     * @return GrTexture object or NULL on failure.
     */
    GrTexture* wrapBackendTexture(const GrBackendTextureDesc& desc);

    /**
     * Wraps an existing render target with a GrRenderTarget object. It is
     * similar to wrapBackendTexture but can be used to draw into surfaces
     * that are not also textures (e.g. FBO 0 in OpenGL, or an MSAA buffer that
     * the client will resolve to a texture).
     *
     * @param  desc     description of the object to create.
     *
     * @return GrTexture object or NULL on failure.
     */
     GrRenderTarget* wrapBackendRenderTarget(const GrBackendRenderTargetDesc& desc);

    ///////////////////////////////////////////////////////////////////////////
    // Matrix state

    /**
     * Gets the current transformation matrix.
     * @return the current matrix.
     */
    const SkMatrix& getMatrix() const { return fViewMatrix; }

    /**
     * Sets the transformation matrix.
     * @param m the matrix to set.
     */
    void setMatrix(const SkMatrix& m) { fViewMatrix = m; }

    /**
     * Sets the current transformation matrix to identity.
     */
    void setIdentityMatrix() { fViewMatrix.reset(); }

    /**
     * Concats the current matrix. The passed matrix is applied before the
     * current matrix.
     * @param m the matrix to concat.
     */
    void concatMatrix(const SkMatrix& m) { fViewMatrix.preConcat(m); }


    ///////////////////////////////////////////////////////////////////////////
    // Clip state
    /**
     * Gets the current clip.
     * @return the current clip.
     */
    const GrClipData* getClip() const { return fClip; }

    /**
     * Sets the clip.
     * @param clipData  the clip to set.
     */
    void setClip(const GrClipData* clipData) { fClip = clipData; }

    ///////////////////////////////////////////////////////////////////////////
    // Draws

    /**
     * Clear the entire or rect of the render target, ignoring any clips.
     * @param rect  the rect to clear or the whole thing if rect is NULL.
     * @param color the color to clear to.
     * @param canIgnoreRect allows partial clears to be converted to whole
     *                      clears on platforms for which that is cheap
     * @param target The render target to clear.
     */
    void clear(const SkIRect* rect, GrColor color, bool canIgnoreRect, GrRenderTarget* target);

    /**
     *  Draw everywhere (respecting the clip) with the paint.
     */
    void drawPaint(const GrPaint& paint);

    /**
     *  Draw the rect using a paint.
     *  @param paint        describes how to color pixels.
     *  @param strokeInfo   the stroke information (width, join, cap), and.
     *                      the dash information (intervals, count, phase).
     *                      If strokeInfo == NULL, then the rect is filled.
     *                      Otherwise, if stroke width == 0, then the stroke
     *                      is always a single pixel thick, else the rect is
     *                      mitered/beveled stroked based on stroke width.
     *  The rects coords are used to access the paint (through texture matrix)
     */
    void drawRect(const GrPaint& paint,
                  const SkRect&,
                  const GrStrokeInfo* strokeInfo = NULL);

    /**
     * Maps a rect of local coordinates onto the a rect of destination
     * coordinates. The localRect is stretched over the dstRect. The dstRect is
     * transformed by the context's matrix. An additional optional matrix can be
     *  provided to transform the local rect.
     *
     * @param paint         describes how to color pixels.
     * @param dstRect       the destination rect to draw.
     * @param localRect     rect of local coordinates to be mapped onto dstRect
     * @param localMatrix   Optional matrix to transform localRect.
     */
    void drawRectToRect(const GrPaint& paint,
                        const SkRect& dstRect,
                        const SkRect& localRect,
                        const SkMatrix* localMatrix = NULL);

    /**
     *  Draw a roundrect using a paint.
     *
     *  @param paint        describes how to color pixels.
     *  @param rrect        the roundrect to draw
     *  @param strokeInfo   the stroke information (width, join, cap) and
     *                      the dash information (intervals, count, phase).
     */
    void drawRRect(const GrPaint& paint, const SkRRect& rrect, const GrStrokeInfo& strokeInfo);

    /**
     *  Shortcut for drawing an SkPath consisting of nested rrects using a paint.
     *  Does not support stroking. The result is undefined if outer does not contain
     *  inner.
     *
     *  @param paint        describes how to color pixels.
     *  @param outer        the outer roundrect
     *  @param inner        the inner roundrect
     */
    void drawDRRect(const GrPaint& paint, const SkRRect& outer, const SkRRect& inner);


    /**
     * Draws a path.
     *
     * @param paint         describes how to color pixels.
     * @param path          the path to draw
     * @param strokeInfo    the stroke information (width, join, cap) and
     *                      the dash information (intervals, count, phase).
     */
    void drawPath(const GrPaint& paint, const SkPath& path, const GrStrokeInfo& strokeInfo);

    /**
     * Draws vertices with a paint.
     *
     * @param   paint           describes how to color pixels.
     * @param   primitiveType   primitives type to draw.
     * @param   vertexCount     number of vertices.
     * @param   positions       array of vertex positions, required.
     * @param   texCoords       optional array of texture coordinates used
     *                          to access the paint.
     * @param   colors          optional array of per-vertex colors, supercedes
     *                          the paint's color field.
     * @param   indices         optional array of indices. If NULL vertices
     *                          are drawn non-indexed.
     * @param   indexCount      if indices is non-null then this is the
     *                          number of indices.
     */
    void drawVertices(const GrPaint& paint,
                      GrPrimitiveType primitiveType,
                      int vertexCount,
                      const SkPoint positions[],
                      const SkPoint texs[],
                      const GrColor colors[],
                      const uint16_t indices[],
                      int indexCount);

    /**
     * Draws an oval.
     *
     * @param paint         describes how to color pixels.
     * @param oval          the bounding rect of the oval.
     * @param strokeInfo    the stroke information (width, join, cap) and
     *                      the dash information (intervals, count, phase).
     */
    void drawOval(const GrPaint& paint,
                  const SkRect& oval,
                  const GrStrokeInfo& strokeInfo);

    ///////////////////////////////////////////////////////////////////////////
    // Misc.

    /**
     * Flags that affect flush() behavior.
     */
    enum FlushBits {
        /**
         * A client may reach a point where it has partially rendered a frame
         * through a GrContext that it knows the user will never see. This flag
         * causes the flush to skip submission of deferred content to the 3D API
         * during the flush.
         */
        kDiscard_FlushBit                    = 0x2,
    };

    /**
     * Call to ensure all drawing to the context has been issued to the
     * underlying 3D API.
     * @param flagsBitfield     flags that control the flushing behavior. See
     *                          FlushBits.
     */
    void flush(int flagsBitfield = 0);

   /**
    * These flags can be used with the read/write pixels functions below.
    */
    enum PixelOpsFlags {
        /** The GrContext will not be flushed before the surface read or write. This means that
            the read or write may occur before previous draws have executed. */
        kDontFlush_PixelOpsFlag = 0x1,
        /** Any surface writes should be flushed to the backend 3D API after the surface operation
            is complete */
        kFlushWrites_PixelOp = 0x2,
        /** The src for write or dst read is unpremultiplied. This is only respected if both the
            config src and dst configs are an RGBA/BGRA 8888 format. */
        kUnpremul_PixelOpsFlag  = 0x4,
    };

    /**
     * Reads a rectangle of pixels from a render target.
     * @param target        the render target to read from.
     * @param left          left edge of the rectangle to read (inclusive)
     * @param top           top edge of the rectangle to read (inclusive)
     * @param width         width of rectangle to read in pixels.
     * @param height        height of rectangle to read in pixels.
     * @param config        the pixel config of the destination buffer
     * @param buffer        memory to read the rectangle into.
     * @param rowBytes      number of bytes bewtween consecutive rows. Zero means rows are tightly
     *                      packed.
     * @param pixelOpsFlags see PixelOpsFlags enum above.
     *
     * @return true if the read succeeded, false if not. The read can fail because of an unsupported
     *         pixel config or because no render target is currently set and NULL was passed for
     *         target.
     */
    bool readRenderTargetPixels(GrRenderTarget* target,
                                int left, int top, int width, int height,
                                GrPixelConfig config, void* buffer,
                                size_t rowBytes = 0,
                                uint32_t pixelOpsFlags = 0);

    /**
     * Writes a rectangle of pixels to a surface.
     * @param surface       the surface to write to.
     * @param left          left edge of the rectangle to write (inclusive)
     * @param top           top edge of the rectangle to write (inclusive)
     * @param width         width of rectangle to write in pixels.
     * @param height        height of rectangle to write in pixels.
     * @param config        the pixel config of the source buffer
     * @param buffer        memory to read pixels from
     * @param rowBytes      number of bytes between consecutive rows. Zero
     *                      means rows are tightly packed.
     * @param pixelOpsFlags see PixelOpsFlags enum above.
     * @return true if the write succeeded, false if not. The write can fail because of an
     *         unsupported combination of surface and src configs.
     */
    bool writeSurfacePixels(GrSurface* surface,
                            int left, int top, int width, int height,
                            GrPixelConfig config, const void* buffer,
                            size_t rowBytes,
                            uint32_t pixelOpsFlags = 0);

    /**
     * Copies a rectangle of texels from src to dst.
     * bounds.
     * @param dst           the surface to copy to.
     * @param src           the surface to copy from.
     * @param srcRect       the rectangle of the src that should be copied.
     * @param dstPoint      the translation applied when writing the srcRect's pixels to the dst.
     * @param pixelOpsFlags see PixelOpsFlags enum above. (kUnpremul_PixelOpsFlag is not allowed).
     */
    void copySurface(GrSurface* dst,
                     GrSurface* src,
                     const SkIRect& srcRect,
                     const SkIPoint& dstPoint,
                     uint32_t pixelOpsFlags = 0);

    /** Helper that copies the whole surface but fails when the two surfaces are not identically
        sized. */
    bool copySurface(GrSurface* dst, GrSurface* src) {
        if (NULL == dst || NULL == src || dst->width() != src->width() ||
            dst->height() != src->height()) {
            return false;
        }
        this->copySurface(dst, src, SkIRect::MakeWH(dst->width(), dst->height()),
                          SkIPoint::Make(0,0));
        return true;
    }

    /**
     * After this returns any pending writes to the surface will have been issued to the backend 3D API.
     */
    void flushSurfaceWrites(GrSurface* surface);

    /**
     * Equivalent to flushSurfaceWrites but also performs MSAA resolve if necessary. This call is
     * used to make the surface contents available to be read in the backend 3D API, usually for a
     * compositing step external to Skia.
     *
     * It is not necessary to call this before reading the render target via Skia/GrContext.
     * GrContext will detect when it must perform a resolve before reading pixels back from the
     * surface or using it as a texture.
     */
    void prepareSurfaceForExternalRead(GrSurface*);

    /**
     * Provides a perfomance hint that the render target's contents are allowed
     * to become undefined.
     */
    void discardRenderTarget(GrRenderTarget*);

#ifdef SK_DEVELOPER
    void dumpFontCache() const;
#endif

    ///////////////////////////////////////////////////////////////////////////
    // Helpers

    class AutoRenderTarget : public ::SkNoncopyable {
    public:
        AutoRenderTarget(GrContext* context, GrRenderTarget* target) {
            fPrevTarget = context->getRenderTarget();
            SkSafeRef(fPrevTarget);
            context->setRenderTarget(target);
            fContext = context;
        }
        AutoRenderTarget(GrContext* context) {
            fPrevTarget = context->getRenderTarget();
            SkSafeRef(fPrevTarget);
            fContext = context;
        }
        ~AutoRenderTarget() {
            if (fContext) {
                fContext->setRenderTarget(fPrevTarget);
            }
            SkSafeUnref(fPrevTarget);
        }
    private:
        GrContext*      fContext;
        GrRenderTarget* fPrevTarget;
    };

    /**
     * Save/restore the view-matrix in the context. It can optionally adjust a paint to account
     * for a coordinate system change. Here is an example of how the paint param can be used:
     *
     * A GrPaint is setup with GrProcessors. The stages will have access to the pre-matrix source
     * geometry positions when the draw is executed. Later on a decision is made to transform the
     * geometry to device space on the CPU. The effects now need to know that the space in which
     * the geometry will be specified has changed.
     *
     * Note that when restore is called (or in the destructor) the context's matrix will be
     * restored. However, the paint will not be restored. The caller must make a copy of the
     * paint if necessary. Hint: use SkTCopyOnFirstWrite if the AutoMatrix is conditionally
     * initialized.
     */
    class AutoMatrix : public ::SkNoncopyable {
    public:
        AutoMatrix() : fContext(NULL) {}

        ~AutoMatrix() { this->restore(); }

        /**
         * Initializes by pre-concat'ing the context's current matrix with the preConcat param.
         */
        void setPreConcat(GrContext* context, const SkMatrix& preConcat, GrPaint* paint = NULL) {
            SkASSERT(context);

            this->restore();

            fContext = context;
            fMatrix = context->getMatrix();
            this->preConcat(preConcat, paint);
        }

        /**
         * Sets the context's matrix to identity. Returns false if the inverse matrix is required to
         * update a paint but the matrix cannot be inverted.
         */
        bool setIdentity(GrContext* context, GrPaint* paint = NULL) {
            SkASSERT(context);

            this->restore();

            if (paint) {
                if (!paint->localCoordChangeInverse(context->getMatrix())) {
                    return false;
                }
            }
            fMatrix = context->getMatrix();
            fContext = context;
            context->setIdentityMatrix();
            return true;
        }

        /**
         * Replaces the context's matrix with a new matrix. Returns false if the inverse matrix is
         * required to update a paint but the matrix cannot be inverted.
         */
        bool set(GrContext* context, const SkMatrix& newMatrix, GrPaint* paint = NULL) {
            if (paint) {
                if (!this->setIdentity(context, paint)) {
                    return false;
                }
                this->preConcat(newMatrix, paint);
            } else {
                this->restore();
                fContext = context;
                fMatrix = context->getMatrix();
                context->setMatrix(newMatrix);
            }
            return true;
        }

        /**
         * If this has been initialized then the context's matrix will be further updated by
         * pre-concat'ing the preConcat param. The matrix that will be restored remains unchanged.
         * The paint is assumed to be relative to the context's matrix at the time this call is
         * made, not the matrix at the time AutoMatrix was first initialized. In other words, this
         * performs an incremental update of the paint.
         */
        void preConcat(const SkMatrix& preConcat, GrPaint* paint = NULL) {
            if (paint) {
                paint->localCoordChange(preConcat);
            }
            fContext->concatMatrix(preConcat);
        }

        /**
         * Returns false if never initialized or the inverse matrix was required to update a paint
         * but the matrix could not be inverted.
         */
        bool succeeded() const { return SkToBool(fContext); }

        /**
         * If this has been initialized then the context's original matrix is restored.
         */
        void restore() {
            if (fContext) {
                fContext->setMatrix(fMatrix);
                fContext = NULL;
            }
        }

    private:
        GrContext*  fContext;
        SkMatrix    fMatrix;
    };

    class AutoClip : public ::SkNoncopyable {
    public:
        // This enum exists to require a caller of the constructor to acknowledge that the clip will
        // initially be wide open. It also could be extended if there are other desirable initial
        // clip states.
        enum InitialClip {
            kWideOpen_InitialClip,
        };

        AutoClip(GrContext* context, InitialClip initialState)
        : fContext(context) {
            SkASSERT(kWideOpen_InitialClip == initialState);
            fNewClipData.fClipStack = &fNewClipStack;

            fOldClip = context->getClip();
            context->setClip(&fNewClipData);
        }

        AutoClip(GrContext* context, const SkRect& newClipRect)
        : fContext(context)
        , fNewClipStack(newClipRect) {
            fNewClipData.fClipStack = &fNewClipStack;

            fOldClip = fContext->getClip();
            fContext->setClip(&fNewClipData);
        }

        ~AutoClip() {
            if (fContext) {
                fContext->setClip(fOldClip);
            }
        }
    private:
        GrContext*        fContext;
        const GrClipData* fOldClip;

        SkClipStack       fNewClipStack;
        GrClipData        fNewClipData;
    };

    class AutoWideOpenIdentityDraw {
    public:
        AutoWideOpenIdentityDraw(GrContext* ctx, GrRenderTarget* rt)
            : fAutoClip(ctx, AutoClip::kWideOpen_InitialClip)
            , fAutoRT(ctx, rt) {
            fAutoMatrix.setIdentity(ctx);
            // should never fail with no paint param.
            SkASSERT(fAutoMatrix.succeeded());
        }

    private:
        AutoClip fAutoClip;
        AutoRenderTarget fAutoRT;
        AutoMatrix fAutoMatrix;
    };

    ///////////////////////////////////////////////////////////////////////////
    // Functions intended for internal use only.
    GrGpu* getGpu() { return fGpu; }
    const GrGpu* getGpu() const { return fGpu; }
    GrFontCache* getFontCache() { return fFontCache; }
    GrLayerCache* getLayerCache() { return fLayerCache.get(); }
    GrDrawTarget* getTextTarget();
    const GrIndexBuffer* getQuadIndexBuffer() const;
    GrAARectRenderer* getAARectRenderer() { return fAARectRenderer; }
    GrResourceCache* getResourceCache() { return fResourceCache; }
    GrResourceCache2* getResourceCache2() { return fResourceCache2; }

    // Called by tests that draw directly to the context via GrDrawTarget
    void getTestTarget(GrTestTarget*);

    void addGpuTraceMarker(const GrGpuTraceMarker* marker);
    void removeGpuTraceMarker(const GrGpuTraceMarker* marker);

    /**
     * Stencil buffers add themselves to the cache using addStencilBuffer. findStencilBuffer is
     * called to check the cache for a SB that matches an RT's criteria.
     */
    void addStencilBuffer(GrStencilBuffer* sb);
    GrStencilBuffer* findStencilBuffer(int width, int height, int sampleCnt);

    GrPathRenderer* getPathRenderer(
                    const SkPath& path,
                    const SkStrokeRec& stroke,
                    const GrDrawTarget* target,
                    bool allowSW,
                    GrPathRendererChain::DrawType drawType = GrPathRendererChain::kColor_DrawType,
                    GrPathRendererChain::StencilSupport* stencilSupport = NULL);

    /**
     *  This returns a copy of the the GrContext::Options that was passed to the
     *  constructor of this class.
     */
    const Options& getOptions() const { return fOptions; }

#if GR_CACHE_STATS
    void printCacheStats() const;
#endif

    class GPUStats {
    public:
#if GR_GPU_STATS
        GPUStats() { this->reset(); }

        void reset() { fRenderTargetBinds = 0; fShaderCompilations = 0; }

        int renderTargetBinds() const { return fRenderTargetBinds; }
        void incRenderTargetBinds() { fRenderTargetBinds++; }
        int shaderCompilations() const { return fShaderCompilations; }
        void incShaderCompilations() { fShaderCompilations++; }
    private:
        int fRenderTargetBinds;
        int fShaderCompilations;
#else
        void incRenderTargetBinds() {}
        void incShaderCompilations() {}
#endif
    };

#if GR_GPU_STATS
    const GPUStats* gpuStats() const;
#endif

private:
    GrGpu*                          fGpu;
    SkMatrix                        fViewMatrix;
    SkAutoTUnref<GrRenderTarget>    fRenderTarget;
    const GrClipData*               fClip;  // TODO: make this ref counted
    GrDrawState*                    fDrawState;

    GrResourceCache*                fResourceCache;
    GrResourceCache2*               fResourceCache2;
    GrFontCache*                    fFontCache;
    SkAutoTDelete<GrLayerCache>     fLayerCache;

    GrPathRendererChain*            fPathRendererChain;
    GrSoftwarePathRenderer*         fSoftwarePathRenderer;

    GrVertexBufferAllocPool*        fDrawBufferVBAllocPool;
    GrIndexBufferAllocPool*         fDrawBufferIBAllocPool;
    GrInOrderDrawBuffer*            fDrawBuffer;

    // Set by OverbudgetCB() to request that GrContext flush before exiting a draw.
    bool                            fFlushToReduceCacheSize;

    GrAARectRenderer*               fAARectRenderer;
    GrOvalRenderer*                 fOvalRenderer;

    bool                            fDidTestPMConversions;
    int                             fPMToUPMConversion;
    int                             fUPMToPMConversion;

    struct CleanUpData {
        PFCleanUpFunc fFunc;
        void*         fInfo;
    };

    SkTDArray<CleanUpData>          fCleanUpData;

    int                             fMaxTextureSizeOverride;

    const Options                   fOptions;

    GrContext(const Options&); // init must be called after the constructor.
    bool init(GrBackend, GrBackendContext);
    void initMockContext();
    void initCommon();

    void setupDrawBuffer();

    class AutoRestoreEffects;
    class AutoCheckFlush;
    /// Sets the paint and returns the target to draw into. The paint can be NULL in which case the
    /// draw state is left unmodified.
    GrDrawTarget* prepareToDraw(const GrPaint*, AutoRestoreEffects*, AutoCheckFlush*);

    void internalDrawPath(GrDrawTarget* target, bool useAA, const SkPath& path,
                          const GrStrokeInfo& stroke);

    GrTexture* createResizedTexture(const GrSurfaceDesc& desc,
                                    const GrCacheID& cacheID,
                                    const void* srcData,
                                    size_t rowBytes,
                                    bool filter);

    GrTexture* createNewScratchTexture(const GrSurfaceDesc& desc);

    /**
     * These functions create premul <-> unpremul effects if it is possible to generate a pair
     * of effects that make a readToUPM->writeToPM->readToUPM cycle invariant. Otherwise, they
     * return NULL.
     */
    const GrFragmentProcessor* createPMToUPMEffect(GrTexture*, bool swapRAndB, const SkMatrix&);
    const GrFragmentProcessor* createUPMToPMEffect(GrTexture*, bool swapRAndB, const SkMatrix&);

    /**
     *  This callback allows the resource cache to callback into the GrContext
     *  when the cache is still overbudget after a purge.
     */
    static bool OverbudgetCB(void* data);

    typedef SkRefCnt INHERITED;
};

#endif
