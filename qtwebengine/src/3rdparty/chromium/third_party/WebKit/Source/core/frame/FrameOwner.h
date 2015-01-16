// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameOwner_h
#define FrameOwner_h

#include "core/dom/SandboxFlags.h"

namespace WebCore {

class FrameOwner {
public:
    virtual ~FrameOwner() { }

    virtual bool isLocal() const = 0;

    virtual SandboxFlags sandboxFlags() const = 0;
    virtual void dispatchLoad() = 0;
};

} // namespace WebCore

#endif // FrameOwner_h
