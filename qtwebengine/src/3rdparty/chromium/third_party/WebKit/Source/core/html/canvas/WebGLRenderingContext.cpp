/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/WebGLRenderingContext.h"

#include "core/frame/LocalFrame.h"
#include "core/html/canvas/ANGLEInstancedArrays.h"
#include "core/html/canvas/EXTBlendMinMax.h"
#include "core/html/canvas/EXTFragDepth.h"
#include "core/html/canvas/EXTShaderTextureLOD.h"
#include "core/html/canvas/EXTTextureFilterAnisotropic.h"
#include "core/html/canvas/EXTsRGB.h"
#include "core/html/canvas/OESElementIndexUint.h"
#include "core/html/canvas/OESStandardDerivatives.h"
#include "core/html/canvas/OESTextureFloat.h"
#include "core/html/canvas/OESTextureFloatLinear.h"
#include "core/html/canvas/OESTextureHalfFloat.h"
#include "core/html/canvas/OESTextureHalfFloatLinear.h"
#include "core/html/canvas/OESVertexArrayObject.h"
#include "core/html/canvas/WebGLCompressedTextureATC.h"
#include "core/html/canvas/WebGLCompressedTextureETC1.h"
#include "core/html/canvas/WebGLCompressedTexturePVRTC.h"
#include "core/html/canvas/WebGLCompressedTextureS3TC.h"
#include "core/html/canvas/WebGLContextAttributes.h"
#include "core/html/canvas/WebGLContextEvent.h"
#include "core/html/canvas/WebGLDebugRendererInfo.h"
#include "core/html/canvas/WebGLDebugShaders.h"
#include "core/html/canvas/WebGLDepthTexture.h"
#include "core/html/canvas/WebGLDrawBuffers.h"
#include "core/html/canvas/WebGLLoseContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderBox.h"
#include "platform/CheckedInt.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "public/platform/Platform.h"

namespace blink {

PassOwnPtrWillBeRawPtr<WebGLRenderingContext> WebGLRenderingContext::create(HTMLCanvasElement* canvas, WebGLContextAttributes* attrs)
{
    Document& document = canvas->document();
    LocalFrame* frame = document.frame();
    if (!frame) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Web page was not allowed to create a WebGL context."));
        return nullptr;
    }
    Settings* settings = frame->settings();

    // The FrameLoaderClient might block creation of a new WebGL context despite the page settings; in
    // particular, if WebGL contexts were lost one or more times via the GL_ARB_robustness extension.
    if (!frame->loader().client()->allowWebGL(settings && settings->webGLEnabled())) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Web page was not allowed to create a WebGL context."));
        return nullptr;
    }

    // The only situation that attrs is null is through Document::getCSSCanvasContext().
    RefPtrWillBeRawPtr<WebGLContextAttributes> defaultAttrs = nullptr;
    if (!attrs) {
        defaultAttrs = WebGLContextAttributes::create();
        attrs = defaultAttrs.get();
    }
    blink::WebGraphicsContext3D::Attributes attributes = attrs->attributes(document.topDocument().url().string(), settings, 1);
    blink::WebGLInfo glInfo;
    OwnPtr<blink::WebGraphicsContext3D> context = adoptPtr(blink::Platform::current()->createOffscreenGraphicsContext3D(attributes, 0, &glInfo));
    if (!context) {
        String statusMessage("Could not create a WebGL context for VendorInfo = ");
        statusMessage.append(glInfo.vendorInfo);
        statusMessage.append(", RendererInfo = ");
        statusMessage.append(glInfo.rendererInfo);
        statusMessage.append(", DriverInfo = ");
        statusMessage.append(glInfo.driverVersion);
        statusMessage.append(".");
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, statusMessage));
        return nullptr;
    }

    OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(context.get());
    if (!extensionsUtil)
        return nullptr;
    if (extensionsUtil->supportsExtension("GL_EXT_debug_marker"))
        context->pushGroupMarkerEXT("WebGLRenderingContext");

    OwnPtrWillBeRawPtr<WebGLRenderingContext> renderingContext = adoptPtrWillBeNoop(new WebGLRenderingContext(canvas, context.release(), attrs));
    renderingContext->registerContextExtensions();
    renderingContext->suspendIfNeeded();

    if (!renderingContext->drawingBuffer()) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Could not create a WebGL context."));
        return nullptr;
    }

    return renderingContext.release();
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassOwnPtr<blink::WebGraphicsContext3D> context, WebGLContextAttributes* requestedAttributes)
    : WebGLRenderingContextBase(passedCanvas, context, requestedAttributes)
{
}

