/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PictureRenderer_DEFINED
#define PictureRenderer_DEFINED

#include "SkCanvas.h"
#include "SkCountdown.h"
#include "SkDrawFilter.h"
#include "SkMath.h"
#include "SkPaint.h"
#include "SkPicture.h"
#include "SkPictureRecorder.h"
#include "SkRect.h"
#include "SkRefCnt.h"
#include "SkRunnable.h"
#include "SkString.h"
#include "SkTDArray.h"
#include "SkThreadPool.h"
#include "SkTypes.h"

#if SK_SUPPORT_GPU
#include "GrContextFactory.h"
#include "GrContext.h"
#endif

#include "image_expectations.h"

class SkBitmap;
class SkCanvas;
class SkGLContextHelper;
class SkThread;

namespace sk_tools {

class TiledPictureRenderer;

class PictureRenderer : public SkRefCnt {

public:
    enum SkDeviceTypes {
#if SK_ANGLE
        kAngle_DeviceType,
#endif
#if SK_MESA
        kMesa_DeviceType,
#endif
        kBitmap_DeviceType,
#if SK_SUPPORT_GPU
        kGPU_DeviceType,
        kNVPR_DeviceType,
#endif
    };

    enum BBoxHierarchyType {
        kNone_BBoxHierarchyType = 0,
        kQuadTree_BBoxHierarchyType,
        kRTree_BBoxHierarchyType,
        kTileGrid_BBoxHierarchyType,

        kLast_BBoxHierarchyType = kTileGrid_BBoxHierarchyType,
    };

    // this uses SkPaint::Flags as a base and adds additional flags
    enum DrawFilterFlags {
        kNone_DrawFilterFlag = 0,
        kHinting_DrawFilterFlag = 0x10000, // toggles between no hinting and normal hinting
        kSlightHinting_DrawFilterFlag = 0x20000, // toggles between slight and normal hinting
        kAAClip_DrawFilterFlag = 0x40000, // toggles between soft and hard clip
        kMaskFilter_DrawFilterFlag = 0x80000, // toggles on/off mask filters (e.g., blurs)
    };

    SK_COMPILE_ASSERT(!(kMaskFilter_DrawFilterFlag & SkPaint::kAllFlags), maskfilter_flag_must_be_greater);
    SK_COMPILE_ASSERT(!(kHinting_DrawFilterFlag & SkPaint::kAllFlags),
            hinting_flag_must_be_greater);
    SK_COMPILE_ASSERT(!(kSlightHinting_DrawFilterFlag & SkPaint::kAllFlags),
            slight_hinting_flag_must_be_greater);

    /**
     * Called with each new SkPicture to render.
     *
     * @param pict The SkPicture to render.
     * @param writePath The output directory within which this renderer should write all images,
     *     or NULL if this renderer should not write all images.
     * @param mismatchPath The output directory within which this renderer should write any images
     *     which do not match expectations, or NULL if this renderer should not write mismatches.
     * @param inputFilename The name of the input file we are rendering.
     * @param useChecksumBasedFilenames Whether to use checksum-based filenames when writing
     *     bitmap images to disk.
     */
    virtual void init(SkPicture* pict, const SkString* writePath, const SkString* mismatchPath,
                      const SkString* inputFilename, bool useChecksumBasedFilenames);

    /**
     * TODO(epoger): Temporary hack, while we work on http://skbug.com/2584 ('bench_pictures is
     * timing reading pixels and writing json files'), such that:
     * - render_pictures can call this method and continue to work
     * - any other callers (bench_pictures) will skip calls to write() by default
     */
    void enableWrites() { fEnableWrites = true; }

    /**
     *  Set the viewport so that only the portion listed gets drawn.
     */
    void setViewport(SkISize size) { fViewport = size; }

    /**
     *  Set the scale factor at which draw the picture.
     */
    void setScaleFactor(SkScalar scale) { fScaleFactor = scale; }

    /**
     * Perform any setup that should done prior to each iteration of render() which should not be
     * timed.
     */
    virtual void setup() {}

