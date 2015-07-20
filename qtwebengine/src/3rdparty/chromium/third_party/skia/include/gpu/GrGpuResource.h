/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGpuResource_DEFINED
#define GrGpuResource_DEFINED

#include "GrResourceKey.h"
#include "GrTypesPriv.h"
#include "SkInstCnt.h"
#include "SkTInternalLList.h"

class GrContext;
class GrGpu;
class GrResourceCache2;
class GrResourceCacheEntry;

/**
 * Base class for GrGpuResource. Handles the various types of refs we need. Separated out as a base
 * class to isolate the ref-cnting behavior and provide friendship without exposing all of
 * GrGpuResource.
 * 
 * Gpu resources can have three types of refs:
 *   1) Normal ref (+ by ref(), - by unref()): These are used by code that is issuing draw calls
 *      that read and write the resource via GrDrawTarget and by any object that must own a
 *      GrGpuResource and is itself owned (directly or indirectly) by Skia-client code.
 *   2) Pending read (+ by addPendingRead(), - by completedRead()): GrContext has scheduled a read
 *      of the resource by the GPU as a result of a skia API call but hasn't executed it yet.
 *   3) Pending write (+ by addPendingWrite(), - by completedWrite()): GrContext has scheduled a
 *      write to the resource by the GPU as a result of a skia API call but hasn't executed it yet.
 *
 * The latter two ref types are private and intended only for Gr core code.
 *
 * When an item is purgable DERIVED:notifyIsPurgable() will be called (static poly morphism using
 * CRTP). GrIORef and GrGpuResource are separate classes for organizational reasons and to be
 * able to give access via friendship to only the functions related to pending IO operations.
 */
template <typename DERIVED> class GrIORef : public SkNoncopyable {
public:
    SK_DECLARE_INST_COUNT_ROOT(GrIORef)

    // Some of the signatures are written to mirror SkRefCnt so that GrGpuResource can work with
    // templated helper classes (e.g. SkAutoTUnref). However, we have different categories of
    // refs (e.g. pending reads). We also don't require thread safety as GrCacheable objects are
    // not intended to cross thread boundaries.
    void ref() const {
        this->validate();
        ++fRefCnt;
    }

    void unref() const {
        this->validate();
        --fRefCnt;
        this->didUnref();
    }

    bool isPurgable() const { return this->reffedOnlyByCache() && !this->internalHasPendingIO(); }
    bool reffedOnlyByCache() const { return 1 == fRefCnt; }

    void validate() const {
#ifdef SK_DEBUG
        SkASSERT(fRefCnt >= 0);
        SkASSERT(fPendingReads >= 0);
        SkASSERT(fPendingWrites >= 0);
        SkASSERT(fRefCnt + fPendingReads + fPendingWrites > 0);
#endif
    }

protected:
    GrIORef() : fRefCnt(1), fPendingReads(0), fPendingWrites(0) { }

    bool internalHasPendingRead() const { return SkToBool(fPendingReads); }
    bool internalHasPendingWrite() const { return SkToBool(fPendingWrites); }
    bool internalHasPendingIO() const { return SkToBool(fPendingWrites | fPendingReads); }

private:
    void addPendingRead() const {
        this->validate();
        ++fPendingReads;
    }

    void completedRead() const {
        this->validate();
        --fPendingReads;
        this->didUnref();
    }

    void addPendingWrite() const {
        this->validate();
        ++fPendingWrites;
    }

    void completedWrite() const {
        this->validate();
        --fPendingWrites;
        this->didUnref();
    }

private:
    void didUnref() const {
        if (0 == fPendingReads && 0 == fPendingWrites) {
            if (0 == fRefCnt) {
                // Must call derived destructor since this is not a virtual class.
                SkDELETE(static_cast<const DERIVED*>(this));
            } else if (1 == fRefCnt) {
                // The one ref is the cache's
                static_cast<const DERIVED*>(this)->notifyIsPurgable();
            }
        }
    }

    mutable int32_t fRefCnt;
    mutable int32_t fPendingReads;
    mutable int32_t fPendingWrites;

    // This class is used to manage conversion of refs to pending reads/writes.
    friend class GrGpuResourceRef;
    friend class GrResourceCache2; // to check IO ref counts.

    template <typename, GrIOType> friend class GrPendingIOResource;
};

/**
 * Base class for objects that can be kept in the GrResourceCache.
 */
