
/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrResourceCache.h"
#include "GrGpuResource.h"
#include "GrTexturePriv.h"

DECLARE_SKMESSAGEBUS_MESSAGE(GrResourceInvalidatedMessage);

///////////////////////////////////////////////////////////////////////////////

void GrGpuResource::didChangeGpuMemorySize() const {
    if (this->isInCache()) {
        fCacheEntry->didChangeResourceSize();
    }
}

///////////////////////////////////////////////////////////////////////////////

GrResourceKey::ResourceType GrResourceKey::GenerateResourceType() {
    static int32_t gNextType = 0;

    int32_t type = sk_atomic_inc(&gNextType);
    if (type >= (1 << 8 * sizeof(ResourceType))) {
        SkFAIL("Too many Resource Types");
    }

    return static_cast<ResourceType>(type);
}

///////////////////////////////////////////////////////////////////////////////

GrResourceCacheEntry::GrResourceCacheEntry(GrResourceCache* resourceCache,
                                           const GrResourceKey& key,
                                           GrGpuResource* resource)
        : fResourceCache(resourceCache),
          fKey(key),
          fResource(resource),
          fCachedSize(resource->gpuMemorySize()),
          fIsExclusive(false) {
    // we assume ownership of the resource, and will unref it when we die
    SkASSERT(resource);
    resource->ref();
}

GrResourceCacheEntry::~GrResourceCacheEntry() {
    fResource->setCacheEntry(NULL);
    fResource->unref();
}

#ifdef SK_DEBUG
void GrResourceCacheEntry::validate() const {
    SkASSERT(fResourceCache);
    SkASSERT(fResource);
    SkASSERT(fResource->getCacheEntry() == this);
    SkASSERT(fResource->gpuMemorySize() == fCachedSize);
    fResource->validate();
}
#endif

void GrResourceCacheEntry::didChangeResourceSize() {
    size_t oldSize = fCachedSize;
    fCachedSize = fResource->gpuMemorySize();
    if (fCachedSize > oldSize) {
        fResourceCache->didIncreaseResourceSize(this, fCachedSize - oldSize);
    } else if (fCachedSize < oldSize) {
        fResourceCache->didDecreaseResourceSize(this, oldSize - fCachedSize);
    }
}

///////////////////////////////////////////////////////////////////////////////

GrResourceCache::GrResourceCache(const GrDrawTargetCaps* caps, int maxCount, size_t maxBytes)
    : fMaxCount(maxCount)
    , fMaxBytes(maxBytes)
    , fCaps(SkRef(caps)) {
#if GR_CACHE_STATS
    fHighWaterEntryCount          = 0;
    fHighWaterEntryBytes          = 0;
#endif

    fEntryCount                   = 0;
    fEntryBytes                   = 0;

    fPurging                      = false;

    fOverbudgetCB                 = NULL;
    fOverbudgetData               = NULL;
}

GrResourceCache::~GrResourceCache() {
    GrAutoResourceCacheValidate atcv(this);

    EntryList::Iter iter;

    // Unlike the removeAll, here we really remove everything, including locked resources.
    while (GrResourceCacheEntry* entry = fList.head()) {
        GrAutoResourceCacheValidate atcv(this);

        // remove from our cache
        fCache.remove(entry->fKey, entry);

        // remove from our llist
        this->internalDetach(entry);

        delete entry;
    }
}

void GrResourceCache::getLimits(int* maxResources, size_t* maxResourceBytes) const{
    if (maxResources) {
        *maxResources = fMaxCount;
    }
    if (maxResourceBytes) {
        *maxResourceBytes = fMaxBytes;
    }
}

void GrResourceCache::setLimits(int maxResources, size_t maxResourceBytes) {
    bool smaller = (maxResources < fMaxCount) || (maxResourceBytes < fMaxBytes);

    fMaxCount = maxResources;
    fMaxBytes = maxResourceBytes;

    if (smaller) {
        this->purgeAsNeeded();
    }
}

void GrResourceCache::internalDetach(GrResourceCacheEntry* entry) {
    fList.remove(entry);
    fEntryCount -= 1;
    fEntryBytes -= entry->fCachedSize;
}

void GrResourceCache::attachToHead(GrResourceCacheEntry* entry) {
    fList.addToHead(entry);

    fEntryCount += 1;
    fEntryBytes += entry->fCachedSize;

#if GR_CACHE_STATS
    if (fHighWaterEntryCount < fEntryCount) {
        fHighWaterEntryCount = fEntryCount;
    }
    if (fHighWaterEntryBytes < fEntryBytes) {
        fHighWaterEntryBytes = fEntryBytes;
    }
#endif
}

