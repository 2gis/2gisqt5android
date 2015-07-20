/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrLayerCache_DEFINED
#define GrLayerCache_DEFINED

#include "GrAtlas.h"
#include "GrPictureUtils.h"
#include "GrRect.h"

#include "SkChecksum.h"
#include "SkMessageBus.h"
#include "SkTDynamicHash.h"

class SkPicture;

// Set to 0 to disable caching of hoisted layers
#define GR_CACHE_HOISTED_LAYERS 0

// The layer cache listens for these messages to purge picture-related resources.
struct GrPictureDeletedMessage {
    uint32_t pictureID;
};

// GrPictureInfo stores the atlas plots used by a single picture. A single 
// plot may be used to store layers from multiple pictures.
struct GrPictureInfo {
public:
    static const int kNumPlots = 4;

    // for SkTDynamicHash - just use the pictureID as the hash key
    static const uint32_t& GetKey(const GrPictureInfo& pictInfo) { return pictInfo.fPictureID; }
    static uint32_t Hash(const uint32_t& key) { return SkChecksum::Mix(key); }

    // GrPictureInfo proper
    GrPictureInfo(uint32_t pictureID) : fPictureID(pictureID) { 
#if !GR_CACHE_HOISTED_LAYERS
        memset(fPlotUses, 0, sizeof(fPlotUses)); 
#endif
    }

#if !GR_CACHE_HOISTED_LAYERS
    void incPlotUsage(int plotID) {
        SkASSERT(plotID < kNumPlots);
        fPlotUses[plotID]++;
    }

    void decPlotUsage(int plotID) {
        SkASSERT(plotID < kNumPlots);
        SkASSERT(fPlotUses[plotID] > 0);
        fPlotUses[plotID]--;
    }

    int plotUsage(int plotID) const { 
        SkASSERT(plotID < kNumPlots);
        return fPlotUses[plotID];
    }
#endif

    const uint32_t fPictureID;
    GrAtlas::ClientPlotUsage  fPlotUsage;

#if !GR_CACHE_HOISTED_LAYERS
private:
    int fPlotUses[kNumPlots];
#endif
};

// GrCachedLayer encapsulates the caching information for a single saveLayer.
//
// Atlased layers get a ref to the backing GrTexture while non-atlased layers
// get a ref to the GrTexture in which they reside. In both cases 'fRect' 
// contains the layer's extent in its texture.
// Atlased layers also get a pointer to the plot in which they reside.
// For non-atlased layers, the lock field just corresponds to locking in
// the resource cache. For atlased layers, it implements an additional level
// of locking to allow atlased layers to be reused multiple times.
struct GrCachedLayer {
public:
    // For SkTDynamicHash
    struct Key {
        Key(uint32_t pictureID, int start, const SkIRect& bounds, const SkMatrix& ctm)
        : fPictureID(pictureID)
        , fStart(start)
        , fBounds(bounds)
        , fCTM(ctm) {
            fCTM.getType(); // force initialization of type so hashes match

            // Key needs to be tightly packed.
            GR_STATIC_ASSERT(sizeof(Key) == sizeof(uint32_t) +      // picture ID
                                            sizeof(int) +           // start index
                                            4 * sizeof(uint32_t) +  // bounds
                                            9 * sizeof(SkScalar) + sizeof(uint32_t)); // matrix
        }

        bool operator==(const Key& other) const {
            return fPictureID == other.fPictureID &&
                   fStart == other.fStart &&
                   fBounds == other.fBounds &&
                   fCTM.cheapEqualTo(other.fCTM);
        }

        uint32_t pictureID() const { return fPictureID; }
        int start() const { return fStart; }
        const SkIRect& bound() const { return fBounds; }

    private:
        // ID of the picture of which this layer is a part
        const uint32_t fPictureID;
        // The the index of the saveLayer command in the picture
        const int      fStart;
        // The bounds of the layer. The TL corner is its offset.
        const SkIRect  fBounds;
        // The 2x2 portion of the CTM applied to this layer in the picture
        SkMatrix       fCTM;
    };

    static const Key& GetKey(const GrCachedLayer& layer) { return layer.fKey; }
    static uint32_t Hash(const Key& key) { 
        return SkChecksum::Murmur3(reinterpret_cast<const uint32_t*>(&key), sizeof(Key));
    }

