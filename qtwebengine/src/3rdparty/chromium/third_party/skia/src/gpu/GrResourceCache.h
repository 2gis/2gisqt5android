
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrResourceCache_DEFINED
#define GrResourceCache_DEFINED

#include "GrDrawTargetCaps.h"
#include "GrResourceKey.h"
#include "SkTMultiMap.h"
#include "SkMessageBus.h"
#include "SkTInternalLList.h"

class GrGpuResource;
class GrResourceCache;
class GrResourceCacheEntry;


// The cache listens for these messages to purge junk resources proactively.
struct GrResourceInvalidatedMessage {
    GrResourceKey key;
};

///////////////////////////////////////////////////////////////////////////////

class GrResourceCacheEntry {
public:
    GrGpuResource* resource() const { return fResource; }
    const GrResourceKey& key() const { return fKey; }

    static const GrResourceKey& GetKey(const GrResourceCacheEntry& e) { return e.key(); }
    static uint32_t Hash(const GrResourceKey& key) { return key.getHash(); }
#ifdef SK_DEBUG
    void validate() const;
#else
    void validate() const {}
#endif

    /**
     *  Update the cached size for this entry and inform the resource cache that
     *  it has changed. Usually invoked from GrGpuResource::didChangeGpuMemorySize,
     *  not directly from here.
     */
    void didChangeResourceSize();

private:
    GrResourceCacheEntry(GrResourceCache* resourceCache,
                         const GrResourceKey& key,
                         GrGpuResource* resource);
    ~GrResourceCacheEntry();

    GrResourceCache* fResourceCache;
    GrResourceKey    fKey;
    GrGpuResource*   fResource;
    size_t           fCachedSize;
    bool             fIsExclusive;

    // Linked list for the LRU ordering.
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(GrResourceCacheEntry);

    friend class GrResourceCache;
    friend class GrContext;
};

///////////////////////////////////////////////////////////////////////////////

/**
 *  Cache of GrGpuResource objects.
 *
 *  These have a corresponding GrResourceKey, built from 128bits identifying the
 *  resource. Multiple resources can map to same GrResourceKey.
 *
 *  The cache stores the entries in a double-linked list, which is its LRU.
 *  When an entry is "locked" (i.e. given to the caller), it is moved to the
 *  head of the list. If/when we must purge some of the entries, we walk the
 *  list backwards from the tail, since those are the least recently used.
 *
 *  For fast searches, we maintain a hash map based on the GrResourceKey.
 *
 *  It is a goal to make the GrResourceCache the central repository and bookkeeper
 *  of all resources. It should replace the linked list of GrGpuResources that
 *  GrGpu uses to call abandon/release.
 */
class GrResourceCache {
public:
    GrResourceCache(const GrDrawTargetCaps*, int maxCount, size_t maxBytes);
    ~GrResourceCache();

    /**
     *  Return the current resource cache limits.
     *
     *  @param maxResource If non-null, returns maximum number of resources
     *                     that can be held in the cache.
     *  @param maxBytes    If non-null, returns maximum number of bytes of
     *                     gpu memory that can be held in the cache.
     */
    void getLimits(int* maxResources, size_t* maxBytes) const;

    /**
     *  Specify the resource cache limits. If the current cache exceeds either
     *  of these, it will be purged (LRU) to keep the cache within these limits.
     *
     *  @param maxResources The maximum number of resources that can be held in
     *                      the cache.
     *  @param maxBytes     The maximum number of bytes of resource memory that
     *                      can be held in the cache.
     */
    void setLimits(int maxResources, size_t maxResourceBytes);

    /**
     *  The callback function used by the cache when it is still over budget
     *  after a purge. The passed in 'data' is the same 'data' handed to
     *  setOverbudgetCallback. The callback returns true if some resources
     *  have been freed.
     */
    typedef bool (*PFOverbudgetCB)(void* data);

    /**
     *  Set the callback the cache should use when it is still over budget
     *  after a purge. The 'data' provided here will be passed back to the
     *  callback. Note that the cache will attempt to purge any resources newly
     *  freed by the callback.
     */
    void setOverbudgetCallback(PFOverbudgetCB overbudgetCB, void* data) {
        fOverbudgetCB = overbudgetCB;
        fOverbudgetData = data;
    }