    /**
     * Perform the work.  If this is being called within the context of bench_pictures,
     * this is the step that will be timed.
     *
     * Typically "the work" is rendering an SkPicture into a bitmap, but in some subclasses
     * it is recording the source SkPicture into another SkPicture.
     *
     * If fWritePath has been specified, the result of the work will be written to that dir.
     * If fMismatchPath has been specified, and the actual image result differs from its
     * expectation, the result of the work will be written to that dir.
     *
     * @param out If non-null, the implementing subclass MAY allocate an SkBitmap, copy the
     *            output image into it, and return it here.  (Some subclasses ignore this parameter)
     * @return bool True if rendering succeeded and, if fWritePath had been specified, the output
     *              was successfully written to a file.
     */
    virtual bool render(SkBitmap** out = NULL) = 0;

    /**
     * Called once finished with a particular SkPicture, before calling init again, and before
     * being done with this Renderer.
     */
    virtual void end();

    /**
     * If this PictureRenderer is actually a TiledPictureRender, return a pointer to this as a
     * TiledPictureRender so its methods can be called.
     */
    virtual TiledPictureRenderer* getTiledRenderer() { return NULL; }

    /**
     * Resets the GPU's state. Does nothing if the backing is raster. For a GPU renderer, calls
     * flush, swapBuffers and, if callFinish is true, finish.
     * @param callFinish Whether to call finish.
     */
    void resetState(bool callFinish);

    /**
     * Remove all decoded textures from the CPU caches and all uploaded textures
     * from the GPU.
     */
    void purgeTextures();

    /**
     * Set the backend type. Returns true on success and false on failure.
     */
    bool setDeviceType(SkDeviceTypes deviceType) {
        fDeviceType = deviceType;
#if SK_SUPPORT_GPU
        // In case this function is called more than once
        SkSafeUnref(fGrContext);
        fGrContext = NULL;
        // Set to Native so it will have an initial value.
        GrContextFactory::GLContextType glContextType = GrContextFactory::kNative_GLContextType;
#endif
        switch(deviceType) {
            case kBitmap_DeviceType:
                return true;
#if SK_SUPPORT_GPU
            case kGPU_DeviceType:
                // Already set to GrContextFactory::kNative_GLContextType, above.
                break;
            case kNVPR_DeviceType:
                glContextType = GrContextFactory::kNVPR_GLContextType;
                break;
#if SK_ANGLE
            case kAngle_DeviceType:
                glContextType = GrContextFactory::kANGLE_GLContextType;
                break;
#endif
#if SK_MESA
            case kMesa_DeviceType:
                glContextType = GrContextFactory::kMESA_GLContextType;
                break;
#endif
#endif
            default:
                // Invalid device type.
                return false;
        }
#if SK_SUPPORT_GPU
        fGrContext = fGrContextFactory.get(glContextType);
        if (NULL == fGrContext) {
            return false;
        } else {
            fGrContext->ref();
            return true;
        }
#endif
    }

#if SK_SUPPORT_GPU
    void setSampleCount(int sampleCount) {
        fSampleCount = sampleCount;
    }
#endif

    void setDrawFilters(DrawFilterFlags const * const filters, const SkString& configName) {
        memcpy(fDrawFilters, filters, sizeof(fDrawFilters));
        fDrawFiltersConfig = configName;
    }

    void setBBoxHierarchyType(BBoxHierarchyType bbhType) {
        fBBoxHierarchyType = bbhType;
    }

    BBoxHierarchyType getBBoxHierarchyType() { return fBBoxHierarchyType; }

    void setGridSize(int width, int height) {
        fGridInfo.fTileInterval.set(width, height);
    }

    void setJsonSummaryPtr(ImageResultsAndExpectations* jsonSummaryPtr) {
        fJsonSummaryPtr = jsonSummaryPtr;
    }

    bool isUsingBitmapDevice() {
        return kBitmap_DeviceType == fDeviceType;
    }

    virtual SkString getPerIterTimeFormat() { return SkString("%.2f"); }

    virtual SkString getNormalTimeFormat() { return SkString("%6.2f"); }

