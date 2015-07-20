/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/gpu/WebGLImageBufferSurface.h"

#include "platform/graphics/skia/GaneshUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

WebGLImageBufferSurface::WebGLImageBufferSurface(const IntSize& size, OpacityMode opacityMode)
    : ImageBufferSurface(size, opacityMode)
{
    m_contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!m_contextProvider)
        return;
    GrContext* gr = m_contextProvider->grContext();
    if (!gr)
        return;
    ensureTextureBackedSkBitmap(gr, m_bitmap, size, kDefault_GrSurfaceOrigin, kRGBA_8888_GrPixelConfig);
}

WebGLImageBufferSurface::~WebGLImageBufferSurface()
{
}

Platform3DObject WebGLImageBufferSurface::getBackingTexture() const
{
    GrTexture* texture = m_bitmap.getTexture();
    if (!texture)
        return 0;
    return texture->getTextureHandle();
}

void WebGLImageBufferSurface::didModifyBackingTexture()
{
    if (!m_bitmap.isNull()) {
        m_bitmap.pixelRef()->notifyPixelsChanged();
    }
}

void WebGLImageBufferSurface::invalidateCachedBitmap()
{
    m_cachedBitmap.reset();
}

void WebGLImageBufferSurface::updateCachedBitmapIfNeeded()
{
    if (m_cachedBitmap.isNull() && m_bitmap.pixelRef()) {
        (m_bitmap.pixelRef())->readPixels(&m_cachedBitmap);
    }
}

} // namespace blink
