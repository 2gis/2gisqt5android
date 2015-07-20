// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_ca_layer_tree_mac.h"

#include <map>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/compositor/io_surface_layer_mac.h"
#include "content/browser/compositor/software_layer_mac.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_resize_helper.h"
#include "content/common/gpu/surface_handle_types_mac.h"
#include "content/public/browser/context_factory.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gl/scoped_cgl.h"

namespace content {
namespace {

typedef std::map<gfx::AcceleratedWidget,BrowserCompositorCALayerTreeMac*>
    WidgetToInternalsMap;
base::LazyInstance<WidgetToInternalsMap> g_widget_to_internals_map;

}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorCALayerTreeMac

BrowserCompositorCALayerTreeMac::BrowserCompositorCALayerTreeMac()
    : view_(NULL),
      accelerated_output_surface_id_(0) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin.
  flipped_layer_.reset([[CALayer alloc] init]);
  [flipped_layer_ setGeometryFlipped:YES];
  [flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];

  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static uintptr_t last_sequence_number = 0;
  last_sequence_number += 1;
  native_widget_ = reinterpret_cast<gfx::AcceleratedWidget>(
      last_sequence_number);
  g_widget_to_internals_map.Pointer()->insert(
      std::make_pair(native_widget_, this));

  // Create a compositor to draw the contents of this view.
  compositor_.reset(new ui::Compositor(
      native_widget_,
      content::GetContextFactory(),
      RenderWidgetResizeHelper::Get()->task_runner()));
}

BrowserCompositorCALayerTreeMac::~BrowserCompositorCALayerTreeMac() {
  DCHECK(!view_);
  g_widget_to_internals_map.Pointer()->erase(native_widget_);
}

void BrowserCompositorCALayerTreeMac::SetView(
    BrowserCompositorViewMac* view) {
  // Disable the fade-in animation as the view is added.
  ScopedCAActionDisabler disabler;

  DCHECK(view && !view_);
  view_ = view;
  compositor_->SetRootLayer(view_->ui_root_layer());

  CALayer* background_layer = [view_->native_view() layer];
  DCHECK(background_layer);
  [flipped_layer_ setBounds:[background_layer bounds]];
  [background_layer addSublayer:flipped_layer_];
}

void BrowserCompositorCALayerTreeMac::ResetView() {
  if (!view_)
    return;

  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [flipped_layer_ removeFromSuperlayer];
  DestroyIOSurfaceLayer(io_surface_layer_);
  DestroyCAContextLayer(ca_context_layer_);
  DestroySoftwareLayer();

  accelerated_output_surface_id_ = 0;
  last_swap_size_dip_ = gfx::Size();

  content::ImageTransportFactory::GetInstance()->OnCompositorRecycled(
      compositor_.get());
  compositor_->SetRootLayer(NULL);
  view_ = NULL;
}

bool BrowserCompositorCALayerTreeMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  return last_swap_size_dip_ == dip_size;
}

int BrowserCompositorCALayerTreeMac::GetRendererID() const {
  if (io_surface_layer_)
    return [io_surface_layer_ rendererID];
  return 0;
}

bool BrowserCompositorCALayerTreeMac::IsRendererThrottlingDisabled() const {
  if (view_)
    return view_->client()->BrowserCompositorViewShouldAckImmediately();
  return false;
}

void BrowserCompositorCALayerTreeMac::BeginPumpingFrames() {
  [io_surface_layer_ beginPumpingFrames];
}

void BrowserCompositorCALayerTreeMac::EndPumpingFrames() {
  [io_surface_layer_ endPumpingFrames];
}

void BrowserCompositorCALayerTreeMac::GotAcceleratedFrame(
    uint64 surface_handle, int output_surface_id,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor) {
  // Record the surface and latency info to use when acknowledging this frame.
  DCHECK(!accelerated_output_surface_id_);
  accelerated_output_surface_id_ = output_surface_id;
  accelerated_latency_info_.insert(accelerated_latency_info_.end(),
                                   latency_info.begin(), latency_info.end());

  // If there is no view and therefore no superview to draw into, early-out.
  if (!view_) {
    IOSurfaceLayerDidDrawFrame();
    return;
  }

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  last_swap_size_dip_ = ConvertSizeToDIP(scale_factor, pixel_size);
  switch (GetSurfaceHandleType(surface_handle)) {
    case kSurfaceHandleTypeIOSurface: {
      IOSurfaceID io_surface_id = IOSurfaceIDFromSurfaceHandle(surface_handle);
      GotAcceleratedIOSurfaceFrame(io_surface_id, pixel_size, scale_factor);
      break;
    }
    case kSurfaceHandleTypeCAContext: {
      CAContextID ca_context_id = CAContextIDFromSurfaceHandle(surface_handle);
      GotAcceleratedCAContextFrame(ca_context_id, pixel_size, scale_factor);
      break;
    }
    default:
      LOG(ERROR) << "Unrecognized accelerated frame type.";
      return;
  }
}

