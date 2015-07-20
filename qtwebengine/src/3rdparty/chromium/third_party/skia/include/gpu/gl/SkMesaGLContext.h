
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkMesaGLContext_DEFINED
#define SkMesaGLContext_DEFINED

#include "SkGLContext.h"

#if SK_MESA

class SkMesaGLContext : public SkGLContext {
private:
    typedef intptr_t Context;

public:
    virtual ~SkMesaGLContext() SK_OVERRIDE;
    virtual void makeCurrent() const SK_OVERRIDE;
    virtual void swapBuffers() const SK_OVERRIDE;

    static SkMesaGLContext* Create(GrGLStandard forcedGpuAPI) {
        if (kGLES_GrGLStandard == forcedGpuAPI) {
            return NULL;
        }
        SkMesaGLContext* ctx = SkNEW(SkMesaGLContext);
        if (!ctx->isValid()) {
            SkDELETE(ctx);
            return NULL;
        }
        return ctx;
    }

private:
    SkMesaGLContext();
    void destroyGLContext();

    Context fContext;
    GrGLubyte *fImage;
};

#endif

#endif
