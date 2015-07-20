// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/blocking_task_runner.h"

namespace cc {

Display::Display(DisplayClient* client,
                 SurfaceManager* manager,
                 SharedBitmapManager* bitmap_manager,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : client_(client),
      manager_(manager),
      bitmap_manager_(bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      device_scale_factor_(1.f),
      blocking_main_thread_task_runner_(
          BlockingTaskRunner::Create(base::MessageLoopProxy::current())),
      texture_mailbox_deleter_(
          new TextureMailboxDeleter(base::MessageLoopProxy::current())) {
  manager_->AddObserver(this);
}

Display::~Display() {
  manager_->RemoveObserver(this);
}

bool Display::Initialize(scoped_ptr<OutputSurface> output_surface) {
  output_surface_ = output_surface.Pass();
  return output_surface_->BindToClient(this);
}

void Display::Resize(SurfaceId id,
                     const gfx::Size& size,
                     float device_scale_factor) {
  current_surface_id_ = id;
  current_surface_size_ = size;
  device_scale_factor_ = device_scale_factor;
  client_->DisplayDamaged();
}

void Display::InitializeRenderer() {
  if (resource_provider_)
    return;

  int highp_threshold_min = 0;
  bool use_rgba_4444_texture_format = false;
  size_t id_allocation_chunk_size = 1;
  scoped_ptr<ResourceProvider> resource_provider =
      ResourceProvider::Create(output_surface_.get(),
                               bitmap_manager_,
                               gpu_memory_buffer_manager_,
                               blocking_main_thread_task_runner_.get(),
                               highp_threshold_min,
                               use_rgba_4444_texture_format,
                               id_allocation_chunk_size);
  if (!resource_provider)
    return;

  if (output_surface_->context_provider()) {
    scoped_ptr<GLRenderer> renderer =
        GLRenderer::Create(this,
                           &settings_,
                           output_surface_.get(),
                           resource_provider.get(),
                           texture_mailbox_deleter_.get(),
                           highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  } else {
    scoped_ptr<SoftwareRenderer> renderer = SoftwareRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get());
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  }

  resource_provider_ = resource_provider.Pass();
  aggregator_.reset(new SurfaceAggregator(manager_, resource_provider_.get()));
}

void Display::DidLoseOutputSurface() {
  client_->OutputSurfaceLost();
}

bool Display::Draw() {
  if (current_surface_id_.is_null())
    return false;

  InitializeRenderer();
  if (!output_surface_)
    return false;

  // TODO(skyostil): We should hold a BlockingTaskRunner::CapturePostTasks
  // while Aggregate is called to immediately run release callbacks afterward.
  scoped_ptr<CompositorFrame> frame =
      aggregator_->Aggregate(current_surface_id_);
  if (!frame)
    return false;

  TRACE_EVENT0("cc", "Display::Draw");
  benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
  DelegatedFrameData* frame_data = frame->delegated_frame_data.get();

  gfx::Rect device_viewport_rect = gfx::Rect(current_surface_size_);
  gfx::Rect device_clip_rect = device_viewport_rect;
  bool disable_picture_quad_image_filtering = false;

  renderer_->DecideRenderPassAllocationsForFrame(frame_data->render_pass_list);
  renderer_->DrawFrame(&frame_data->render_pass_list,
                       device_scale_factor_,
                       device_viewport_rect,
                       device_clip_rect,
                       disable_picture_quad_image_filtering);
  renderer_->SwapBuffers(frame->metadata);
  for (SurfaceAggregator::SurfaceIndexMap::iterator it =
           aggregator_->previous_contained_surfaces().begin();
       it != aggregator_->previous_contained_surfaces().end();
       ++it) {
    Surface* surface = manager_->GetSurfaceForId(it->first);
    if (surface)
      surface->RunDrawCallbacks();
  }
  return true;
}

void Display::DidSwapBuffers() {
  client_->DidSwapBuffers();
}

void Display::DidSwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

void Display::CommitVSyncParameters(base::TimeTicks timebase,
                                    base::TimeDelta interval) {
  client_->CommitVSyncParameters(timebase, interval);
}

void Display::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  client_->SetMemoryPolicy(policy);
}

void Display::OnSurfaceDamaged(SurfaceId surface) {
  if (aggregator_ && aggregator_->previous_contained_surfaces().count(surface))
    client_->DisplayDamaged();
}

SurfaceId Display::CurrentSurfaceId() {
  return current_surface_id_;
}

int Display::GetMaxFramesPending() {
  if (!output_surface_)
    return OutputSurface::DEFAULT_MAX_FRAMES_PENDING;
  return output_surface_->capabilities().max_frames_pending;
}

}  // namespace cc
