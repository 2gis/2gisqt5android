/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrAARectRenderer_DEFINED
#define GrAARectRenderer_DEFINED

#include "SkMatrix.h"
#include "SkRect.h"
#include "SkRefCnt.h"
#include "SkStrokeRec.h"

class GrGpu;
class GrDrawTarget;
class GrIndexBuffer;

/*
 * This class wraps helper functions that draw AA rects (filled & stroked)
 */
class GrAARectRenderer : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(GrAARectRenderer)

    GrAARectRenderer(GrGpu* gpu)
    : fGpu(gpu)
    , fAAFillRectIndexBuffer(NULL)
    , fAAMiterStrokeRectIndexBuffer(NULL)
    , fAABevelStrokeRectIndexBuffer(NULL) {
    }

    void reset();

    ~GrAARectRenderer() {
        this->reset();
    }

    // TODO: potentialy fuse the fill & stroke methods and differentiate
    // between them by passing in stroke (==NULL means fill).

    void fillAARect(GrDrawTarget* target,
                    const SkRect& rect,
                    const SkMatrix& combinedMatrix,
                    const SkRect& devRect) {
#ifdef SHADER_AA_FILL_RECT
        if (combinedMatrix.rectStaysRect()) {
            this->shaderFillAlignedAARect(gpu, target,
                                          rect, combinedMatrix);
        } else {
            this->shaderFillAARect(gpu, target,
                                   rect, combinedMatrix);
        }
#else
        this->geometryFillAARect(target, rect, combinedMatrix, devRect);
#endif
    }

    void strokeAARect(GrDrawTarget* target,
                      const SkRect& rect,
                      const SkMatrix& combinedMatrix,
                      const SkRect& devRect,
                      const SkStrokeRec& stroke);

    // First rect is outer; second rect is inner
    void fillAANestedRects(GrDrawTarget* target,
                           const SkRect rects[2],
                           const SkMatrix& combinedMatrix);

private:
    GrIndexBuffer* aaStrokeRectIndexBuffer(bool miterStroke);

    void geometryFillAARect(GrDrawTarget* target,
                            const SkRect& rect,
                            const SkMatrix& combinedMatrix,
                            const SkRect& devRect);

    void shaderFillAARect(GrDrawTarget* target,
                          const SkRect& rect,
                          const SkMatrix& combinedMatrix);

    void shaderFillAlignedAARect(GrDrawTarget* target,
                                 const SkRect& rect,
                                 const SkMatrix& combinedMatrix);

    void geometryStrokeAARect(GrDrawTarget* target,
                              const SkRect& devOutside,
                              const SkRect& devOutsideAssist,
                              const SkRect& devInside,
                              bool miterStroke);

    GrGpu*                      fGpu;
    GrIndexBuffer*              fAAFillRectIndexBuffer;
    GrIndexBuffer*              fAAMiterStrokeRectIndexBuffer;
    GrIndexBuffer*              fAABevelStrokeRectIndexBuffer;

    typedef SkRefCnt INHERITED;
};

#endif // GrAARectRenderer_DEFINED
