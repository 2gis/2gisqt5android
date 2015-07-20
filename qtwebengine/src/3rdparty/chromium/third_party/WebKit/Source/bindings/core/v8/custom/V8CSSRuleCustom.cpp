/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8CSSRule.h"

#include "bindings/core/v8/V8CSSCharsetRule.h"
#include "bindings/core/v8/V8CSSFontFaceRule.h"
#include "bindings/core/v8/V8CSSImportRule.h"
#include "bindings/core/v8/V8CSSKeyframeRule.h"
#include "bindings/core/v8/V8CSSKeyframesRule.h"
#include "bindings/core/v8/V8CSSMediaRule.h"
#include "bindings/core/v8/V8CSSPageRule.h"
#include "bindings/core/v8/V8CSSStyleRule.h"
#include "bindings/core/v8/V8CSSSupportsRule.h"
#include "bindings/core/v8/V8CSSViewportRule.h"
#include "bindings/core/v8/V8WebKitCSSFilterRule.h"

namespace blink {

v8::Handle<v8::Object> wrapCustom(CSSRule* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->type()) {
    case CSSRule::UNKNOWN_RULE:
        // CSSUnknownRule.idl is explicitly excluded as it doesn't add anything
        // over CSSRule.idl (see core/core.gypi: 'core_idl_files').
        // -> Use the base class wrapper here.
        return impl->wrap(creationContext, isolate, &V8CSSRule::wrapperTypeInfo);
    case CSSRule::STYLE_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSStyleRule::wrapperTypeInfo);
    case CSSRule::CHARSET_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSCharsetRule::wrapperTypeInfo);
    case CSSRule::IMPORT_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSImportRule::wrapperTypeInfo);
    case CSSRule::MEDIA_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSMediaRule::wrapperTypeInfo);
    case CSSRule::FONT_FACE_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSFontFaceRule::wrapperTypeInfo);
    case CSSRule::PAGE_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSPageRule::wrapperTypeInfo);
    case CSSRule::KEYFRAME_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSKeyframeRule::wrapperTypeInfo);
    case CSSRule::KEYFRAMES_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSKeyframesRule::wrapperTypeInfo);
    case CSSRule::SUPPORTS_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSSupportsRule::wrapperTypeInfo);
    case CSSRule::VIEWPORT_RULE:
        return impl->wrap(creationContext, isolate, &V8CSSViewportRule::wrapperTypeInfo);
    case CSSRule::WEBKIT_FILTER_RULE:
        return impl->wrap(creationContext, isolate, &V8WebKitCSSFilterRule::wrapperTypeInfo);
    }
    return impl->wrap(creationContext, isolate, &V8CSSRule::wrapperTypeInfo);
}

} // namespace blink