    /**
     * Reports the configuration of this PictureRenderer.
     */
    SkString getConfigName() {
        SkString config = this->getConfigNameInternal();
        if (!fViewport.isEmpty()) {
            config.appendf("_viewport_%ix%i", fViewport.width(), fViewport.height());
        }
        if (fScaleFactor != SK_Scalar1) {
            config.appendf("_scalar_%f", SkScalarToFloat(fScaleFactor));
        }
        if (kRTree_BBoxHierarchyType == fBBoxHierarchyType) {
            config.append("_rtree");
        } else if (kQuadTree_BBoxHierarchyType == fBBoxHierarchyType) {
            config.append("_quadtree");
        } else if (kTileGrid_BBoxHierarchyType == fBBoxHierarchyType) {
            config.append("_grid");
            config.append("_");
            config.appendS32(fGridInfo.fTileInterval.width());
            config.append("x");
            config.appendS32(fGridInfo.fTileInterval.height());
        }
#if SK_SUPPORT_GPU
        switch (fDeviceType) {
            case kGPU_DeviceType:
                if (fSampleCount) {
                    config.appendf("_msaa%d", fSampleCount);
                } else {
                    config.append("_gpu");
                }
                break;
            case kNVPR_DeviceType:
                config.appendf("_nvprmsaa%d", fSampleCount);
                break;
#if SK_ANGLE
            case kAngle_DeviceType:
                config.append("_angle");
                break;
#endif
#if SK_MESA
            case kMesa_DeviceType:
                config.append("_mesa");
                break;
#endif
            default:
                // Assume that no extra info means bitmap.
                break;
        }
#endif
        config.append(fDrawFiltersConfig.c_str());
        return config;
    }

#if SK_SUPPORT_GPU
    bool isUsingGpuDevice() {
        switch (fDeviceType) {
            case kGPU_DeviceType:
            case kNVPR_DeviceType:
                // fall through
#if SK_ANGLE
            case kAngle_DeviceType:
                // fall through
#endif
#if SK_MESA
            case kMesa_DeviceType:
#endif
                return true;
            default:
                return false;
        }
    }

    SkGLContextHelper* getGLContext() {
        GrContextFactory::GLContextType glContextType
                = GrContextFactory::kNull_GLContextType;
        switch(fDeviceType) {
            case kGPU_DeviceType:
                glContextType = GrContextFactory::kNative_GLContextType;
                break;
            case kNVPR_DeviceType:
                glContextType = GrContextFactory::kNVPR_GLContextType;
                break;
#if SK_ANGLE
            case kAngle_DeviceType:
                glContextType = GrContextFactory::kANGLE_GLContextType;
                break;
#endif
#if SK_MESA
            case kMesa_DeviceType:
                glContextType = GrContextFactory::kMESA_GLContextType;
                break;
#endif
            default:
                return NULL;
        }
        return fGrContextFactory.getGLContext(glContextType);
    }

    GrContext* getGrContext() {
        return fGrContext;
    }
#endif

    SkCanvas* getCanvas() {
        return fCanvas;
    }

    SkPicture* getPicture() {
        return fPicture;
    }

    PictureRenderer()
        : fJsonSummaryPtr(NULL)
        , fDeviceType(kBitmap_DeviceType)
        , fEnableWrites(false)
        , fBBoxHierarchyType(kNone_BBoxHierarchyType)
        , fScaleFactor(SK_Scalar1)
#if SK_SUPPORT_GPU
        , fGrContext(NULL)
        , fSampleCount(0)
#endif
        {
            fGridInfo.fMargin.setEmpty();
            fGridInfo.fOffset.setZero();
            fGridInfo.fTileInterval.set(1, 1);
            sk_bzero(fDrawFilters, sizeof(fDrawFilters));
            fViewport.set(0, 0);
        }

#if SK_SUPPORT_GPU
    virtual ~PictureRenderer() {
        SkSafeUnref(fGrContext);
    }
#endif

protected:
    SkAutoTUnref<SkCanvas> fCanvas;
    SkAutoTUnref<SkPicture> fPicture;
    bool                   fUseChecksumBasedFilenames;
    ImageResultsAndExpectations*   fJsonSummaryPtr;
    SkDeviceTypes          fDeviceType;
    bool                   fEnableWrites;
    BBoxHierarchyType      fBBoxHierarchyType;
    DrawFilterFlags        fDrawFilters[SkDrawFilter::kTypeCount];
    SkString               fDrawFiltersConfig;
    SkString               fWritePath;
    SkString               fMismatchPath;
    SkString               fInputFilename;
    SkTileGridFactory::TileGridInfo fGridInfo; // used when fBBoxHierarchyType is TileGrid

