// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
    : OutputSurface(context_provider),
      surface_id_(surface_id),
      output_surface_map_(output_surface_map),
      vsync_manager_(vsync_manager) {
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
    : OutputSurface(software_device.Pass()),
      surface_id_(surface_id),
      output_surface_map_(output_surface_map),
      vsync_manager_(vsync_manager) {
  Initialize();
}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (reflector_.get())
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_.get());
  if (!HasClient())
    return;
  output_surface_map_->Remove(surface_id_);
  vsync_manager_->RemoveObserver(this);
}

void BrowserCompositorOutputSurface::Initialize() {
  capabilities_.max_frames_pending = 1;
  capabilities_.adjust_deadline_for_parent = false;

  DetachFromThread();
}

bool BrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());

  if (!OutputSurface::BindToClient(client))
    return false;

  output_surface_map_->AddWithID(this, surface_id_);
  if (reflector_.get())
    reflector_->OnSourceSurfaceReady(this);
  vsync_manager_->AddObserver(this);
  return true;
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  CommitVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  vsync_manager_->UpdateVSyncParameters(timebase, interval);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  reflector_ = reflector;
}

}  // namespace content
