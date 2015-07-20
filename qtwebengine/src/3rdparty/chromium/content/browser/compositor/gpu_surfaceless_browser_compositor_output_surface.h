// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

namespace content {

class BufferQueue;
class GLHelper;

class GpuSurfacelessBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface {
 public:
  GpuSurfacelessBrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      scoped_ptr<cc::OverlayCandidateValidator> overlay_candidate_validator,
      unsigned internalformat,
      bool use_own_gl_helper);
  ~GpuSurfacelessBrowserCompositorOutputSurface() override;

 private:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  void OnSwapBuffersComplete() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  unsigned int internalformat_;
  bool use_own_gl_helper_;
  scoped_ptr<GLHelper> gl_helper_;
  scoped_ptr<BufferQueue> output_surface_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
