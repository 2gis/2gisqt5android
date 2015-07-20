/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrGLRenderTarget.h"

#include "GrGpuGL.h"

#define GPUGL static_cast<GrGpuGL*>(this->getGpu())
#define GL_CALL(X) GR_GL_CALL(GPUGL->glInterface(), X)

// Because this class is virtually derived from GrSurface we must explicitly call its constructor.
GrGLRenderTarget::GrGLRenderTarget(GrGpuGL* gpu, const GrSurfaceDesc& desc, const IDDesc& idDesc)
    : GrSurface(gpu, idDesc.fIsWrapped, desc)
    , INHERITED(gpu, idDesc.fIsWrapped, desc) {
    this->init(desc, idDesc);
    this->registerWithCache();
}

GrGLRenderTarget::GrGLRenderTarget(GrGpuGL* gpu, const GrSurfaceDesc& desc, const IDDesc& idDesc,
                                   Derived)
    : GrSurface(gpu, idDesc.fIsWrapped, desc)
    , INHERITED(gpu, idDesc.fIsWrapped, desc) {
    this->init(desc, idDesc);
}

void GrGLRenderTarget::init(const GrSurfaceDesc& desc, const IDDesc& idDesc) {
    fRTFBOID                = idDesc.fRTFBOID;
    fTexFBOID               = idDesc.fTexFBOID;
    fMSColorRenderbufferID  = idDesc.fMSColorRenderbufferID;
    fIsWrapped              = idDesc.fIsWrapped;

    fViewport.fLeft   = 0;
    fViewport.fBottom = 0;
    fViewport.fWidth  = desc.fWidth;
    fViewport.fHeight = desc.fHeight;

    // We own one color value for each MSAA sample.
    fColorValuesPerPixel = SkTMax(1, fDesc.fSampleCnt);
    if (fTexFBOID != fRTFBOID) {
        // If we own the resolve buffer then that is one more sample per pixel.
        fColorValuesPerPixel += 1;
    } 
}

size_t GrGLRenderTarget::gpuMemorySize() const {
    SkASSERT(kUnknown_GrPixelConfig != fDesc.fConfig);
    SkASSERT(!GrPixelConfigIsCompressed(fDesc.fConfig));
    size_t colorBytes = GrBytesPerPixel(fDesc.fConfig);
    SkASSERT(colorBytes > 0);
    return fColorValuesPerPixel * fDesc.fWidth * fDesc.fHeight * colorBytes;
}

void GrGLRenderTarget::onRelease() {
    if (!fIsWrapped) {
        if (fTexFBOID) {
            GL_CALL(DeleteFramebuffers(1, &fTexFBOID));
        }
        if (fRTFBOID && fRTFBOID != fTexFBOID) {
            GL_CALL(DeleteFramebuffers(1, &fRTFBOID));
        }
        if (fMSColorRenderbufferID) {
            GL_CALL(DeleteRenderbuffers(1, &fMSColorRenderbufferID));
        }
    }
    fRTFBOID                = 0;
    fTexFBOID               = 0;
    fMSColorRenderbufferID  = 0;
    fIsWrapped              = false;
    INHERITED::onRelease();
}

void GrGLRenderTarget::onAbandon() {
    fRTFBOID                = 0;
    fTexFBOID               = 0;
    fMSColorRenderbufferID  = 0;
    fIsWrapped              = false;
    INHERITED::onAbandon();
}
