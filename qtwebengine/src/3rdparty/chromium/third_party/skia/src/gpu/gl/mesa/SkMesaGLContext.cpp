
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <GL/osmesa.h>

#include "gl/SkMesaGLContext.h"
#include "gl/GrGLDefines.h"

static const GrGLint gBOGUS_SIZE = 16;

SkMesaGLContext::SkMesaGLContext()
    : fContext(static_cast<Context>(NULL))
    , fImage(NULL) {
    GR_STATIC_ASSERT(sizeof(Context) == sizeof(OSMesaContext));

    /* Create an RGBA-mode context */
#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
    /* specify Z, stencil, accum sizes */
    fContext = (Context)OSMesaCreateContextExt(OSMESA_BGRA, 0, 0, 0, NULL);
#else
    fContext = (Context)OSMesaCreateContext(OSMESA_BGRA, NULL);
#endif
    if (!fContext) {
        SkDebugf("OSMesaCreateContext failed!\n");
        this->destroyGLContext();
        return;
    }
    // Allocate the image buffer
    fImage = (GrGLubyte *) sk_malloc_throw(gBOGUS_SIZE * gBOGUS_SIZE *
                                           4 * sizeof(GrGLubyte));
    if (!fImage) {
        SkDebugf("Alloc image buffer failed!\n");
        this->destroyGLContext();
        return;
    }

    // Bind the buffer to the context and make it current
    if (!OSMesaMakeCurrent((OSMesaContext)fContext,
                           fImage,
                           GR_GL_UNSIGNED_BYTE,
                           gBOGUS_SIZE,
                           gBOGUS_SIZE)) {
        SkDebugf("OSMesaMakeCurrent failed!\n");
        this->destroyGLContext();
        return;
    }

    fGL.reset(GrGLCreateMesaInterface());
    if (NULL == fGL.get()) {
        SkDebugf("Could not create GL interface!\n");
        this->destroyGLContext();
        return;
    }

    if (!fGL->validate()) {
        SkDebugf("Could not validate GL interface!\n");
        this->destroyGLContext();
        return;
    }
}

SkMesaGLContext::~SkMesaGLContext() {
    this->destroyGLContext();
}

void SkMesaGLContext::destroyGLContext() {
    fGL.reset(NULL);
    if (fImage) {
        sk_free(fImage);
        fImage = NULL;
    }

    if (fContext) {
        OSMesaDestroyContext((OSMesaContext)fContext);
        fContext = static_cast<Context>(NULL);
    }
}



void SkMesaGLContext::makeCurrent() const {
    if (fContext) {
        if (!OSMesaMakeCurrent((OSMesaContext)fContext, fImage,
                               GR_GL_UNSIGNED_BYTE, gBOGUS_SIZE, gBOGUS_SIZE)) {
            SkDebugf("Could not make MESA context current.");
        }
    }
}

void SkMesaGLContext::swapBuffers() const { }