class SK_API GrGpuResource : public GrIORef<GrGpuResource> {
public:
    SK_DECLARE_INST_COUNT(GrGpuResource)

    /**
     * Frees the object in the underlying 3D API. It must be safe to call this
     * when the object has been previously abandoned.
     */
    void release();

    /**
     * Removes references to objects in the underlying 3D API without freeing
     * them. Used when the API context has been torn down before the GrContext.
     */
    void abandon();

    /**
     * Tests whether a object has been abandoned or released. All objects will
     * be in this state after their creating GrContext is destroyed or has
     * contextLost called. It's up to the client to test wasDestroyed() before
     * attempting to use an object if it holds refs on objects across
     * ~GrContext, freeResources with the force flag, or contextLost.
     *
     * @return true if the object has been released or abandoned,
     *         false otherwise.
     */
    bool wasDestroyed() const { return NULL == fGpu; }

    /**
     * Retrieves the context that owns the object. Note that it is possible for
     * this to return NULL. When objects have been release()ed or abandon()ed
     * they no longer have an owning context. Destroying a GrContext
     * automatically releases all its resources.
     */
    const GrContext* getContext() const;
    GrContext* getContext();

    /**
     * Retrieves the amount of GPU memory used by this resource in bytes. It is
     * approximate since we aren't aware of additional padding or copies made
     * by the driver.
     *
     * @return the amount of GPU memory used in bytes
     */
    virtual size_t gpuMemorySize() const = 0;

    void setCacheEntry(GrResourceCacheEntry* cacheEntry) { fCacheEntry = cacheEntry; }
    GrResourceCacheEntry* getCacheEntry() const { return fCacheEntry; }
    bool isScratch() const;

    /** 
     * If this resource can be used as a scratch resource this returns a valid
     * scratch key. Otherwise it returns a key for which isNullScratch is true.
     */
    const GrResourceKey& getScratchKey() const { return fScratchKey; }

    /** 
     * If this resource is currently cached by its contents then this will return
     * the content key. Otherwise, NULL is returned.
     */
    const GrResourceKey* getContentKey() const;

    /**
     * Gets an id that is unique for this GrGpuResource object. It is static in that it does
     * not change when the content of the GrGpuResource object changes. This will never return
     * 0.
     */
    uint32_t getUniqueID() const { return fUniqueID; }

protected:
    // This must be called by every GrGpuObject. It should be called once the object is fully
    // initialized (i.e. not in a base class constructor).
    void registerWithCache();

    GrGpuResource(GrGpu*, bool isWrapped);
    virtual ~GrGpuResource();

    bool isInCache() const { return SkToBool(fCacheEntry); }

    GrGpu* getGpu() const { return fGpu; }

    // Derived classes should always call their parent class' onRelease
    // and onAbandon methods in their overrides.
    virtual void onRelease() {};
    virtual void onAbandon() {};

    bool isWrapped() const { return kWrapped_FlagBit & fFlags; }

    /**
     * This entry point should be called whenever gpuMemorySize() begins
     * reporting a different size. If the object is in the cache, it will call
     * gpuMemorySize() immediately and pass the new size on to the resource
     * cache.
     */
    void didChangeGpuMemorySize() const;

    /**
     * Optionally called by the GrGpuResource subclass if the resource can be used as scratch.
     * By default resources are not usable as scratch. This should only be called once.
     **/
    void setScratchKey(const GrResourceKey& scratchKey);

private:
    void notifyIsPurgable() const;

#ifdef SK_DEBUG
    friend class GrGpu; // for assert in GrGpu to access getGpu
#endif

    static uint32_t CreateUniqueID();

    // We're in an internal doubly linked list owned by GrResourceCache2
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(GrGpuResource);

    // This is not ref'ed but abandon() or release() will be called before the GrGpu object
    // is destroyed. Those calls set will this to NULL.
    GrGpu* fGpu;

    enum Flags {
        /**
         * This object wraps a GPU object given to us by the user.
         * Lifetime management is left up to the user (i.e., we will not
         * free it).
         */
        kWrapped_FlagBit         = 0x1,
    };

    uint32_t                fFlags;

    GrResourceCacheEntry*   fCacheEntry;  // NULL if not in cache
    const uint32_t          fUniqueID;

    GrResourceKey           fScratchKey;

    typedef GrIORef<GrGpuResource> INHERITED;
    friend class GrIORef<GrGpuResource>; // to access notifyIsPurgable.
};

#endif