    // GrCachedLayer proper
    GrCachedLayer(uint32_t pictureID, int start, int stop,
                  const SkIRect& bounds, const SkMatrix& ctm,
                  const SkPaint* paint)
        : fKey(pictureID, start, bounds, ctm)
        , fStop(stop)
        , fPaint(paint ? SkNEW_ARGS(SkPaint, (*paint)) : NULL)
        , fTexture(NULL)
        , fRect(GrIRect16::MakeEmpty())
        , fPlot(NULL)
        , fUses(0)
        , fLocked(false) {
        SkASSERT(SK_InvalidGenID != pictureID && start >= 0 && stop >= 0);
    }

    ~GrCachedLayer() {
        SkSafeUnref(fTexture);
        SkDELETE(fPaint);
    }

    uint32_t pictureID() const { return fKey.pictureID(); }
    int start() const { return fKey.start(); }
    const SkIRect& bound() const { return fKey.bound(); }

    int stop() const { return fStop; }
    void setTexture(GrTexture* texture, const GrIRect16& rect) {
        SkRefCnt_SafeAssign(fTexture, texture);
        fRect = rect;
    }
    GrTexture* texture() { return fTexture; }
    const SkPaint* paint() const { return fPaint; }
    const GrIRect16& rect() const { return fRect; }

    void setPlot(GrPlot* plot) {
        SkASSERT(NULL == plot || NULL == fPlot);
        fPlot = plot;
    }
    GrPlot* plot() { return fPlot; }

    bool isAtlased() const { return SkToBool(fPlot); }

    void setLocked(bool locked) { fLocked = locked; }
    bool locked() const { return fLocked; }

    SkDEBUGCODE(const GrPlot* plot() const { return fPlot; })
    SkDEBUGCODE(void validate(const GrTexture* backingTexture) const;)

private:
    const Key       fKey;

    // The final "restore" operation index of the cached layer
    const int       fStop;

    // The paint used when dropping the layer down into the owning canvas.
    // Can be NULL. This class makes a copy for itself.
    const SkPaint*  fPaint;

    // fTexture is a ref on the atlasing texture for atlased layers and a
    // ref on a GrTexture for non-atlased textures.
    GrTexture*      fTexture;

    // For both atlased and non-atlased layers 'fRect' contains the  bound of
    // the layer in whichever texture it resides. It is empty when 'fTexture'
    // is NULL.
    GrIRect16       fRect;

    // For atlased layers, fPlot stores the atlas plot in which the layer rests.
    // It is always NULL for non-atlased layers.
    GrPlot*         fPlot;

    // The number of actively hoisted layers using this cached image (e.g.,
    // extant GrHoistedLayers pointing at this object). This object will
    // be unlocked when the use count reaches 0.
    int             fUses;

    // For non-atlased layers 'fLocked' should always match "fTexture".
    // (i.e., if there is a texture it is locked).
    // For atlased layers, 'fLocked' is true if the layer is in a plot and
    // actively required for rendering. If the layer is in a plot but not
    // actively required for rendering, then 'fLocked' is false. If the
    // layer isn't in a plot then is can never be locked.
    bool            fLocked;

    void addUse()     { ++fUses; }
    void removeUse()  { SkASSERT(fUses > 0); --fUses; }
    int uses() const { return fUses; }

    friend class GrLayerCache;  // for access to usage methods
    friend class TestingAccess; // for testing
};

// The GrLayerCache caches pre-computed saveLayers for later rendering.
// Non-atlased layers are stored in their own GrTexture while the atlased
// layers share a single GrTexture.
// Unlike the GrFontCache, the GrTexture atlas only has one GrAtlas (for 8888)
// and one GrPlot (for the entire atlas). As such, the GrLayerCache
// roughly combines the functionality of the GrFontCache and GrTextStrike
// classes.
class GrLayerCache {
public:
    GrLayerCache(GrContext*);
    ~GrLayerCache();

    // As a cache, the GrLayerCache can be ordered to free up all its cached
    // elements by the GrContext
    void freeAll();

    GrCachedLayer* findLayer(uint32_t pictureID, int start, 
                             const SkIRect& bounds, const SkMatrix& ctm);
    GrCachedLayer* findLayerOrCreate(uint32_t pictureID,
                                     int start, int stop, 
                                     const SkIRect& bounds,
                                     const SkMatrix& ctm,
                                     const SkPaint* paint);