    void buildBBoxHierarchy();

    /**
     * Return the total width that should be drawn. If the viewport width has been set greater than
     * 0, this will be the minimum of the current SkPicture's width and the viewport's width.
     */
    int getViewWidth();

    /**
     * Return the total height that should be drawn. If the viewport height has been set greater
     * than 0, this will be the minimum of the current SkPicture's height and the viewport's height.
     */
    int getViewHeight();

    /**
     * Scales the provided canvas to the scale factor set by setScaleFactor.
     */
    void scaleToScaleFactor(SkCanvas*);

    SkBBHFactory* getFactory();
    uint32_t recordFlags() const { return 0; }
    SkCanvas* setupCanvas();
    virtual SkCanvas* setupCanvas(int width, int height);

    /**
     * Copy src to dest; if src==NULL, set dest to empty string.
     */
    static void CopyString(SkString* dest, const SkString* src);

private:
    SkISize                fViewport;
    SkScalar               fScaleFactor;
#if SK_SUPPORT_GPU
    GrContextFactory       fGrContextFactory;
    GrContext*             fGrContext;
    int                    fSampleCount;
#endif

    virtual SkString getConfigNameInternal() = 0;

    typedef SkRefCnt INHERITED;
};

/**
 * This class does not do any rendering, but its render function executes recording, which we want
 * to time.
 */
class RecordPictureRenderer : public PictureRenderer {
    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

    virtual SkString getPerIterTimeFormat() SK_OVERRIDE { return SkString("%.4f"); }

    virtual SkString getNormalTimeFormat() SK_OVERRIDE { return SkString("%6.4f"); }

protected:
    virtual SkCanvas* setupCanvas(int width, int height) SK_OVERRIDE;

private:
    virtual SkString getConfigNameInternal() SK_OVERRIDE;
};

class PipePictureRenderer : public PictureRenderer {
public:
    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

private:
    virtual SkString getConfigNameInternal() SK_OVERRIDE;

    typedef PictureRenderer INHERITED;
};

class SimplePictureRenderer : public PictureRenderer {
public:
    virtual void init(SkPicture* pict, const SkString* writePath, const SkString* mismatchPath,
                      const SkString* inputFilename, bool useChecksumBasedFilenames) SK_OVERRIDE;

    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

private:
    virtual SkString getConfigNameInternal() SK_OVERRIDE;

    typedef PictureRenderer INHERITED;
};

class TiledPictureRenderer : public PictureRenderer {
public:
    TiledPictureRenderer();

    virtual void init(SkPicture* pict, const SkString* writePath, const SkString* mismatchPath,
                      const SkString* inputFilename, bool useChecksumBasedFilenames) SK_OVERRIDE;

    /**
     * Renders to tiles, rather than a single canvas.
     * If fWritePath was provided, a separate file is
     * created for each tile, named "path0.png", "path1.png", etc.
     * Multithreaded mode currently does not support writing to a file.
     */
    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

    virtual void end() SK_OVERRIDE;

    void setTileWidth(int width) {
        fTileWidth = width;
    }

    int getTileWidth() const {
        return fTileWidth;
    }

    void setTileHeight(int height) {
        fTileHeight = height;
    }

    int getTileHeight() const {
        return fTileHeight;
    }

    void setTileWidthPercentage(double percentage) {
        fTileWidthPercentage = percentage;
    }

    double getTileWidthPercentage() const {
        return fTileWidthPercentage;
    }

    void setTileHeightPercentage(double percentage) {
        fTileHeightPercentage = percentage;
    }

    double getTileHeightPercentage() const {
        return fTileHeightPercentage;
    }

    void setTileMinPowerOf2Width(int width) {
        SkASSERT(SkIsPow2(width) && width > 0);
        if (!SkIsPow2(width) || width <= 0) {
            return;
        }

        fTileMinPowerOf2Width = width;
    }