// This functor just searches for an entry with only a single ref (from
// the texture cache itself). Presumably in this situation no one else
// is relying on the texture.
class GrTFindUnreffedFunctor {
public:
    bool operator()(const GrResourceCacheEntry* entry) const {
        return entry->resource()->isPurgable();
    }
};


void GrResourceCache::makeResourceMRU(GrGpuResource* resource) {
    GrResourceCacheEntry* entry = resource->getCacheEntry();
    if (entry) {
        this->internalDetach(entry);
        this->attachToHead(entry);
    }
}

void GrResourceCache::notifyPurgable(const GrGpuResource* resource) {
    // Remove scratch textures from the cache the moment they become purgeable if
    // scratch texture reuse is turned off.
    SkASSERT(resource->getCacheEntry());
    if (resource->getCacheEntry()->key().getResourceType() == GrTexturePriv::ResourceType() &&
        resource->getCacheEntry()->key().isScratch() &&
        !fCaps->reuseScratchTextures() &&
        !(static_cast<const GrSurface*>(resource)->desc().fFlags & kRenderTarget_GrSurfaceFlag)) {
        this->deleteResource(resource->getCacheEntry());
    }
}

GrGpuResource* GrResourceCache::find(const GrResourceKey& key) {
    // GrResourceCache2 is responsible for scratch resources.
    SkASSERT(!key.isScratch());

    GrAutoResourceCacheValidate atcv(this);

    GrResourceCacheEntry* entry = fCache.find(key);
    if (NULL == entry) {
        return NULL;
    }

    // Make this resource MRU
    this->internalDetach(entry);
    this->attachToHead(entry);

    return entry->fResource;
}

void GrResourceCache::addResource(const GrResourceKey& key, GrGpuResource* resource) {
    SkASSERT(NULL == resource->getCacheEntry());
    // we don't expect to create new resources during a purge. In theory
    // this could cause purgeAsNeeded() into an infinite loop (e.g.
    // each resource destroyed creates and locks 2 resources and
    // unlocks 1 thereby causing a new purge).
    SkASSERT(!fPurging);
    GrAutoResourceCacheValidate atcv(this);

    GrResourceCacheEntry* entry = SkNEW_ARGS(GrResourceCacheEntry, (this, key, resource));
    resource->setCacheEntry(entry);

    this->attachToHead(entry);
    fCache.insert(key, entry);

    this->purgeAsNeeded();
}

void GrResourceCache::didIncreaseResourceSize(const GrResourceCacheEntry* entry, size_t amountInc) {
    fEntryBytes += amountInc;
    this->purgeAsNeeded();
}

void GrResourceCache::didDecreaseResourceSize(const GrResourceCacheEntry* entry, size_t amountDec) {
    fEntryBytes -= amountDec;
#ifdef SK_DEBUG
    this->validate();
#endif
}

/**
 * Destroying a resource may potentially trigger the unlock of additional
 * resources which in turn will trigger a nested purge. We block the nested
 * purge using the fPurging variable. However, the initial purge will keep
 * looping until either all resources in the cache are unlocked or we've met
 * the budget. There is an assertion in createAndLock to check against a
 * resource's destructor inserting new resources into the cache. If these
 * new resources were unlocked before purgeAsNeeded completed it could
 * potentially make purgeAsNeeded loop infinitely.
 *
 * extraCount and extraBytes are added to the current resource totals to account
 * for incoming resources (e.g., GrContext is about to add 10MB split between
 * 10 textures).
 */
void GrResourceCache::purgeAsNeeded(int extraCount, size_t extraBytes) {
    if (fPurging) {
        return;
    }

    fPurging = true;

    this->purgeInvalidated();

    this->internalPurge(extraCount, extraBytes);
    if (((fEntryCount+extraCount) > fMaxCount ||
        (fEntryBytes+extraBytes) > fMaxBytes) &&
        fOverbudgetCB) {
        // Despite the purge we're still over budget. See if Ganesh can
        // release some resources and purge again.
        if ((*fOverbudgetCB)(fOverbudgetData)) {
            this->internalPurge(extraCount, extraBytes);
        }
    }

    fPurging = false;
}

void GrResourceCache::purgeInvalidated() {
    SkTDArray<GrResourceInvalidatedMessage> invalidated;
    fInvalidationInbox.poll(&invalidated);

    for (int i = 0; i < invalidated.count(); i++) {
        while (GrResourceCacheEntry* entry = fCache.find(invalidated[i].key, GrTFindUnreffedFunctor())) {
            this->deleteResource(entry);
        }
    }
}

