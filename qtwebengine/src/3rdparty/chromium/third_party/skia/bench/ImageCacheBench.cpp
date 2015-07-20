/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Benchmark.h"
#include "SkResourceCache.h"

namespace {
static void* gGlobalAddress;
class TestKey : public SkResourceCache::Key {
public:
    intptr_t fValue;

    TestKey(intptr_t value) : fValue(value) {
        this->init(&gGlobalAddress, sizeof(fValue));
    }
};
struct TestRec : public SkResourceCache::Rec {
    TestKey     fKey;
    intptr_t    fValue;

    TestRec(const TestKey& key, intptr_t value) : fKey(key), fValue(value) {}

    virtual const Key& getKey() const SK_OVERRIDE { return fKey; }
    virtual size_t bytesUsed() const SK_OVERRIDE { return sizeof(fKey) + sizeof(fValue); }

    static bool Visitor(const SkResourceCache::Rec&, void*) {
        return true;
    }
};
}

class ImageCacheBench : public Benchmark {
    SkResourceCache fCache;

    enum {
        CACHE_COUNT = 500
    };
public:
    ImageCacheBench()  : fCache(CACHE_COUNT * 100) {}

    void populateCache() {
        for (int i = 0; i < CACHE_COUNT; ++i) {
            fCache.add(SkNEW_ARGS(TestRec, (TestKey(i), i)));
        }
    }

protected:
    virtual const char* onGetName() SK_OVERRIDE {
        return "imagecache";
    }

    virtual void onDraw(const int loops, SkCanvas*) SK_OVERRIDE {
        if (fCache.getTotalBytesUsed() == 0) {
            this->populateCache();
        }

        TestKey key(-1);
        // search for a miss (-1)
        for (int i = 0; i < loops; ++i) {
            SkDEBUGCODE(bool found =) fCache.find(key, TestRec::Visitor, NULL);
            SkASSERT(!found);
        }
    }

private:
    typedef Benchmark INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

DEF_BENCH( return new ImageCacheBench(); )