    int getTileMinPowerOf2Width() const {
        return fTileMinPowerOf2Width;
    }

    virtual TiledPictureRenderer* getTiledRenderer() SK_OVERRIDE { return this; }

    virtual bool supportsTimingIndividualTiles() { return true; }

    /**
     * Report the number of tiles in the x and y directions. Must not be called before init.
     * @param x Output parameter identifying the number of tiles in the x direction.
     * @param y Output parameter identifying the number of tiles in the y direction.
     * @return True if the tiles have been set up, and x and y are meaningful. If false, x and y are
     *         unmodified.
     */
    bool tileDimensions(int& x, int&y);

    /**
     * Move to the next tile and return its indices. Must be called before calling drawCurrentTile
     * for the first time.
     * @param i Output parameter identifying the column of the next tile to be drawn on the next
     *          call to drawNextTile.
     * @param j Output parameter identifying the row  of the next tile to be drawn on the next call
     *          to drawNextTile.
     * @param True if the tiles have been created and the next tile to be drawn by drawCurrentTile
     *        is within the range of tiles. If false, i and j are unmodified.
     */
    bool nextTile(int& i, int& j);

    /**
     * Render one tile. This will draw the same tile each time it is called until nextTile is
     * called. The tile rendered will depend on how many calls have been made to nextTile.
     * It is an error to call this without first calling nextTile, or if nextTile returns false.
     */
    void drawCurrentTile();

protected:
    SkTDArray<SkRect> fTileRects;

    virtual SkCanvas* setupCanvas(int width, int height) SK_OVERRIDE;
    virtual SkString getConfigNameInternal() SK_OVERRIDE;

private:
    int    fTileWidth;
    int    fTileHeight;
    double fTileWidthPercentage;
    double fTileHeightPercentage;
    int    fTileMinPowerOf2Width;

    // These variables are only used for timing individual tiles.
    // Next tile to draw in fTileRects.
    int    fCurrentTileOffset;
    // Number of tiles in the x direction.
    int    fTilesX;
    // Number of tiles in the y direction.
    int    fTilesY;

    void setupTiles();
    void setupPowerOf2Tiles();

    typedef PictureRenderer INHERITED;
};

class CloneData;

class MultiCorePictureRenderer : public TiledPictureRenderer {
public:
    explicit MultiCorePictureRenderer(int threadCount);

    ~MultiCorePictureRenderer();

    virtual void init(SkPicture* pict, const SkString* writePath, const SkString* mismatchPath,
                      const SkString* inputFilename, bool useChecksumBasedFilenames) SK_OVERRIDE;

    /**
     * Behaves like TiledPictureRenderer::render(), only using multiple threads.
     */
    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

    virtual void end() SK_OVERRIDE;

    virtual bool supportsTimingIndividualTiles() SK_OVERRIDE { return false; }

private:
    virtual SkString getConfigNameInternal() SK_OVERRIDE;

    const int            fNumThreads;
    SkTDArray<SkCanvas*> fCanvasPool;
    SkThreadPool         fThreadPool;
    SkPicture*           fPictureClones;
    CloneData**          fCloneData;
    SkCountdown          fCountdown;

    typedef TiledPictureRenderer INHERITED;
};

/**
 * This class does not do any rendering, but its render function executes turning an SkPictureRecord
 * into an SkPicturePlayback, which we want to time.
 */
class PlaybackCreationRenderer : public PictureRenderer {
public:
    virtual void setup() SK_OVERRIDE;

    virtual bool render(SkBitmap** out = NULL) SK_OVERRIDE;

    virtual SkString getPerIterTimeFormat() SK_OVERRIDE { return SkString("%.4f"); }

    virtual SkString getNormalTimeFormat() SK_OVERRIDE { return SkString("%6.4f"); }

private:
    SkAutoTDelete<SkPictureRecorder> fRecorder;

    virtual SkString getConfigNameInternal() SK_OVERRIDE;

    typedef PictureRenderer INHERITED;
};

extern PictureRenderer* CreateGatherPixelRefsRenderer();
extern PictureRenderer* CreatePictureCloneRenderer();

}

#endif  // PictureRenderer_DEFINED
