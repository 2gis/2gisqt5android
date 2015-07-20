// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence.h"

#include "base/compiler_specific.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_arb.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_fence_nv.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gfx {

namespace {

// static
GLFence* CreateFence(bool flush) {
  DCHECK(GLContext::GetCurrent())
      << "Trying to create fence with no context";

  scoped_ptr<GLFence> fence;
  // Prefer ARB_sync which supports server-side wait.
  if (g_driver_gl.ext.b_GL_ARB_sync ||
      GetGLVersionInfo()->is_es3) {
    fence.reset(new GLFenceARB(flush));
#if !defined(OS_MACOSX)
  } else if (g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    fence.reset(new GLFenceEGL(flush));
#endif
  } else if (g_driver_gl.ext.b_GL_NV_fence) {
    fence.reset(new GLFenceNV(flush));
  }

  DCHECK_EQ(!!fence.get(), GLFence::IsSupported());
  return fence.release();
}

}  // namespace

GLFence::GLFence() {
}

GLFence::~GLFence() {
}

bool GLFence::IsSupported() {
  DCHECK(GetGLVersionInfo());
  return g_driver_gl.ext.b_GL_ARB_sync || GetGLVersionInfo()->is_es3 ||
#if !defined(OS_MACOSX)
         g_driver_egl.ext.b_EGL_KHR_fence_sync ||
#endif
         g_driver_gl.ext.b_GL_NV_fence;
}

GLFence* GLFence::Create() {
  return CreateFence(true);
}

GLFence* GLFence::CreateWithoutFlush() {
  return CreateFence(false);
}

}  // namespace gfx
