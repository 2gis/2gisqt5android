// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTShaderTextureLOD_h
#define EXTShaderTextureLOD_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/canvas/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class EXTShaderTextureLOD FINAL : public WebGLExtension, public ScriptWrappable {
public:
    static PassRefPtr<EXTShaderTextureLOD> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    virtual ~EXTShaderTextureLOD();
    virtual WebGLExtensionName name() const OVERRIDE;

private:
    EXTShaderTextureLOD(WebGLRenderingContextBase*);
};

} // namespace WebCore

#endif // EXTShaderTextureLOD_h
