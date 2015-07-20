/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef sk_tool_utils_DEFINED
#define sk_tool_utils_DEFINED

#include "SkImageInfo.h"
#include "SkTypeface.h"

class SkBitmap;
class SkCanvas;
class SkPaint;
class SkTestFont;

namespace sk_tool_utils {

    const char* colortype_name(SkColorType);

    /**
     * Sets the paint to use a platform-independent text renderer.
     */
    void set_portable_typeface(SkPaint* paint, const char* name = NULL,
                               SkTypeface::Style style = SkTypeface::kNormal);
    SkTypeface* create_portable_typeface(const char* name, SkTypeface::Style style);
    void report_used_chars();

    /**
     *  Call canvas->writePixels() by using the pixels from bitmap, but with an info that claims
     *  the pixels are colorType + alphaType
     */
    void write_pixels(SkCanvas*, const SkBitmap&, int x, int y, SkColorType, SkAlphaType);

    // private to sk_tool_utils
    SkTypeface* create_font(const char* name, SkTypeface::Style );
    SkTypeface* resource_font(const char* name, SkTypeface::Style );

}  // namespace sk_tool_utils

#endif  // sk_tool_utils_DEFINED