WebGLRenderingContext::~WebGLRenderingContext()
{
}

void WebGLRenderingContext::registerContextExtensions()
{
    // Register extensions.
    static const char* const bothPrefixes[] = { "", "WEBKIT_", 0, };

    registerExtension<ANGLEInstancedArrays>(m_angleInstancedArrays);
    registerExtension<EXTBlendMinMax>(m_extBlendMinMax);
    registerExtension<EXTFragDepth>(m_extFragDepth);
    registerExtension<EXTShaderTextureLOD>(m_extShaderTextureLOD);
    registerExtension<EXTsRGB>(m_extsRGB);
    registerExtension<EXTTextureFilterAnisotropic>(m_extTextureFilterAnisotropic, ApprovedExtension, bothPrefixes);
    registerExtension<OESElementIndexUint>(m_oesElementIndexUint);
    registerExtension<OESStandardDerivatives>(m_oesStandardDerivatives);
    registerExtension<OESTextureFloat>(m_oesTextureFloat);
    registerExtension<OESTextureFloatLinear>(m_oesTextureFloatLinear);
    registerExtension<OESTextureHalfFloat>(m_oesTextureHalfFloat);
    registerExtension<OESTextureHalfFloatLinear>(m_oesTextureHalfFloatLinear);
    registerExtension<OESVertexArrayObject>(m_oesVertexArrayObject);
    registerExtension<WebGLCompressedTextureATC>(m_webglCompressedTextureATC, ApprovedExtension, bothPrefixes);
    registerExtension<WebGLCompressedTextureETC1>(m_webglCompressedTextureETC1);
    registerExtension<WebGLCompressedTexturePVRTC>(m_webglCompressedTexturePVRTC, ApprovedExtension, bothPrefixes);
    registerExtension<WebGLCompressedTextureS3TC>(m_webglCompressedTextureS3TC, ApprovedExtension, bothPrefixes);
    registerExtension<WebGLDebugRendererInfo>(m_webglDebugRendererInfo);
    registerExtension<WebGLDebugShaders>(m_webglDebugShaders);
    registerExtension<WebGLDepthTexture>(m_webglDepthTexture, ApprovedExtension, bothPrefixes);
    registerExtension<WebGLDrawBuffers>(m_webglDrawBuffers);
    registerExtension<WebGLLoseContext>(m_webglLoseContext, ApprovedExtension, bothPrefixes);
}

void WebGLRenderingContext::trace(Visitor* visitor)
{
    visitor->trace(m_angleInstancedArrays);
    visitor->trace(m_extBlendMinMax);
    visitor->trace(m_extFragDepth);
    visitor->trace(m_extShaderTextureLOD);
    visitor->trace(m_extsRGB);
    visitor->trace(m_extTextureFilterAnisotropic);
    visitor->trace(m_oesTextureFloat);
    visitor->trace(m_oesTextureFloatLinear);
    visitor->trace(m_oesTextureHalfFloat);
    visitor->trace(m_oesTextureHalfFloatLinear);
    visitor->trace(m_oesStandardDerivatives);
    visitor->trace(m_oesVertexArrayObject);
    visitor->trace(m_oesElementIndexUint);
    visitor->trace(m_webglLoseContext);
    visitor->trace(m_webglDebugRendererInfo);
    visitor->trace(m_webglDebugShaders);
    visitor->trace(m_webglDrawBuffers);
    visitor->trace(m_webglCompressedTextureATC);
    visitor->trace(m_webglCompressedTextureETC1);
    visitor->trace(m_webglCompressedTexturePVRTC);
    visitor->trace(m_webglCompressedTextureS3TC);
    visitor->trace(m_webglDepthTexture);
    WebGLRenderingContextBase::trace(visitor);
}

} // namespace blink