void BrowserCompositorCALayerTreeMac::GotAcceleratedCAContextFrame(
    CAContextID ca_context_id,
    gfx::Size pixel_size,
    float scale_factor) {
  // In the layer is replaced, keep the old one around until after the new one
  // is installed to avoid flashes.
  base::scoped_nsobject<CALayerHost> old_ca_context_layer =
      ca_context_layer_;

  // Create the layer to host the layer exported by the GPU process with this
  // particular CAContext ID.
  if ([ca_context_layer_ contextId] != ca_context_id) {
    ca_context_layer_.reset([[CALayerHost alloc] init]);
    [ca_context_layer_ setContextId:ca_context_id];
    [ca_context_layer_
        setAutoresizingMask:kCALayerMaxXMargin|kCALayerMaxYMargin];
    [flipped_layer_ addSublayer:ca_context_layer_];
  }

  // Acknowledge the frame to unblock the compositor immediately (the GPU
  // process will do any required throttling).
  IOSurfaceLayerDidDrawFrame();

  // If this replacing a same-type layer, remove it now that the new layer is
  // in the hierarchy.
  if (old_ca_context_layer != ca_context_layer_)
    DestroyCAContextLayer(old_ca_context_layer);

  // Remove any different-type layers that this is replacing.
  DestroyIOSurfaceLayer(io_surface_layer_);
  DestroySoftwareLayer();
}

void BrowserCompositorCALayerTreeMac::GotAcceleratedIOSurfaceFrame(
    IOSurfaceID io_surface_id,
    gfx::Size pixel_size,
    float scale_factor) {
  // In the layer is replaced, keep the old one around until after the new one
  // is installed to avoid flashes.
  base::scoped_nsobject<IOSurfaceLayer> old_io_surface_layer =
      io_surface_layer_;

  // Create or re-create an IOSurface layer if needed. If there already exists
  // a layer but it has the wrong scale factor or it was poisoned, re-create the
  // layer.
  bool needs_new_layer =
      !io_surface_layer_ ||
      [io_surface_layer_ hasBeenPoisoned] ||
      [io_surface_layer_ scaleFactor] != scale_factor;
  if (needs_new_layer) {
    io_surface_layer_.reset(
        [[IOSurfaceLayer alloc] initWithClient:this
                               withScaleFactor:scale_factor]);
    if (io_surface_layer_)
      [flipped_layer_ addSublayer:io_surface_layer_];
    else
      LOG(ERROR) << "Failed to create IOSurfaceLayer";
  }

  // Open the provided IOSurface.
  if (io_surface_layer_) {
    bool result = [io_surface_layer_ gotFrameWithIOSurface:io_surface_id
                                             withPixelSize:pixel_size
                                           withScaleFactor:scale_factor];
    if (!result) {
      DestroyIOSurfaceLayer(io_surface_layer_);
      LOG(ERROR) << "Failed open IOSurface in IOSurfaceLayer";
    }
  }

  // Give a final complaint if anything with the layer's creation went wrong.
  // This frame will appear blank, the compositor will try to create another,
  // and maybe that will go better.
  if (!io_surface_layer_) {
    LOG(ERROR) << "IOSurfaceLayer is nil, tab will be blank";
    IOSurfaceLayerHitError();
  }

  // Make the CALayer draw and set its size appropriately.
  if (io_surface_layer_) {
    [io_surface_layer_ gotNewFrame];

    // Set the bounds of the accelerated layer to match the size of the frame.
    // If the bounds changed, force the content to be displayed immediately.
    CGRect new_layer_bounds = CGRectMake(
        0, 0, last_swap_size_dip_.width(), last_swap_size_dip_.height());
    bool bounds_changed = !CGRectEqualToRect(
        new_layer_bounds, [io_surface_layer_ bounds]);
    [io_surface_layer_ setBounds:new_layer_bounds];
    if (bounds_changed)
      [io_surface_layer_ setNeedsDisplayAndDisplayAndAck];
  }

  // If this replacing a same-type layer, remove it now that the new layer is
  // in the hierarchy.
  if (old_io_surface_layer != io_surface_layer_)
    DestroyIOSurfaceLayer(old_io_surface_layer);

  // Remove any different-type layers that this is replacing.
  DestroyCAContextLayer(ca_context_layer_);
  DestroySoftwareLayer();
}