    // Attempt to place 'layer' in the atlas. Return true on success; false on failure.
    // When true is returned, 'needsRendering' will indicate if the layer must be (re)drawn.
    // Additionally, the GPU resources will be locked.
    bool tryToAtlas(GrCachedLayer* layer, const GrSurfaceDesc& desc, bool* needsRendering);

    // Attempt to lock the GPU resources required for a layer. Return true on success;
    // false on failure. When true is returned 'needsRendering' will indicate if the
    // layer must be (re)drawn.
    // Note that atlased layers should already have been locked and rendered so only
    // free floating layers will have 'needsRendering' set.
    // Currently, this path always uses a new scratch texture for non-Atlased layers
    // and (thus) doesn't cache anything. This can yield a lot of re-rendering.
    // TODO: allow rediscovery of free-floating layers that are still in the resource cache.
    bool lock(GrCachedLayer* layer, const GrSurfaceDesc& desc, bool* needsRendering);

    // addUse is just here to keep the API symmetric
    void addUse(GrCachedLayer* layer) { layer->addUse(); }
    void removeUse(GrCachedLayer* layer) {
        layer->removeUse();
        if (layer->uses() == 0) {
            // If no one cares about the layer allow it to be recycled.
            this->unlock(layer);
        }
    }

    // Setup to be notified when 'picture' is deleted
    void trackPicture(const SkPicture* picture);

    // Cleanup after any SkPicture deletions
    void processDeletedPictures();

    SkDEBUGCODE(void validate() const;)

#ifdef SK_DEVELOPER
    void writeLayersToDisk(const SkString& dirName);
#endif

    static bool PlausiblyAtlasable(int width, int height) {
        return width <= kPlotWidth && height <= kPlotHeight;
    }

#if !GR_CACHE_HOISTED_LAYERS
    void purgeAll();
#endif

private:
    static const int kAtlasTextureWidth = 1024;
    static const int kAtlasTextureHeight = 1024;

    static const int kNumPlotsX = 2;
    static const int kNumPlotsY = 2;

    static const int kPlotWidth = kAtlasTextureWidth / kNumPlotsX;
    static const int kPlotHeight = kAtlasTextureHeight / kNumPlotsY;

    GrContext*                fContext;  // pointer back to owning context
    SkAutoTDelete<GrAtlas>    fAtlas;    // TODO: could lazily allocate

    // We cache this information here (rather then, say, on the owning picture)
    // because we want to be able to clean it up as needed (e.g., if a picture
    // is leaked and never cleans itself up we still want to be able to 
    // remove the GrPictureInfo once its layers are purged from all the atlas
    // plots).
    SkTDynamicHash<GrPictureInfo, uint32_t> fPictureHash;

    SkTDynamicHash<GrCachedLayer, GrCachedLayer::Key> fLayerHash;

    SkMessageBus<GrPictureDeletedMessage>::Inbox fPictDeletionInbox;

    SkAutoTUnref<SkPicture::DeletionListener> fDeletionListener;

    // This implements a plot-centric locking mechanism (since the atlas
    // backing texture is always locked). Each layer that is locked (i.e.,
    // needed for the current rendering) in a plot increments the plot lock
    // count for that plot. Similarly, once a rendering is complete all the
    // layers used in it decrement the lock count for the used plots.
    // Plots with a 0 lock count are open for recycling/purging.
    int fPlotLocks[kNumPlotsX * kNumPlotsY];

    // Inform the cache that layer's cached image is not currently required
    void unlock(GrCachedLayer* layer);

    void initAtlas();
    GrCachedLayer* createLayer(uint32_t pictureID, int start, int stop, 
                               const SkIRect& bounds, const SkMatrix& ctm, 
                               const SkPaint* paint);

    // Remove all the layers (and unlock any resources) associated with 'pictureID'
    void purge(uint32_t pictureID);

    void purgePlot(GrPlot* plot);

    // Try to find a purgeable plot and clear it out. Return true if a plot
    // was purged; false otherwise.
    bool purgePlot();

    void incPlotLock(int plotIdx) { ++fPlotLocks[plotIdx]; }
    void decPlotLock(int plotIdx) {
        SkASSERT(fPlotLocks[plotIdx] > 0);
        --fPlotLocks[plotIdx];
    }

    // for testing
    friend class TestingAccess;
    int numLayers() const { return fLayerHash.count(); }
};

#endif