void GrResourceCache::deleteResource(GrResourceCacheEntry* entry) {
    SkASSERT(entry->fResource->isPurgable());

    // remove from our cache
    fCache.remove(entry->key(), entry);

    // remove from our llist
    this->internalDetach(entry);
    delete entry;
}

void GrResourceCache::internalPurge(int extraCount, size_t extraBytes) {
    SkASSERT(fPurging);

    bool withinBudget = false;
    bool changed = false;

    // The purging process is repeated several times since one pass
    // may free up other resources
    do {
        EntryList::Iter iter;

        changed = false;

        // Note: the following code relies on the fact that the
        // doubly linked list doesn't invalidate its data/pointers
        // outside of the specific area where a deletion occurs (e.g.,
        // in internalDetach)
        GrResourceCacheEntry* entry = iter.init(fList, EntryList::Iter::kTail_IterStart);

        while (entry) {
            GrAutoResourceCacheValidate atcv(this);

            if ((fEntryCount+extraCount) <= fMaxCount &&
                (fEntryBytes+extraBytes) <= fMaxBytes) {
                withinBudget = true;
                break;
            }

            GrResourceCacheEntry* prev = iter.prev();
            if (entry->fResource->isPurgable()) {
                changed = true;
                this->deleteResource(entry);
            }
            entry = prev;
        }
    } while (!withinBudget && changed);
}

void GrResourceCache::purgeAllUnlocked() {
    GrAutoResourceCacheValidate atcv(this);

    // we can have one GrCacheable holding a lock on another
    // so we don't want to just do a simple loop kicking each
    // entry out. Instead change the budget and purge.

    size_t savedMaxBytes = fMaxBytes;
    int savedMaxCount = fMaxCount;
    fMaxBytes = (size_t) -1;
    fMaxCount = 0;
    this->purgeAsNeeded();

#ifdef SK_DEBUG
    if (!fCache.count()) {
        SkASSERT(fList.isEmpty());
    }
#endif

    fMaxBytes = savedMaxBytes;
    fMaxCount = savedMaxCount;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef SK_DEBUG
size_t GrResourceCache::countBytes(const EntryList& list) {
    size_t bytes = 0;

    EntryList::Iter iter;

    const GrResourceCacheEntry* entry = iter.init(const_cast<EntryList&>(list),
                                                  EntryList::Iter::kTail_IterStart);

    for ( ; entry; entry = iter.prev()) {
        bytes += entry->resource()->gpuMemorySize();
    }
    return bytes;
}

static bool both_zero_or_nonzero(int count, size_t bytes) {
    return (count == 0 && bytes == 0) || (count > 0 && bytes > 0);
}

void GrResourceCache::validate() const {
    fList.validate();
    SkASSERT(both_zero_or_nonzero(fEntryCount, fEntryBytes));
    SkASSERT(fEntryCount == fCache.count());

    EntryList::Iter iter;

    // check that the shareable entries are okay
    const GrResourceCacheEntry* entry = iter.init(const_cast<EntryList&>(fList),
                                                  EntryList::Iter::kHead_IterStart);

    int count = 0;
    for ( ; entry; entry = iter.next()) {
        entry->validate();
        SkASSERT(fCache.find(entry->key()));
        count += 1;
    }
    SkASSERT(count == fEntryCount);

    size_t bytes = this->countBytes(fList);
    SkASSERT(bytes == fEntryBytes);
    SkASSERT(fList.countEntries() == fEntryCount);
}
#endif // SK_DEBUG

#if GR_CACHE_STATS

void GrResourceCache::printStats() {
    int locked = 0;
    int scratch = 0;

    EntryList::Iter iter;

    GrResourceCacheEntry* entry = iter.init(fList, EntryList::Iter::kTail_IterStart);

    for ( ; entry; entry = iter.prev()) {
        if (!entry->fResource->isPurgable()) {
            ++locked;
        }
        if (entry->fResource->isScratch()) {
            ++scratch;
        }
    }

    float countUtilization = (100.f * fEntryCount) / fMaxCount;
    float byteUtilization = (100.f * fEntryBytes) / fMaxBytes;

    SkDebugf("Budget: %d items %d bytes\n", fMaxCount, fMaxBytes);
    SkDebugf("\t\tEntry Count: current %d (%d locked, %d scratch %.2g%% full), high %d\n",
                fEntryCount, locked, scratch, countUtilization, fHighWaterEntryCount);
    SkDebugf("\t\tEntry Bytes: current %d (%.2g%% full) high %d\n",
                fEntryBytes, byteUtilization, fHighWaterEntryBytes);
}

#endif

///////////////////////////////////////////////////////////////////////////////