void BrowserCompositorCALayerTreeMac::GotSoftwareFrame(
    cc::SoftwareFrameData* frame_data,
    float scale_factor,
    SkCanvas* canvas) {
  if (!frame_data || !canvas || !view_)
    return;

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  // If there is not a layer for software frames, create one.
  if (!software_layer_) {
    software_layer_.reset([[SoftwareLayer alloc] init]);
    [flipped_layer_ addSublayer:software_layer_];
  }

  // Set the software layer to draw the provided canvas.
  SkImageInfo info;
  size_t row_bytes;
  const void* pixels = canvas->peekPixels(&info, &row_bytes);
  gfx::Size pixel_size(info.fWidth, info.fHeight);
  [software_layer_ setContentsToData:pixels
                        withRowBytes:row_bytes
                       withPixelSize:pixel_size
                     withScaleFactor:scale_factor];
  last_swap_size_dip_ = ConvertSizeToDIP(scale_factor, pixel_size);

  // Remove any different-type layers that this is replacing.
  DestroyCAContextLayer(ca_context_layer_);
  DestroyIOSurfaceLayer(io_surface_layer_);
}

void BrowserCompositorCALayerTreeMac::DestroyCAContextLayer(
    base::scoped_nsobject<CALayerHost> ca_context_layer) {
  if (!ca_context_layer)
    return;
  [ca_context_layer removeFromSuperlayer];
  if (ca_context_layer == ca_context_layer_)
    ca_context_layer_.reset();
}

void BrowserCompositorCALayerTreeMac::DestroyIOSurfaceLayer(
    base::scoped_nsobject<IOSurfaceLayer> io_surface_layer) {
  if (!io_surface_layer)
    return;
  [io_surface_layer resetClient];
  [io_surface_layer removeFromSuperlayer];
  if (io_surface_layer == io_surface_layer_)
    io_surface_layer_.reset();
}

void BrowserCompositorCALayerTreeMac::DestroySoftwareLayer() {
  if (!software_layer_)
    return;
  [software_layer_ removeFromSuperlayer];
  software_layer_.reset();
}

bool BrowserCompositorCALayerTreeMac::IOSurfaceLayerShouldAckImmediately()
    const {
  // If there is no view then the accelerated layer is not in the hierarchy
  // and will never draw.
  if (!view_)
    return true;
  return view_->client()->BrowserCompositorViewShouldAckImmediately();
}

void BrowserCompositorCALayerTreeMac::IOSurfaceLayerDidDrawFrame() {
  if (accelerated_output_surface_id_) {
    content::ImageTransportFactory::GetInstance()->OnSurfaceDisplayed(
        accelerated_output_surface_id_);
    accelerated_output_surface_id_ = 0;
  }

  if (view_) {
    view_->client()->BrowserCompositorViewFrameSwapped(
        accelerated_latency_info_);
  }

  accelerated_latency_info_.clear();
}

void BrowserCompositorCALayerTreeMac::IOSurfaceLayerHitError() {
  // Perform all acks that would have been done if the frame had succeeded, to
  // un-block the compositor and renderer.
  IOSurfaceLayerDidDrawFrame();

  // Poison the context being used and request a mulligan.
  [io_surface_layer_ poisonContextAndSharegroup];
  compositor_->ScheduleFullRedraw();
}

// static
BrowserCompositorCALayerTreeMac* BrowserCompositorCALayerTreeMac::
    FromAcceleratedWidget(gfx::AcceleratedWidget widget) {
  WidgetToInternalsMap::const_iterator found =
      g_widget_to_internals_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_internals_map.Pointer()->end())
    return NULL;
  return found->second;
}

void BrowserCompositorCALayerTreeMacGotAcceleratedFrame(
    gfx::AcceleratedWidget widget,
    uint64 surface_handle, int surface_id,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
    bool* disable_throttling, int* renderer_id) {
  BrowserCompositorCALayerTreeMac* ca_layer_tree =
      BrowserCompositorCALayerTreeMac::FromAcceleratedWidget(widget);
  if (ca_layer_tree) {
    ca_layer_tree->GotAcceleratedFrame(
        surface_handle, surface_id, latency_info, pixel_size, scale_factor);
    *disable_throttling = ca_layer_tree->IsRendererThrottlingDisabled();
    *renderer_id = ca_layer_tree->GetRendererID();
  } else {
    *disable_throttling = false;
    *renderer_id = 0;
  }
}

void BrowserCompositorCALayerTreeMacGotSoftwareFrame(
    gfx::AcceleratedWidget widget,
    cc::SoftwareFrameData* frame_data, float scale_factor, SkCanvas* canvas) {
  BrowserCompositorCALayerTreeMac* ca_layer_tree =
      BrowserCompositorCALayerTreeMac::FromAcceleratedWidget(widget);
  if (ca_layer_tree)
    ca_layer_tree->GotSoftwareFrame(frame_data, scale_factor, canvas);
}

}  // namespace content
