/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RecordingBench_DEFINED
#define RecordingBench_DEFINED

#include "Benchmark.h"
#include "SkPicture.h"

class RecordingBench : public Benchmark {
public:
    RecordingBench(const char* name, const SkPicture*, bool useBBH);

protected:
    virtual const char* onGetName() SK_OVERRIDE;
    virtual bool isSuitableFor(Backend) SK_OVERRIDE;
    virtual void onDraw(const int loops, SkCanvas*) SK_OVERRIDE;
    virtual SkIPoint onGetSize() SK_OVERRIDE;

private:
    SkAutoTUnref<const SkPicture> fSrc;
    SkString fName;
    bool fUseBBH;

    typedef Benchmark INHERITED;
};

#endif//RecordingBench_DEFINED