    /**
     * Returns the number of bytes consumed by cached resources.
     */
    size_t getCachedResourceBytes() const { return fEntryBytes; }

    /**
     * Returns the number of cached resources.
     */
    int getCachedResourceCount() const { return fEntryCount; }

    /**
     *  Search for an entry with the same Key. If found, return it.
     *  If not found, return null.
     */
    GrGpuResource* find(const GrResourceKey& key);

    void makeResourceMRU(GrGpuResource*);

    /** Called by GrGpuResources when they detects that they are newly purgable. */
    void notifyPurgable(const GrGpuResource*);

    /**
     *  Add the new resource to the cache (by creating a new cache entry based
     *  on the provided key and resource).
     *
     *  Ownership of the resource is transferred to the resource cache,
     *  which will unref() it when it is purged or deleted.
     */
    void addResource(const GrResourceKey& key, GrGpuResource* resource);

    /**
     * Determines if the cache contains an entry matching a key. If a matching
     * entry exists but was detached then it will not be found.
     */
    bool hasKey(const GrResourceKey& key) const { return SkToBool(fCache.find(key)); }

    /**
     * Notify the cache that the size of a resource has changed.
     */
    void didIncreaseResourceSize(const GrResourceCacheEntry*, size_t amountInc);
    void didDecreaseResourceSize(const GrResourceCacheEntry*, size_t amountDec);

    /**
     * Remove a resource from the cache and delete it!
     */
    void deleteResource(GrResourceCacheEntry* entry);

    /**
     * Removes every resource in the cache that isn't locked.
     */
    void purgeAllUnlocked();

    /**
     * Allow cache to purge unused resources to obey resource limitations
     * Note: this entry point will be hidden (again) once totally ref-driven
     * cache maintenance is implemented. Note that the overbudget callback
     * will be called if the initial purge doesn't get the cache under
     * its budget.
     *
     * extraCount and extraBytes are added to the current resource allocation
     * to make sure enough room is available for future additions (e.g,
     * 10MB across 10 textures is about to be added).
     */
    void purgeAsNeeded(int extraCount = 0, size_t extraBytes = 0);

#ifdef SK_DEBUG
    void validate() const;
#else
    void validate() const {}
#endif

#if GR_CACHE_STATS
    void printStats();
#endif

private:
    void internalDetach(GrResourceCacheEntry*);
    void attachToHead(GrResourceCacheEntry*);
    void purgeInvalidated();
    void internalPurge(int extraCount, size_t extraBytes);
#ifdef SK_DEBUG
    static size_t countBytes(const SkTInternalLList<GrResourceCacheEntry>& list);
#endif

    typedef SkTMultiMap<GrResourceCacheEntry, GrResourceKey> CacheMap;
    CacheMap                                fCache;

    // We're an internal doubly linked list
    typedef SkTInternalLList<GrResourceCacheEntry> EntryList;
    EntryList                               fList;

    // our budget, used in purgeAsNeeded()
    int                                     fMaxCount;
    size_t                                  fMaxBytes;

    // our current stats, related to our budget
#if GR_CACHE_STATS
    int                                     fHighWaterEntryCount;
    size_t                                  fHighWaterEntryBytes;
#endif

    int                                     fEntryCount;
    size_t                                  fEntryBytes;

    // prevents recursive purging
    bool                                    fPurging;

    PFOverbudgetCB                          fOverbudgetCB;
    void*                                   fOverbudgetData;

    SkAutoTUnref<const GrDrawTargetCaps>    fCaps;

    // Listen for messages that a resource has been invalidated and purge cached junk proactively.
   typedef  SkMessageBus<GrResourceInvalidatedMessage>::Inbox Inbox;
   Inbox                                    fInvalidationInbox;
};

///////////////////////////////////////////////////////////////////////////////

#ifdef SK_DEBUG
    class GrAutoResourceCacheValidate {
    public:
        GrAutoResourceCacheValidate(GrResourceCache* cache) : fCache(cache) {
            cache->validate();
        }
        ~GrAutoResourceCacheValidate() {
            fCache->validate();
        }
    private:
        GrResourceCache* fCache;
    };
#else
    class GrAutoResourceCacheValidate {
    public:
        GrAutoResourceCacheValidate(GrResourceCache*) {}
    };
#endif

#endif
