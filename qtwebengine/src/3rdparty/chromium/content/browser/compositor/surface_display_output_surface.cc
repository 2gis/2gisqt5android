// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_display_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/onscreen_display_client.h"

namespace content {

SurfaceDisplayOutputSurface::SurfaceDisplayOutputSurface(
    cc::SurfaceManager* surface_manager,
    cc::SurfaceIdAllocator* allocator,
    const scoped_refptr<cc::ContextProvider>& context_provider)
    : cc::OutputSurface(context_provider,
                        scoped_ptr<cc::SoftwareOutputDevice>()),
      display_client_(NULL),
      surface_manager_(surface_manager),
      factory_(surface_manager, this),
      allocator_(allocator) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

SurfaceDisplayOutputSurface::~SurfaceDisplayOutputSurface() {
  client_ = NULL;
  if (!surface_id_.is_null()) {
    factory_.Destroy(surface_id_);
  }
}

void SurfaceDisplayOutputSurface::ReceivedVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  CommitVSyncParameters(timebase, interval);
}

void SurfaceDisplayOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size != display_size_) {
    if (!surface_id_.is_null()) {
      factory_.Destroy(surface_id_);
    }
    surface_id_ = allocator_->GenerateId();
    factory_.Create(surface_id_, frame_size);
    display_size_ = frame_size;
  }
  display_client_->display()->Resize(
      surface_id_, frame_size, frame->metadata.device_scale_factor);

  scoped_ptr<cc::CompositorFrame> frame_copy(new cc::CompositorFrame());
  frame->AssignTo(frame_copy.get());
  factory_.SubmitFrame(
      surface_id_,
      frame_copy.Pass(),
      base::Bind(&SurfaceDisplayOutputSurface::SwapBuffersComplete,
                 base::Unretained(this)));

  client_->DidSwapBuffers();
}

bool SurfaceDisplayOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(display_client_);
  client_ = client;
  // Avoid initializing GL context here, as this should be sharing the
  // Display's context.
  return display_client_->Initialize();
}

void SurfaceDisplayOutputSurface::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  cc::CompositorFrameAck ack;
  ack.resources = resources;
  if (client_)
    client_->ReclaimResources(&ack);
}

void SurfaceDisplayOutputSurface::SwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

}  // namespace content
