// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderObjectInlines_h
#define RenderObjectInlines_h

#include "core/dom/StyleEngine.h"
#include "core/rendering/RenderObject.h"

namespace blink {

// The following methods are inlined for performance but not put in
// RenderObject.h because that would unnecessarily tie RenderObject.h
// to StyleEngine.h for all users of RenderObject.h that don't use
// these methods.

inline RenderStyle* RenderObject::firstLineStyle() const
{
    return document().styleEngine()->usesFirstLineRules() ? cachedFirstLineStyle() : style();
}

inline RenderStyle* RenderObject::style(bool firstLine) const
{
    return firstLine ? firstLineStyle() : style();
}

}

#endif
