// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTsRGB_h
#define EXTsRGB_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/html/canvas/WebGLExtension.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class EXTsRGB final : public WebGLExtension, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<EXTsRGB> create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    virtual ~EXTsRGB();
    virtual WebGLExtensionName name() const override;

private:
    explicit EXTsRGB(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // EXTsRGB_h
