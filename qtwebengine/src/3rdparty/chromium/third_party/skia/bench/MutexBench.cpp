/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "Benchmark.h"
#include "SkThread.h"

class MutexBench : public Benchmark {
public:
    virtual bool isSuitableFor(Backend backend) SK_OVERRIDE {
        return backend == kNonRendering_Backend;
    }

protected:
    virtual const char* onGetName() {
        return "mutex";
    }

    virtual void onDraw(const int loops, SkCanvas*) {
        SK_DECLARE_STATIC_MUTEX(mu);
        for (int i = 0; i < loops; i++) {
            mu.acquire();
            mu.release();
        }
    }

private:
    typedef Benchmark INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

DEF_BENCH( return new MutexBench(); )
