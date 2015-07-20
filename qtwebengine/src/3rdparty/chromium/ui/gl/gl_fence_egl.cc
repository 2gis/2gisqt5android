// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_egl.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gfx {

GLFenceEGL::GLFenceEGL(bool flush) {
  display_ = eglGetCurrentDisplay();
  sync_ = eglCreateSyncKHR(display_, EGL_SYNC_FENCE_KHR, NULL);
  DCHECK(sync_ != EGL_NO_SYNC_KHR);
  if (flush) {
    glFlush();
  } else {
    flush_event_ = GLContext::GetCurrent()->SignalFlush();
  }
}

TransferableFence GLFenceEGL::Transfer() {
  gfx::TransferableFence ret;
  ret.type = gfx::TransferableFence::EglSync;
  ret.egl.display = display_;
  ret.egl.sync = sync_;
  sync_ = 0;
  return ret;
}

bool GLFenceEGL::HasCompleted() {
  EGLint value = 0;
  if (eglGetSyncAttribKHR(display_, sync_, EGL_SYNC_STATUS_KHR, &value) !=
      EGL_TRUE) {
    LOG(ERROR) << "Failed to get EGLSync attribute. error code:"
               << eglGetError();
    return true;
  }

  DCHECK(value == EGL_SIGNALED_KHR || value == EGL_UNSIGNALED_KHR);
  return !value || value == EGL_SIGNALED_KHR;
}

void GLFenceEGL::ClientWait() {
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    EGLint flags = 0;
    EGLTimeKHR time = EGL_FOREVER_KHR;
    EGLint result = eglClientWaitSyncKHR(display_, sync_, flags, time);
    DCHECK_NE(EGL_TIMEOUT_EXPIRED_KHR, result);
    if (result == EGL_FALSE) {
      LOG(FATAL) << "Failed to wait for EGLSync. error:"
                 << ui::GetLastEGLErrorString();
    }
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

void GLFenceEGL::ServerWait() {
  if (!gfx::g_driver_egl.ext.b_EGL_KHR_wait_sync) {
    ClientWait();
    return;
  }
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    EGLint flags = 0;
    if (eglWaitSyncKHR(display_, sync_, flags) == EGL_FALSE) {
      LOG(FATAL) << "Failed to wait for EGLSync. error:"
                 << ui::GetLastEGLErrorString();
    }
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

GLFenceEGL::~GLFenceEGL() {
  eglDestroySyncKHR(display_, sync_);
}

}  // namespace gfx
