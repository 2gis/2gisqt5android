// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event_argument.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/animation/timing_function.h"
#include "cc/base/latency_info_swap_promise_monitor.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/debug_rect_history.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/debug/paint_time_counter.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/delegating_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/bitmap_raster_worker_pool.h"
#include "cc/resources/eviction_tile_priority_queue.h"
#include "cc/resources/gpu_raster_worker_pool.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/one_copy_raster_worker_pool.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/zero_copy_raster_worker_pool.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {

// Small helper class that saves the current viewport location as the user sees
// it and resets to the same location.
class ViewportAnchor {
 public:
  ViewportAnchor(LayerImpl* inner_scroll, LayerImpl* outer_scroll)
  : inner_(inner_scroll),
    outer_(outer_scroll) {
    viewport_in_content_coordinates_ = inner_->TotalScrollOffset();

    if (outer_)
      viewport_in_content_coordinates_ += outer_->TotalScrollOffset();
  }

  void ResetViewportToAnchoredPosition() {
    DCHECK(outer_);

    inner_->ClampScrollToMaxScrollOffset();
    outer_->ClampScrollToMaxScrollOffset();

    gfx::ScrollOffset viewport_location = inner_->TotalScrollOffset() +
                                          outer_->TotalScrollOffset();

    gfx::Vector2dF delta =
        viewport_in_content_coordinates_.DeltaFrom(viewport_location);

    delta = outer_->ScrollBy(delta);
    inner_->ScrollBy(delta);
  }

 private:
  LayerImpl* inner_;
  LayerImpl* outer_;
  gfx::ScrollOffset viewport_in_content_coordinates_;
};


void DidVisibilityChange(LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_ASYNC_BEGIN1("webkit",
                             "LayerTreeHostImpl::SetVisible",
                             id,
                             "LayerTreeHostImpl",
                             id);
    return;
  }

  TRACE_EVENT_ASYNC_END0("webkit", "LayerTreeHostImpl::SetVisible", id);
}

size_t GetMaxTransferBufferUsageBytes(
    const ContextProvider::Capabilities& context_capabilities,
    double refresh_rate) {
  // We want to make sure the default transfer buffer size is equal to the
  // amount of data that can be uploaded by the compositor to avoid stalling
  // the pipeline.
  // For reference Chromebook Pixel can upload 1MB in about 0.5ms.
  const size_t kMaxBytesUploadedPerMs = 1024 * 1024 * 2;

  // We need to upload at least enough work to keep the GPU process busy until
  // the next time it can handle a request to start more uploads from the
  // compositor. We assume that it will pick up any sent upload requests within
  // the time of a vsync, since the browser will want to swap a frame within
  // that time interval, and then uploads should have a chance to be processed.
  size_t ms_per_frame = std::floor(1000.0 / refresh_rate);
  size_t max_transfer_buffer_usage_bytes =
      ms_per_frame * kMaxBytesUploadedPerMs;

  // The context may request a lower limit based on the device capabilities.
  return std::min(context_capabilities.max_transfer_buffer_usage_bytes,
                  max_transfer_buffer_usage_bytes);
}

unsigned GetMapImageTextureTarget(
    const ContextProvider::Capabilities& context_capabilities) {
// TODO(reveman): This should be a setting passed to the compositor instead
// of hard-coded here. The target that need to be used depends on our choice
// of GpuMemoryBuffer type. Note: SURFACE_TEXTURE needs EXTERNAL_OES,
// IO_SURFACE needs RECTANGLE_ARB. crbug.com/431059
#if defined(OS_ANDROID)
  if (context_capabilities.gpu.egl_image_external)
    return GL_TEXTURE_EXTERNAL_OES;
#endif
  if (context_capabilities.gpu.texture_rectangle)
    return GL_TEXTURE_RECTANGLE_ARB;

  return GL_TEXTURE_2D;
}

size_t GetMaxStagingResourceCount() {
  // Upper bound for number of staging resource to allow.
  return 32;
}

}  // namespace

class LayerTreeHostImplTimeSourceAdapter : public TimeSourceClient {
 public:
  static scoped_ptr<LayerTreeHostImplTimeSourceAdapter> Create(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<DelayBasedTimeSource> time_source) {
    return make_scoped_ptr(
        new LayerTreeHostImplTimeSourceAdapter(layer_tree_host_impl,
                                               time_source));
  }
  ~LayerTreeHostImplTimeSourceAdapter() override {
    time_source_->SetClient(NULL);
    time_source_->SetActive(false);
  }

  void OnTimerTick() override {
    // In single threaded mode we attempt to simulate changing the current
    // thread by maintaining a fake thread id. When we switch from one
    // thread to another, we construct DebugScopedSetXXXThread objects that
    // update the thread id. This lets DCHECKS that ensure we're on the
    // right thread to work correctly in single threaded mode. The problem
    // here is that the timer tasks are run via the message loop, and when
    // they run, we've had no chance to construct a DebugScopedSetXXXThread
    // object. The result is that we report that we're running on the main
    // thread. In multi-threaded mode, this timer is run on the compositor
    // thread, so to keep this consistent in single-threaded mode, we'll
    // construct a DebugScopedSetImplThread object. There is no need to do
    // this in multi-threaded mode since the real thread id's will be
    // correct. In fact, setting fake thread id's interferes with the real
    // thread id's and causes breakage.
    scoped_ptr<DebugScopedSetImplThread> set_impl_thread;
    if (!layer_tree_host_impl_->proxy()->HasImplThread()) {
      set_impl_thread.reset(
          new DebugScopedSetImplThread(layer_tree_host_impl_->proxy()));
    }

    layer_tree_host_impl_->Animate(
        layer_tree_host_impl_->CurrentBeginFrameArgs().frame_time);
    layer_tree_host_impl_->UpdateBackgroundAnimateTicking(true);
    bool start_ready_animations = true;
    layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

    if (layer_tree_host_impl_->pending_tree()) {
      layer_tree_host_impl_->pending_tree()->UpdateDrawProperties();
      layer_tree_host_impl_->ManageTiles();
    }

    layer_tree_host_impl_->ResetCurrentBeginFrameArgsForNextFrame();
  }

  void SetActive(bool active) {
    if (active != time_source_->Active())
      time_source_->SetActive(active);
  }

  bool Active() const { return time_source_->Active(); }

 private:
  LayerTreeHostImplTimeSourceAdapter(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<DelayBasedTimeSource> time_source)
      : layer_tree_host_impl_(layer_tree_host_impl),
        time_source_(time_source) {
    time_source_->SetClient(this);
  }

  LayerTreeHostImpl* layer_tree_host_impl_;
  scoped_refptr<DelayBasedTimeSource> time_source_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImplTimeSourceAdapter);
};

LayerTreeHostImpl::FrameData::FrameData()
    : contains_incomplete_tile(false), has_no_damage(false) {}

LayerTreeHostImpl::FrameData::~FrameData() {}

scoped_ptr<LayerTreeHostImpl> LayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int id) {
  return make_scoped_ptr(new LayerTreeHostImpl(settings,
                                               client,
                                               proxy,
                                               rendering_stats_instrumentation,
                                               shared_bitmap_manager,
                                               gpu_memory_buffer_manager,
                                               id));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int id)
    : BeginFrameSourceMixIn(),
      client_(client),
      proxy_(proxy),
      use_gpu_rasterization_(false),
      input_handler_client_(NULL),
      did_lock_scrolling_layer_(false),
      should_bubble_scrolls_(false),
      wheel_scrolling_(false),
      scroll_affects_scroll_handler_(false),
      scroll_layer_id_when_mouse_over_scrollbar_(0),
      tile_priorities_dirty_(false),
      root_layer_scroll_offset_delegate_(NULL),
      settings_(settings),
      visible_(true),
      cached_managed_memory_policy_(
          PrioritizedResourceManager::DefaultMemoryAllocationLimit(),
          gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
          ManagedMemoryPolicy::kDefaultNumResourcesLimit),
      pinch_gesture_active_(false),
      pinch_gesture_end_should_clear_scrolling_layer_(false),
      fps_counter_(FrameRateCounter::Create(proxy_->HasImplThread())),
      paint_time_counter_(PaintTimeCounter::Create()),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      texture_mailbox_deleter_(new TextureMailboxDeleter(
          proxy_->HasImplThread() ? proxy_->ImplThreadTaskRunner()
                                  : proxy_->MainThreadTaskRunner())),
      max_memory_needed_bytes_(0),
      zero_budget_(false),
      device_scale_factor_(1.f),
      overhang_ui_resource_id_(0),
      resourceless_software_draw_(false),
      begin_impl_frame_interval_(BeginFrameArgs::DefaultInterval()),
      animation_registrar_(AnimationRegistrar::Create()),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      micro_benchmark_controller_(this),
      need_to_update_visible_tiles_before_draw_(false),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      id_(id),
      requires_high_res_to_draw_(false) {
  DCHECK(proxy_->IsImplThread());
  DidVisibilityChange(this, visible_);
  animation_registrar_->set_supports_scroll_animations(
      proxy_->SupportsImplScrolling());

  SetDebugState(settings.initial_debug_state);

  // LTHI always has an active tree.
  active_tree_ = LayerTreeImpl::create(this);
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerTreeHostImpl", id_);

  if (settings.calculate_top_controls_position) {
    top_controls_manager_ =
        TopControlsManager::Create(this,
                                   settings.top_controls_height,
                                   settings.top_controls_show_threshold,
                                   settings.top_controls_hide_threshold);

    // TODO(bokan): This is a quick fix. The browser should lock the top
    // controls to shown on creation but this appears not to work. Tracked
    // in crbug.com/417680.
    // Initialize with top controls showing.
    SetControlsTopOffset(0.f);
  }
}

LayerTreeHostImpl::~LayerTreeHostImpl() {
  DCHECK(proxy_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerTreeHostImpl", id_);

  if (input_handler_client_) {
    input_handler_client_->WillShutdown();
    input_handler_client_ = NULL;
  }

  // The layer trees must be destroyed before the layer tree host. We've
  // made a contract with our animation controllers that the registrar
  // will outlive them, and we must make good.
  if (recycle_tree_)
    recycle_tree_->Shutdown();
  if (pending_tree_)
    pending_tree_->Shutdown();
  active_tree_->Shutdown();
  recycle_tree_ = nullptr;
  pending_tree_ = nullptr;
  active_tree_ = nullptr;
  DestroyTileManager();
}

void LayerTreeHostImpl::BeginMainFrameAborted(bool did_handle) {
  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  if (did_handle) {
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit();
    active_tree_->ResetContentsTexturesPurged();
  }
}

void LayerTreeHostImpl::BeginCommit() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  if (UsePendingTreeForSync())
    CreatePendingTree();
}

void LayerTreeHostImpl::CommitComplete() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::CommitComplete");

  if (pending_tree_)
    pending_tree_->ApplyScrollDeltasSinceBeginMainFrame();
  sync_tree()->set_needs_update_draw_properties();

  if (settings_.impl_side_painting) {
    // Impl-side painting needs an update immediately post-commit to have the
    // opportunity to create tilings.  Other paths can call UpdateDrawProperties
    // more lazily when needed prior to drawing.
    sync_tree()->UpdateDrawProperties();
    // Start working on newly created tiles immediately if needed.
    if (tile_manager_ && tile_priorities_dirty_)
      ManageTiles();
    else
      NotifyReadyToActivate();
  } else {
    // If we're not in impl-side painting, the tree is immediately considered
    // active.
    ActivateSyncTree();
  }

  micro_benchmark_controller_.DidCompleteCommit();
}

bool LayerTreeHostImpl::CanDraw() const {
  // Note: If you are changing this function or any other function that might
  // affect the result of CanDraw, make sure to call
  // client_->OnCanDrawStateChanged in the proper places and update the
  // NotifyIfCanDrawChanged test.

  if (!renderer_) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no renderer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Must have an OutputSurface if |renderer_| is not NULL.
  DCHECK(output_surface_);

  // TODO(boliu): Make draws without root_layer work and move this below
  // draw_and_swap_full_viewport_every_frame check. Tracked in crbug.com/264967.
  if (!active_tree_->root_layer()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no root layer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (output_surface_->capabilities().draw_and_swap_full_viewport_every_frame)
    return true;

  if (DrawViewportSize().IsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw empty viewport",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (active_tree_->ViewportSizeInvalid()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw viewport size recently changed",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (active_tree_->ContentsTexturesPurged()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw contents textures purged",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (EvictedUIResourcesExist()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw UI resources evicted not recreated",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

void LayerTreeHostImpl::Animate(base::TimeTicks monotonic_time) {
  if (input_handler_client_)
    input_handler_client_->Animate(monotonic_time);
  AnimatePageScale(monotonic_time);
  AnimateLayers(monotonic_time);
  AnimateScrollbars(monotonic_time);
  AnimateTopControls(monotonic_time);
}

void LayerTreeHostImpl::ManageTiles() {
  if (!tile_manager_)
    return;
  if (!tile_priorities_dirty_)
    return;

  tile_priorities_dirty_ = false;
  tile_manager_->ManageTiles(global_tile_state_);

  client_->DidManageTiles();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingLayerAt(
    const gfx::Point& viewport_point,
    InputHandler::ScrollInputType type) {
  if (!CurrentlyScrollingLayer())
    return false;

  gfx::PointF device_viewport_point =
      gfx::ScalePoint(viewport_point, device_scale_factor_);

  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  bool scroll_on_main_thread = false;
  LayerImpl* scrolling_layer_impl = FindScrollLayerForDeviceViewportPoint(
      device_viewport_point, type, layer_impl, &scroll_on_main_thread, NULL);
  return CurrentlyScrollingLayer() == scrolling_layer_impl;
}

bool LayerTreeHostImpl::HaveTouchEventHandlersAt(
    const gfx::Point& viewport_point) {

  gfx::PointF device_viewport_point =
      gfx::ScalePoint(viewport_point, device_scale_factor_);

  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPointInTouchHandlerRegion(
          device_viewport_point);

  return layer_impl != NULL;
}

scoped_ptr<SwapPromiseMonitor>
LayerTreeHostImpl::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return make_scoped_ptr(
      new LatencyInfoSwapPromiseMonitor(latency, NULL, this));
}

void LayerTreeHostImpl::QueueSwapPromiseForMainThreadScrollUpdate(
    scoped_ptr<SwapPromise> swap_promise) {
  swap_promises_for_main_thread_scroll_update_.push_back(swap_promise.Pass());
}

void LayerTreeHostImpl::TrackDamageForAllSurfaces(
    LayerImpl* root_draw_layer,
    const LayerImplList& render_surface_layer_list) {
  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.

  for (int surface_index = render_surface_layer_list.size() - 1;
       surface_index >= 0;
       --surface_index) {
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);
    render_surface->damage_tracker()->UpdateDamageTrackingState(
        render_surface->layer_list(),
        render_surface_layer->id(),
        render_surface->SurfacePropertyChangedOnlyFromDescendant(),
        render_surface->content_rect(),
        render_surface_layer->mask_layer(),
        render_surface_layer->filters());
  }
}

void LayerTreeHostImpl::FrameData::AsValueInto(
    base::debug::TracedValue* value) const {
  value->SetBoolean("contains_incomplete_tile", contains_incomplete_tile);
  value->SetBoolean("has_no_damage", has_no_damage);

  // Quad data can be quite large, so only dump render passes if we select
  // cc.debug.quads.
  bool quads_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"), &quads_enabled);
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (size_t i = 0; i < render_passes.size(); ++i) {
      value->BeginDictionary();
      render_passes[i]->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

void LayerTreeHostImpl::FrameData::AppendRenderPass(
    scoped_ptr<RenderPass> render_pass) {
  render_passes_by_id[render_pass->id] = render_pass.get();
  render_passes.push_back(render_pass.Pass());
}

DrawMode LayerTreeHostImpl::GetDrawMode() const {
  if (resourceless_software_draw_) {
    return DRAW_MODE_RESOURCELESS_SOFTWARE;
  } else if (output_surface_->context_provider()) {
    return DRAW_MODE_HARDWARE;
  } else {
    DCHECK_EQ(!output_surface_->software_device(),
              output_surface_->capabilities().delegated_rendering &&
                  !output_surface_->capabilities().deferred_gl_initialization)
        << output_surface_->capabilities().delegated_rendering << " "
        << output_surface_->capabilities().deferred_gl_initialization;
    return DRAW_MODE_SOFTWARE;
  }
}

static void AppendQuadsForLayer(
    RenderPass* target_render_pass,
    LayerImpl* layer,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  layer->AppendQuads(
      target_render_pass,
      occlusion_tracker.GetCurrentOcclusionForLayer(layer->draw_transform()),
      append_quads_data);
}

static void AppendQuadsForRenderSurfaceLayer(
    RenderPass* target_render_pass,
    LayerImpl* layer,
    const RenderPass* contributing_render_pass,
    const OcclusionTracker<LayerImpl>& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  bool is_replica = false;
  layer->render_surface()->AppendQuads(target_render_pass,
                                       occlusion_tracker,
                                       append_quads_data,
                                       is_replica,
                                       contributing_render_pass->id);

  // Add replica after the surface so that it appears below the surface.
  if (layer->has_replica()) {
    is_replica = true;
    layer->render_surface()->AppendQuads(target_render_pass,
                                         occlusion_tracker,
                                         append_quads_data,
                                         is_replica,
                                         contributing_render_pass->id);
  }
}

static void AppendQuadsToFillScreen(
    ResourceProvider::ResourceId overhang_resource_id,
    const gfx::SizeF& overhang_resource_scaled_size,
    const gfx::Rect& root_scroll_layer_rect,
    RenderPass* target_render_pass,
    LayerImpl* root_layer,
    SkColor screen_background_color,
    const OcclusionTracker<LayerImpl>& occlusion_tracker) {
  if (!root_layer || !SkColorGetA(screen_background_color))
    return;

  Region fill_region = occlusion_tracker.ComputeVisibleRegionInScreen();
  if (fill_region.IsEmpty())
    return;

  // Divide the fill region into the part to be filled with the overhang
  // resource and the part to be filled with the background color.
  Region screen_background_color_region = fill_region;
  Region overhang_region;
  if (overhang_resource_id) {
    overhang_region = fill_region;
    overhang_region.Subtract(root_scroll_layer_rect);
    screen_background_color_region.Intersect(root_scroll_layer_rect);
  }

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  gfx::Rect root_target_rect = root_layer->render_surface()->content_rect();
  float opacity = 1.f;
  int sorting_context_id = 0;
  SharedQuadState* shared_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(),
                            root_target_rect.size(),
                            root_target_rect,
                            root_target_rect,
                            false,
                            opacity,
                            SkXfermode::kSrcOver_Mode,
                            sorting_context_id);

  for (Region::Iterator fill_rects(screen_background_color_region);
       fill_rects.has_rect();
       fill_rects.next()) {
    gfx::Rect screen_space_rect = fill_rects.rect();
    gfx::Rect visible_screen_space_rect = screen_space_rect;
    // Skip the quad culler and just append the quads directly to avoid
    // occlusion checks.
    SolidColorDrawQuad* quad =
        target_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state,
                 screen_space_rect,
                 visible_screen_space_rect,
                 screen_background_color,
                 false);
  }
  for (Region::Iterator fill_rects(overhang_region);
       fill_rects.has_rect();
       fill_rects.next()) {
    DCHECK(overhang_resource_id);
    gfx::Rect screen_space_rect = fill_rects.rect();
    gfx::Rect opaque_screen_space_rect = screen_space_rect;
    gfx::Rect visible_screen_space_rect = screen_space_rect;
    TextureDrawQuad* tex_quad =
        target_render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    const float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
    tex_quad->SetNew(
        shared_quad_state,
        screen_space_rect,
        opaque_screen_space_rect,
        visible_screen_space_rect,
        overhang_resource_id,
        false,
        gfx::PointF(
            screen_space_rect.x() / overhang_resource_scaled_size.width(),
            screen_space_rect.y() / overhang_resource_scaled_size.height()),
        gfx::PointF(
            screen_space_rect.right() / overhang_resource_scaled_size.width(),
            screen_space_rect.bottom() /
                overhang_resource_scaled_size.height()),
        screen_background_color,
        vertex_opacity,
        false);
  }
}

DrawResult LayerTreeHostImpl::CalculateRenderPasses(
    FrameData* frame) {
  DCHECK(frame->render_passes.empty());
  DCHECK(CanDraw());
  DCHECK(active_tree_->root_layer());

  TrackDamageForAllSurfaces(active_tree_->root_layer(),
                            *frame->render_surface_layer_list);

  // If the root render surface has no visible damage, then don't generate a
  // frame at all.
  RenderSurfaceImpl* root_surface =
      active_tree_->root_layer()->render_surface();
  bool root_surface_has_no_visible_damage =
      !root_surface->damage_tracker()->current_damage_rect().Intersects(
          root_surface->content_rect());
  bool root_surface_has_contributing_layers =
      !root_surface->layer_list().empty();
  bool hud_wants_to_draw_ = active_tree_->hud_layer() &&
                            active_tree_->hud_layer()->IsAnimatingHUDContents();
  if (root_surface_has_contributing_layers &&
      root_surface_has_no_visible_damage &&
      active_tree_->LayersWithCopyOutputRequest().empty() &&
      !hud_wants_to_draw_) {
    TRACE_EVENT0("cc",
                 "LayerTreeHostImpl::CalculateRenderPasses::EmptyDamageRect");
    frame->has_no_damage = true;
    DCHECK(!output_surface_->capabilities()
               .draw_and_swap_full_viewport_every_frame);
    return DRAW_SUCCESS;
  }

  TRACE_EVENT1("cc",
               "LayerTreeHostImpl::CalculateRenderPasses",
               "render_surface_layer_list.size()",
               static_cast<uint64>(frame->render_surface_layer_list->size()));

  // Create the render passes in dependency order.
  for (int surface_index = frame->render_surface_layer_list->size() - 1;
       surface_index >= 0;
       --surface_index) {
    LayerImpl* render_surface_layer =
        (*frame->render_surface_layer_list)[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();

    bool should_draw_into_render_pass =
        render_surface_layer->parent() == NULL ||
        render_surface->contributes_to_drawn_surface() ||
        render_surface_layer->HasCopyRequest();
    if (should_draw_into_render_pass)
      render_surface_layer->render_surface()->AppendRenderPasses(frame);
  }

  // When we are displaying the HUD, change the root damage rect to cover the
  // entire root surface. This will disable partial-swap/scissor optimizations
  // that would prevent the HUD from updating, since the HUD does not cause
  // damage itself, to prevent it from messing with damage visualizations. Since
  // damage visualizations are done off the LayerImpls and RenderSurfaceImpls,
  // changing the RenderPass does not affect them.
  if (active_tree_->hud_layer()) {
    RenderPass* root_pass = frame->render_passes.back();
    root_pass->damage_rect = root_pass->output_rect;
  }

  OcclusionTracker<LayerImpl> occlusion_tracker(
      active_tree_->root_layer()->render_surface()->content_rect());
  occlusion_tracker.set_minimum_tracking_size(
      settings_.minimum_occlusion_tracking_size);

  if (debug_state_.show_occluding_rects) {
    occlusion_tracker.set_occluding_screen_space_rects_container(
        &frame->occluding_screen_space_rects);
  }
  if (debug_state_.show_non_occluding_rects) {
    occlusion_tracker.set_non_occluding_screen_space_rects_container(
        &frame->non_occluding_screen_space_rects);
  }

  // Add quads to the Render passes in front-to-back order to allow for testing
  // occlusion and performing culling during the tree walk.
  typedef LayerIterator<LayerImpl> LayerIteratorType;

  // Typically when we are missing a texture and use a checkerboard quad, we
  // still draw the frame. However when the layer being checkerboarded is moving
  // due to an impl-animation, we drop the frame to avoid flashing due to the
  // texture suddenly appearing in the future.
  DrawResult draw_result = DRAW_SUCCESS;
  // When we have a copy request for a layer, we need to draw no matter
  // what, as the layer may disappear after this frame.
  bool have_copy_request = false;

  int layers_drawn = 0;

  const DrawMode draw_mode = GetDrawMode();

  int num_missing_tiles = 0;
  int num_incomplete_tiles = 0;

  LayerIteratorType end =
      LayerIteratorType::End(frame->render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(frame->render_surface_layer_list);
       it != end;
       ++it) {
    RenderPassId target_render_pass_id =
        it.target_render_surface_layer()->render_surface()->GetRenderPassId();
    RenderPass* target_render_pass =
        frame->render_passes_by_id[target_render_pass_id];

    occlusion_tracker.EnterLayer(it);

    AppendQuadsData append_quads_data(target_render_pass_id);

    if (it.represents_target_render_surface()) {
      if (it->HasCopyRequest()) {
        have_copy_request = true;
        it->TakeCopyRequestsAndTransformToTarget(
            &target_render_pass->copy_requests);
      }
    } else if (it.represents_contributing_render_surface() &&
               it->render_surface()->contributes_to_drawn_surface()) {
      RenderPassId contributing_render_pass_id =
          it->render_surface()->GetRenderPassId();
      RenderPass* contributing_render_pass =
          frame->render_passes_by_id[contributing_render_pass_id];
      AppendQuadsForRenderSurfaceLayer(target_render_pass,
                                       *it,
                                       contributing_render_pass,
                                       occlusion_tracker,
                                       &append_quads_data);
    } else if (it.represents_itself() &&
               !it->visible_content_rect().IsEmpty()) {
      bool occluded =
          occlusion_tracker.GetCurrentOcclusionForLayer(it->draw_transform())
              .IsOccluded(it->visible_content_rect());
      if (!occluded && it->WillDraw(draw_mode, resource_provider_.get())) {
        DCHECK_EQ(active_tree_, it->layer_tree_impl());

        frame->will_draw_layers.push_back(*it);

        if (it->HasContributingDelegatedRenderPasses()) {
          RenderPassId contributing_render_pass_id =
              it->FirstContributingRenderPassId();
          while (frame->render_passes_by_id.find(contributing_render_pass_id) !=
                 frame->render_passes_by_id.end()) {
            RenderPass* render_pass =
                frame->render_passes_by_id[contributing_render_pass_id];

            AppendQuadsData append_quads_data(render_pass->id);
            AppendQuadsForLayer(render_pass,
                                *it,
                                occlusion_tracker,
                                &append_quads_data);

            contributing_render_pass_id =
                it->NextContributingRenderPassId(contributing_render_pass_id);
          }
        }

        AppendQuadsForLayer(target_render_pass,
                            *it,
                            occlusion_tracker,
                            &append_quads_data);
      }

      ++layers_drawn;
    }

    rendering_stats_instrumentation_->AddVisibleContentArea(
        append_quads_data.visible_content_area);
    rendering_stats_instrumentation_->AddApproximatedVisibleContentArea(
        append_quads_data.approximated_visible_content_area);

    num_missing_tiles += append_quads_data.num_missing_tiles;
    num_incomplete_tiles += append_quads_data.num_incomplete_tiles;

    if (append_quads_data.num_missing_tiles) {
      bool layer_has_animating_transform =
          it->screen_space_transform_is_animating() ||
          it->draw_transform_is_animating();
      if (layer_has_animating_transform)
        draw_result = DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
    }

    if (append_quads_data.num_incomplete_tiles ||
        append_quads_data.num_missing_tiles) {
      frame->contains_incomplete_tile = true;
      if (RequiresHighResToDraw())
        draw_result = DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
    }

    occlusion_tracker.LeaveLayer(it);
  }

  if (have_copy_request ||
      output_surface_->capabilities().draw_and_swap_full_viewport_every_frame)
    draw_result = DRAW_SUCCESS;

#if DCHECK_IS_ON
  for (const auto& render_pass : frame->render_passes) {
    for (const auto& quad : render_pass->quad_list)
      DCHECK(quad->shared_quad_state);
    DCHECK(frame->render_passes_by_id.find(render_pass->id) !=
           frame->render_passes_by_id.end());
  }
#endif
  DCHECK(frame->render_passes.back()->output_rect.origin().IsOrigin());

  if (!active_tree_->has_transparent_background()) {
    frame->render_passes.back()->has_transparent_background = false;
    AppendQuadsToFillScreen(
        ResourceIdForUIResource(overhang_ui_resource_id_),
        gfx::ScaleSize(overhang_ui_resource_size_, device_scale_factor_),
        active_tree_->RootScrollLayerDeviceViewportBounds(),
        frame->render_passes.back(),
        active_tree_->root_layer(),
        active_tree_->background_color(),
        occlusion_tracker);
  }

  RemoveRenderPasses(CullRenderPassesWithNoQuads(), frame);
  renderer_->DecideRenderPassAllocationsForFrame(frame->render_passes);

  // Any copy requests left in the tree are not going to get serviced, and
  // should be aborted.
  ScopedPtrVector<CopyOutputRequest> requests_to_abort;
  while (!active_tree_->LayersWithCopyOutputRequest().empty()) {
    LayerImpl* layer = active_tree_->LayersWithCopyOutputRequest().back();
    layer->TakeCopyRequestsAndTransformToTarget(&requests_to_abort);
  }
  for (size_t i = 0; i < requests_to_abort.size(); ++i)
    requests_to_abort[i]->SendEmptyResult();

  // If we're making a frame to draw, it better have at least one render pass.
  DCHECK(!frame->render_passes.empty());

  if (active_tree_->has_ever_been_drawn()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumMissingTiles",
        num_missing_tiles);
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumIncompleteTiles",
        num_incomplete_tiles);
  }

  // Should only have one render pass in resourceless software mode.
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE ||
         frame->render_passes.size() == 1u)
      << frame->render_passes.size();

  return draw_result;
}

void LayerTreeHostImpl::MainThreadHasStoppedFlinging() {
  if (input_handler_client_)
    input_handler_client_->MainThreadHasStoppedFlinging();
}

void LayerTreeHostImpl::UpdateBackgroundAnimateTicking(
    bool should_background_tick) {
  DCHECK(proxy_->IsImplThread());
  if (should_background_tick)
    DCHECK(active_tree_->root_layer());

  bool enabled = should_background_tick && needs_animate_layers();

  // Lazily create the time_source adapter so that we can vary the interval for
  // testing.
  if (!time_source_client_adapter_) {
    time_source_client_adapter_ = LayerTreeHostImplTimeSourceAdapter::Create(
        this,
        DelayBasedTimeSource::Create(
            LowFrequencyAnimationInterval(),
            proxy_->HasImplThread() ? proxy_->ImplThreadTaskRunner()
                                    : proxy_->MainThreadTaskRunner()));
  }

  time_source_client_adapter_->SetActive(enabled);
}

void LayerTreeHostImpl::DidAnimateScrollOffset() {
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetViewportDamage(const gfx::Rect& damage_rect) {
  viewport_damage_rect_.Union(damage_rect);
}

static inline RenderPass* FindRenderPassById(
    RenderPassId render_pass_id,
    const LayerTreeHostImpl::FrameData& frame) {
  RenderPassIdHashMap::const_iterator it =
      frame.render_passes_by_id.find(render_pass_id);
  return it != frame.render_passes_by_id.end() ? it->second : NULL;
}

static void RemoveRenderPassesRecursive(RenderPassId remove_render_pass_id,
                                        LayerTreeHostImpl::FrameData* frame) {
  RenderPass* remove_render_pass =
      FindRenderPassById(remove_render_pass_id, *frame);
  // The pass was already removed by another quad - probably the original, and
  // we are the replica.
  if (!remove_render_pass)
    return;
  RenderPassList& render_passes = frame->render_passes;
  RenderPassList::iterator to_remove = std::find(render_passes.begin(),
                                                 render_passes.end(),
                                                 remove_render_pass);

  DCHECK(to_remove != render_passes.end());

  scoped_ptr<RenderPass> removed_pass = render_passes.take(to_remove);
  frame->render_passes.erase(to_remove);
  frame->render_passes_by_id.erase(remove_render_pass_id);

  // Now follow up for all RenderPass quads and remove their RenderPasses
  // recursively.
  const QuadList& quad_list = removed_pass->quad_list;
  for (auto quad_list_iterator = quad_list.BackToFrontBegin();
       quad_list_iterator != quad_list.BackToFrontEnd();
       ++quad_list_iterator) {
    const DrawQuad* current_quad = *quad_list_iterator;
    if (current_quad->material != DrawQuad::RENDER_PASS)
      continue;

    RenderPassId next_remove_render_pass_id =
        RenderPassDrawQuad::MaterialCast(current_quad)->render_pass_id;
    RemoveRenderPassesRecursive(next_remove_render_pass_id, frame);
  }
}

bool LayerTreeHostImpl::CullRenderPassesWithNoQuads::ShouldRemoveRenderPass(
    const RenderPassDrawQuad& quad, const FrameData& frame) const {
  const RenderPass* render_pass =
      FindRenderPassById(quad.render_pass_id, frame);
  if (!render_pass)
    return false;

  // If any quad or RenderPass draws into this RenderPass, then keep it.
  const QuadList& quad_list = render_pass->quad_list;
  for (auto quad_list_iterator = quad_list.BackToFrontBegin();
       quad_list_iterator != quad_list.BackToFrontEnd();
       ++quad_list_iterator) {
    const DrawQuad* current_quad = *quad_list_iterator;

    if (current_quad->material != DrawQuad::RENDER_PASS)
      return false;

    const RenderPass* contributing_pass = FindRenderPassById(
        RenderPassDrawQuad::MaterialCast(current_quad)->render_pass_id, frame);
    if (contributing_pass)
      return false;
  }
  return true;
}

// Defined for linking tests.
template CC_EXPORT void LayerTreeHostImpl::RemoveRenderPasses<
  LayerTreeHostImpl::CullRenderPassesWithNoQuads>(
      CullRenderPassesWithNoQuads culler, FrameData*);

// static
template <typename RenderPassCuller>
void LayerTreeHostImpl::RemoveRenderPasses(RenderPassCuller culler,
                                           FrameData* frame) {
  for (size_t it = culler.RenderPassListBegin(frame->render_passes);
       it != culler.RenderPassListEnd(frame->render_passes);
       it = culler.RenderPassListNext(it)) {
    const RenderPass* current_pass = frame->render_passes[it];
    const QuadList& quad_list = current_pass->quad_list;

    for (auto quad_list_iterator = quad_list.BackToFrontBegin();
         quad_list_iterator != quad_list.BackToFrontEnd();
         ++quad_list_iterator) {
      const DrawQuad* current_quad = *quad_list_iterator;

      if (current_quad->material != DrawQuad::RENDER_PASS)
        continue;

      const RenderPassDrawQuad* render_pass_quad =
          RenderPassDrawQuad::MaterialCast(current_quad);
      if (!culler.ShouldRemoveRenderPass(*render_pass_quad, *frame))
        continue;

      // We are changing the vector in the middle of iteration. Because we
      // delete render passes that draw into the current pass, we are
      // guaranteed that any data from the iterator to the end will not
      // change. So, capture the iterator position from the end of the
      // list, and restore it after the change.
      size_t position_from_end = frame->render_passes.size() - it;
      RemoveRenderPassesRecursive(render_pass_quad->render_pass_id, frame);
      it = frame->render_passes.size() - position_from_end;
      DCHECK_GE(frame->render_passes.size(), position_from_end);
    }
  }
}

DrawResult LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  TRACE_EVENT1("cc",
               "LayerTreeHostImpl::PrepareToDraw",
               "SourceFrameNumber",
               active_tree_->source_frame_number());

  if (need_to_update_visible_tiles_before_draw_ &&
      tile_manager_ && tile_manager_->UpdateVisibleTiles()) {
    DidInitializeVisibleTile();
  }
  need_to_update_visible_tiles_before_draw_ = true;

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Compositing.NumActiveLayers", active_tree_->NumLayers(), 1, 400, 20);

  bool ok = active_tree_->UpdateDrawProperties();
  DCHECK(ok) << "UpdateDrawProperties failed during draw";

  frame->render_surface_layer_list = &active_tree_->RenderSurfaceLayerList();
  frame->render_passes.clear();
  frame->render_passes_by_id.clear();
  frame->will_draw_layers.clear();
  frame->contains_incomplete_tile = false;
  frame->has_no_damage = false;

  if (active_tree_->root_layer()) {
    gfx::Rect device_viewport_damage_rect = viewport_damage_rect_;
    viewport_damage_rect_ = gfx::Rect();

    active_tree_->root_layer()->render_surface()->damage_tracker()->
        AddDamageNextUpdate(device_viewport_damage_rect);
  }

  DrawResult draw_result = CalculateRenderPasses(frame);
  if (draw_result != DRAW_SUCCESS) {
    DCHECK(!output_surface_->capabilities()
               .draw_and_swap_full_viewport_every_frame);
    return draw_result;
  }

  // If we return DRAW_SUCCESS, then we expect DrawLayers() to be called before
  // this function is called again.
  return draw_result;
}

void LayerTreeHostImpl::EvictTexturesForTesting() {
  EnforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
}

void LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting(bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::DidInitializeVisibleTileForTesting() {
  // Add arbitrary damage, to trigger prepare-to-draws.
  // Here, setting damage as viewport size, used only for testing.
  SetFullRootLayerDamage();
  DidInitializeVisibleTile();
}

void LayerTreeHostImpl::ResetTreesForTesting() {
  if (active_tree_)
    active_tree_->DetachLayerTree();
  active_tree_ = LayerTreeImpl::create(this);
  if (pending_tree_)
    pending_tree_->DetachLayerTree();
  pending_tree_ = nullptr;
  if (recycle_tree_)
    recycle_tree_->DetachLayerTree();
  recycle_tree_ = nullptr;
}

void LayerTreeHostImpl::EnforceManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {

  bool evicted_resources = client_->ReduceContentsTextureMemoryOnImplThread(
      visible_ ? policy.bytes_limit_when_visible : 0,
      ManagedMemoryPolicy::PriorityCutoffToValue(
          visible_ ? policy.priority_cutoff_when_visible
                   : gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING));
  if (evicted_resources) {
    active_tree_->SetContentsTexturesPurged();
    if (pending_tree_)
      pending_tree_->SetContentsTexturesPurged();
    client_->SetNeedsCommitOnImplThread();
    client_->OnCanDrawStateChanged(CanDraw());
    client_->RenewTreePriority();
  }

  UpdateTileManagerMemoryPolicy(policy);
}

void LayerTreeHostImpl::UpdateTileManagerMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (!tile_manager_)
    return;

  global_tile_state_.hard_memory_limit_in_bytes = 0;
  global_tile_state_.soft_memory_limit_in_bytes = 0;
  if (visible_ && policy.bytes_limit_when_visible > 0) {
    global_tile_state_.hard_memory_limit_in_bytes =
        policy.bytes_limit_when_visible;
    global_tile_state_.soft_memory_limit_in_bytes =
        (static_cast<int64>(global_tile_state_.hard_memory_limit_in_bytes) *
         settings_.max_memory_for_prepaint_percentage) /
        100;
  }
  global_tile_state_.memory_limit_policy =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          visible_ ?
          policy.priority_cutoff_when_visible :
          gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  global_tile_state_.num_resources_limit = policy.num_resources_limit;

  // TODO(reveman): We should avoid keeping around unused resources if
  // possible. crbug.com/224475
  // Unused limit is calculated from soft-limit, as hard-limit may
  // be very high and shouldn't typically be exceeded.
  size_t unused_memory_limit_in_bytes = static_cast<size_t>(
      (static_cast<int64>(global_tile_state_.soft_memory_limit_in_bytes) *
       settings_.max_unused_resource_memory_percentage) /
      100);

  DCHECK(resource_pool_);
  resource_pool_->CheckBusyResources(false);
  // Soft limit is used for resource pool such that memory returns to soft
  // limit after going over.
  resource_pool_->SetResourceUsageLimits(
      global_tile_state_.soft_memory_limit_in_bytes,
      unused_memory_limit_in_bytes,
      global_tile_state_.num_resources_limit);

  // Release all staging resources when invisible.
  if (staging_resource_pool_) {
    staging_resource_pool_->CheckBusyResources(false);
    staging_resource_pool_->SetResourceUsageLimits(
        std::numeric_limits<size_t>::max(),
        std::numeric_limits<size_t>::max(),
        visible_ ? GetMaxStagingResourceCount() : 0);
  }

  DidModifyTilePriorities();
}

void LayerTreeHostImpl::DidModifyTilePriorities() {
  DCHECK(settings_.impl_side_painting);
  // Mark priorities as dirty and schedule a ManageTiles().
  tile_priorities_dirty_ = true;
  client_->SetNeedsManageTilesOnImplThread();
}

void LayerTreeHostImpl::DidInitializeVisibleTile() {
  if (client_ && !client_->IsInsideDraw())
    client_->DidInitializeVisibleTileOnImplThread();
}

void LayerTreeHostImpl::GetPictureLayerImplPairs(
    std::vector<PictureLayerImpl::Pair>* layer_pairs) const {
  DCHECK(layer_pairs->empty());
  for (std::vector<PictureLayerImpl*>::const_iterator it =
           picture_layers_.begin();
       it != picture_layers_.end();
       ++it) {
    PictureLayerImpl* layer = *it;

    // TODO(vmpstr): Iterators and should handle this instead. crbug.com/381704
    if (!layer->HasValidTilePriorities())
      continue;

    PictureLayerImpl* twin_layer = layer->GetPendingOrActiveTwinLayer();

    // Ignore the twin layer when tile priorities are invalid.
    // TODO(vmpstr): Iterators should handle this instead. crbug.com/381704
    if (twin_layer && !twin_layer->HasValidTilePriorities())
      twin_layer = NULL;

    // If the current tree is ACTIVE_TREE, then always generate a layer_pair.
    // If current tree is PENDING_TREE, then only generate a layer_pair if
    // there is no twin layer.
    if (layer->GetTree() == ACTIVE_TREE) {
      DCHECK(!twin_layer || twin_layer->GetTree() == PENDING_TREE);
      layer_pairs->push_back(PictureLayerImpl::Pair(layer, twin_layer));
    } else if (!twin_layer) {
      layer_pairs->push_back(PictureLayerImpl::Pair(NULL, layer));
    }
  }
}

void LayerTreeHostImpl::BuildRasterQueue(RasterTilePriorityQueue* queue,
                                         TreePriority tree_priority) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BuildRasterQueue");
  picture_layer_pairs_.clear();
  GetPictureLayerImplPairs(&picture_layer_pairs_);
  queue->Build(picture_layer_pairs_, tree_priority);
}

void LayerTreeHostImpl::BuildEvictionQueue(EvictionTilePriorityQueue* queue,
                                           TreePriority tree_priority) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BuildEvictionQueue");
  picture_layer_pairs_.clear();
  GetPictureLayerImplPairs(&picture_layer_pairs_);
  queue->Build(picture_layer_pairs_, tree_priority);
}

const std::vector<PictureLayerImpl*>& LayerTreeHostImpl::GetPictureLayers()
    const {
  return picture_layers_;
}

void LayerTreeHostImpl::NotifyReadyToActivate() {
  client_->NotifyReadyToActivate();
}

void LayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::NotifyTileStateChanged");

  if (active_tree_) {
    LayerImpl* layer_impl =
        active_tree_->FindActiveTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }

  if (pending_tree_) {
    LayerImpl* layer_impl =
        pending_tree_->FindPendingTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }
}

void LayerTreeHostImpl::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  SetManagedMemoryPolicy(policy, zero_budget_);
}

void LayerTreeHostImpl::SetTreeActivationCallback(
    const base::Closure& callback) {
  DCHECK(proxy_->IsImplThread());
  DCHECK(settings_.impl_side_painting || callback.is_null());
  tree_activation_callback_ = callback;
}

void LayerTreeHostImpl::SetManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy, bool zero_budget) {
  if (cached_managed_memory_policy_ == policy && zero_budget_ == zero_budget)
    return;

  ManagedMemoryPolicy old_policy = ActualManagedMemoryPolicy();

  cached_managed_memory_policy_ = policy;
  zero_budget_ = zero_budget;
  ManagedMemoryPolicy actual_policy = ActualManagedMemoryPolicy();

  if (old_policy == actual_policy)
    return;

  if (!proxy_->HasImplThread()) {
    // In single-thread mode, this can be called on the main thread by
    // GLRenderer::OnMemoryAllocationChanged.
    DebugScopedSetImplThread impl_thread(proxy_);
    EnforceManagedMemoryPolicy(actual_policy);
  } else {
    DCHECK(proxy_->IsImplThread());
    EnforceManagedMemoryPolicy(actual_policy);
  }

  // If there is already enough memory to draw everything imaginable and the
  // new memory limit does not change this, then do not re-commit. Don't bother
  // skipping commits if this is not visible (commits don't happen when not
  // visible, there will almost always be a commit when this becomes visible).
  bool needs_commit = true;
  if (visible() &&
      actual_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      old_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      actual_policy.priority_cutoff_when_visible ==
          old_policy.priority_cutoff_when_visible) {
    needs_commit = false;
  }

  if (needs_commit)
    client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::SetExternalDrawConstraints(
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority,
    bool resourceless_software_draw) {
  gfx::Rect viewport_rect_for_tile_priority_in_view_space;
  if (!resourceless_software_draw) {
    gfx::Transform screen_to_view(gfx::Transform::kSkipInitialization);
    if (transform_for_tile_priority.GetInverse(&screen_to_view)) {
      // Convert from screen space to view space.
      viewport_rect_for_tile_priority_in_view_space =
          gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
              screen_to_view, viewport_rect_for_tile_priority));
    }
  }

  if (external_transform_ != transform || external_viewport_ != viewport ||
      resourceless_software_draw_ != resourceless_software_draw ||
      viewport_rect_for_tile_priority_ !=
          viewport_rect_for_tile_priority_in_view_space) {
    active_tree_->set_needs_update_draw_properties();
  }

  external_transform_ = transform;
  external_viewport_ = viewport;
  external_clip_ = clip;
  viewport_rect_for_tile_priority_ =
      viewport_rect_for_tile_priority_in_view_space;
  resourceless_software_draw_ = resourceless_software_draw;
}

void LayerTreeHostImpl::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  if (damage_rect.IsEmpty())
    return;
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsRedrawRectOnImplThread(damage_rect);
}

void LayerTreeHostImpl::BeginFrame(const BeginFrameArgs& args) {
  CallOnBeginFrame(args);
}

void LayerTreeHostImpl::DidSwapBuffers() {
  client_->DidSwapBuffersOnImplThread();
}

void LayerTreeHostImpl::DidSwapBuffersComplete() {
  client_->DidSwapBuffersCompleteOnImplThread();
}

void LayerTreeHostImpl::ReclaimResources(const CompositorFrameAck* ack) {
  // TODO(piman): We may need to do some validation on this ack before
  // processing it.
  if (renderer_)
    renderer_->ReceiveSwapBuffersAck(*ack);

  // In OOM, we now might be able to release more resources that were held
  // because they were exported.
  if (tile_manager_) {
    DCHECK(resource_pool_);

    resource_pool_->CheckBusyResources(false);
    resource_pool_->ReduceResourceUsage();
  }
  // If we're not visible, we likely released resources, so we want to
  // aggressively flush here to make sure those DeleteTextures make it to the
  // GPU process to free up the memory.
  if (output_surface_->context_provider() && !visible_) {
    output_surface_->context_provider()->ContextGL()->ShallowFlushCHROMIUM();
  }
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() const {
  CompositorFrameMetadata metadata;
  metadata.device_scale_factor = device_scale_factor_;
  metadata.page_scale_factor = active_tree_->total_page_scale_factor();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  if (top_controls_manager_) {
    metadata.location_bar_offset =
        gfx::Vector2dF(0.f, top_controls_manager_->ControlsTopOffset());
    metadata.location_bar_content_translation =
        gfx::Vector2dF(0.f, top_controls_manager_->ContentTopOffset());
  }

  active_tree_->GetViewportSelection(&metadata.selection_start,
                                     &metadata.selection_end);

  if (!InnerViewportScrollLayer())
    return metadata;

  // TODO(miletus) : Change the metadata to hold ScrollOffset.
  metadata.root_scroll_offset = gfx::ScrollOffsetToVector2dF(
      active_tree_->TotalScrollOffset());

  return metadata;
}

static void LayerTreeHostImplDidBeginTracingCallback(LayerImpl* layer) {
  layer->DidBeginTracing();
}

void LayerTreeHostImpl::DrawLayers(FrameData* frame,
                                   base::TimeTicks frame_begin_time) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::DrawLayers");
  DCHECK(CanDraw());

  if (frame->has_no_damage) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoDamage", TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!output_surface_->capabilities()
               .draw_and_swap_full_viewport_every_frame);
    return;
  }

  DCHECK(!frame->render_passes.empty());

  fps_counter_->SaveTimeStamp(frame_begin_time,
                              !output_surface_->context_provider());
  rendering_stats_instrumentation_->IncrementFrameCount(1);

  if (tile_manager_) {
    memory_history_->SaveEntry(
        tile_manager_->memory_stats_from_last_assign());
  }

  if (debug_state_.ShowHudRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree_->root_layer(),
        active_tree_->hud_layer(),
        *frame->render_surface_layer_list,
        frame->occluding_screen_space_rects,
        frame->non_occluding_screen_space_rects,
        debug_state_);
  }

  if (!settings_.impl_side_painting && debug_state_.continuous_painting) {
    const RenderingStats& stats =
        rendering_stats_instrumentation_->GetRenderingStats();
    paint_time_counter_->SavePaintTime(stats.main_stats.paint_time);
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace) {
    if (pending_tree_) {
      LayerTreeHostCommon::CallFunctionForSubtree(
          pending_tree_->root_layer(),
          base::Bind(&LayerTreeHostImplDidBeginTracingCallback));
    }
    LayerTreeHostCommon::CallFunctionForSubtree(
        active_tree_->root_layer(),
        base::Bind(&LayerTreeHostImplDidBeginTracingCallback));
  }

  {
    TRACE_EVENT0("cc", "DrawLayers.FrameViewerTracing");
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
       TRACE_DISABLED_BY_DEFAULT("cc.debug") ","
       TRACE_DISABLED_BY_DEFAULT("cc.debug.quads") ","
       TRACE_DISABLED_BY_DEFAULT("devtools.timeline.layers"),
       "cc::LayerTreeHostImpl",
       id_,
       AsValueWithFrame(frame));
  }

  const DrawMode draw_mode = GetDrawMode();

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer()) {
    TRACE_EVENT0("cc", "DrawLayers.UpdateHudTexture");
    active_tree_->hud_layer()->UpdateHudTexture(draw_mode,
                                                resource_provider_.get());
  }

  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    bool disable_picture_quad_image_filtering =
        IsActivelyScrolling() || needs_animate_layers();

    scoped_ptr<SoftwareRenderer> temp_software_renderer =
        SoftwareRenderer::Create(this, &settings_, output_surface_.get(), NULL);
    temp_software_renderer->DrawFrame(&frame->render_passes,
                                      device_scale_factor_,
                                      DeviceViewport(),
                                      DeviceClip(),
                                      disable_picture_quad_image_filtering);
  } else {
    renderer_->DrawFrame(&frame->render_passes,
                         device_scale_factor_,
                         DeviceViewport(),
                         DeviceClip(),
                         false);
  }
  // The render passes should be consumed by the renderer.
  DCHECK(frame->render_passes.empty());
  frame->render_passes_by_id.clear();

  // The next frame should start by assuming nothing has changed, and changes
  // are noted as they occur.
  // TODO(boliu): If we did a temporary software renderer frame, propogate the
  // damage forward to the next frame.
  for (size_t i = 0; i < frame->render_surface_layer_list->size(); i++) {
    (*frame->render_surface_layer_list)[i]->render_surface()->damage_tracker()->
        DidDrawDamagedArea();
  }
  active_tree_->root_layer()->ResetAllChangeTrackingForSubtree();

  active_tree_->set_has_ever_been_drawn(true);
  devtools_instrumentation::DidDrawFrame(id_);
  benchmark_instrumentation::IssueImplThreadRenderingStatsEvent(
      rendering_stats_instrumentation_->impl_thread_rendering_stats());
  rendering_stats_instrumentation_->AccumulateAndClearImplThreadStats();
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  for (size_t i = 0; i < frame.will_draw_layers.size(); ++i)
    frame.will_draw_layers[i]->DidDraw(resource_provider_.get());

  // Once all layers have been drawn, pending texture uploads should no
  // longer block future uploads.
  resource_provider_->MarkPendingUploadsAsNonBlocking();
}

void LayerTreeHostImpl::FinishAllRendering() {
  if (renderer_)
    renderer_->Finish();
}

void LayerTreeHostImpl::SetUseGpuRasterization(bool use_gpu) {
  if (use_gpu == use_gpu_rasterization_)
    return;

  use_gpu_rasterization_ = use_gpu;
  ReleaseTreeResources();

  // Replace existing tile manager with another one that uses appropriate
  // rasterizer.
  if (tile_manager_) {
    DestroyTileManager();
    CreateAndSetTileManager();
  }

  // We have released tilings for both active and pending tree.
  // We would not have any content to draw until the pending tree is activated.
  // Prevent the active tree from drawing until activation.
  SetRequiresHighResToDraw();
}

const RendererCapabilitiesImpl&
LayerTreeHostImpl::GetRendererCapabilities() const {
  return renderer_->Capabilities();
}

bool LayerTreeHostImpl::SwapBuffers(const LayerTreeHostImpl::FrameData& frame) {
  ResetRequiresHighResToDraw();
  if (frame.has_no_damage) {
    active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);
    return false;
  }
  CompositorFrameMetadata metadata = MakeCompositorFrameMetadata();
  active_tree()->FinishSwapPromises(&metadata);
  for (size_t i = 0; i < metadata.latency_info.size(); i++) {
    TRACE_EVENT_FLOW_STEP0(
        "input",
        "LatencyInfo.Flow",
        TRACE_ID_DONT_MANGLE(metadata.latency_info[i].trace_id),
        "SwapBuffers");
  }
  renderer_->SwapBuffers(metadata);
  return true;
}

void LayerTreeHostImpl::OnNeedsBeginFramesChange(bool enable) {
  if (output_surface_)
    output_surface_->SetNeedsBeginFrame(enable);
  else
    DCHECK(!enable);
}

void LayerTreeHostImpl::WillBeginImplFrame(const BeginFrameArgs& args) {
  // Sample the frame time now. This time will be used for updating animations
  // when we draw.
  UpdateCurrentBeginFrameArgs(args);
  // Cache the begin impl frame interval
  begin_impl_frame_interval_ = args.interval;
}

void LayerTreeHostImpl::UpdateViewportContainerSizes() {
  LayerImpl* inner_container = active_tree_->InnerViewportContainerLayer();
  LayerImpl* outer_container = active_tree_->OuterViewportContainerLayer();

  if (!inner_container || !top_controls_manager_)
    return;

  ViewportAnchor anchor(InnerViewportScrollLayer(),
                        OuterViewportScrollLayer());

  // Adjust the inner viewport by shrinking/expanding the container to account
  // for the change in top controls height since the last Resize from Blink.
  inner_container->SetBoundsDelta(
      gfx::Vector2dF(0, active_tree_->top_controls_layout_height() -
          active_tree_->total_top_controls_content_offset()));

  if (!outer_container || outer_container->BoundsForScrolling().IsEmpty())
    return;

  // Adjust the outer viewport container as well, since adjusting only the
  // inner may cause its bounds to exceed those of the outer, causing scroll
  // clamping. We adjust it so it maintains the same aspect ratio as the
  // inner viewport.
  float aspect_ratio = inner_container->BoundsForScrolling().width() /
      inner_container->BoundsForScrolling().height();
  float target_height = outer_container->BoundsForScrolling().width() /
      aspect_ratio;
  float current_outer_height = outer_container->BoundsForScrolling().height() -
      outer_container->bounds_delta().y();
  gfx::Vector2dF delta(0, target_height - current_outer_height);

  outer_container->SetBoundsDelta(delta);
  active_tree_->InnerViewportScrollLayer()->SetBoundsDelta(delta);

  anchor.ResetViewportToAnchoredPosition();
}

void LayerTreeHostImpl::SetTopControlsLayoutHeight(float height) {
  if (active_tree_->top_controls_layout_height() == height)
    return;

  active_tree_->set_top_controls_layout_height(height);
  UpdateViewportContainerSizes();
  SetFullRootLayerDamage();
}

void LayerTreeHostImpl::SynchronouslyInitializeAllTiles() {
  // Only valid for the single-threaded non-scheduled/synchronous case
  // using the zero copy raster worker pool.
  single_thread_synchronous_task_graph_runner_->RunUntilIdle();
}

void LayerTreeHostImpl::DidLoseOutputSurface() {
  if (resource_provider_)
    resource_provider_->DidLoseOutputSurface();
  client_->DidLoseOutputSurfaceOnImplThread();
}

bool LayerTreeHostImpl::HaveRootScrollLayer() const {
  return !!InnerViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::RootLayer() const {
  return active_tree_->root_layer();
}

LayerImpl* LayerTreeHostImpl::InnerViewportScrollLayer() const {
  return active_tree_->InnerViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::OuterViewportScrollLayer() const {
  return active_tree_->OuterViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::CurrentlyScrollingLayer() const {
  return active_tree_->CurrentlyScrollingLayer();
}

bool LayerTreeHostImpl::IsActivelyScrolling() const {
  return (did_lock_scrolling_layer_ && CurrentlyScrollingLayer()) ||
         (InnerViewportScrollLayer() &&
          InnerViewportScrollLayer()->IsExternalFlingActive()) ||
         (OuterViewportScrollLayer() &&
          OuterViewportScrollLayer()->IsExternalFlingActive());
}

// Content layers can be either directly scrollable or contained in an outer
// scrolling layer which applies the scroll transform. Given a content layer,
// this function returns the associated scroll layer if any.
static LayerImpl* FindScrollLayerForContentLayer(LayerImpl* layer_impl) {
  if (!layer_impl)
    return NULL;

  if (layer_impl->scrollable())
    return layer_impl;

  if (layer_impl->DrawsContent() &&
      layer_impl->parent() &&
      layer_impl->parent()->scrollable())
    return layer_impl->parent();

  return NULL;
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!pending_tree_);
  if (recycle_tree_)
    recycle_tree_.swap(pending_tree_);
  else
    pending_tree_ = LayerTreeImpl::create(this);

  // Update the delta from the active tree, which may have
  // adjusted its delta prior to the pending tree being created.
  DCHECK_EQ(1.f, pending_tree_->sent_page_scale_delta());
  DCHECK_EQ(0.f, pending_tree_->sent_top_controls_delta());
  pending_tree_->SetPageScaleDelta(active_tree_->page_scale_delta() /
                                   active_tree_->sent_page_scale_delta());
  pending_tree_->set_top_controls_delta(
      active_tree_->top_controls_delta() -
      active_tree_->sent_top_controls_delta());

  client_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_ASYNC_BEGIN0("cc", "PendingTree:waiting", pending_tree_.get());
}

void LayerTreeHostImpl::UpdateVisibleTiles() {
  if (tile_manager_ && tile_manager_->UpdateVisibleTiles())
    DidInitializeVisibleTile();
  need_to_update_visible_tiles_before_draw_ = false;
}

void LayerTreeHostImpl::ActivateSyncTree() {
  need_to_update_visible_tiles_before_draw_ = true;

  if (pending_tree_) {
    TRACE_EVENT_ASYNC_END0("cc", "PendingTree:waiting", pending_tree_.get());

    active_tree_->SetRootLayerScrollOffsetDelegate(NULL);
    active_tree_->PushPersistedState(pending_tree_.get());
    // Process any requests in the UI resource queue.  The request queue is
    // given in LayerTreeHost::FinishCommitOnImplThread.  This must take place
    // before the swap.
    pending_tree_->ProcessUIResourceRequestQueue();

    if (pending_tree_->needs_full_tree_sync()) {
      active_tree_->SetRootLayer(
          TreeSynchronizer::SynchronizeTrees(pending_tree_->root_layer(),
                                             active_tree_->DetachLayerTree(),
                                             active_tree_.get()));
    }
    TreeSynchronizer::PushProperties(pending_tree_->root_layer(),
                                     active_tree_->root_layer());
    pending_tree_->PushPropertiesTo(active_tree_.get());

    // Now that we've synced everything from the pending tree to the active
    // tree, rename the pending tree the recycle tree so we can reuse it on the
    // next sync.
    DCHECK(!recycle_tree_);
    pending_tree_.swap(recycle_tree_);

    active_tree_->SetRootLayerScrollOffsetDelegate(
        root_layer_scroll_offset_delegate_);

    if (top_controls_manager_) {
      top_controls_manager_->SetControlsTopOffset(
          active_tree_->total_top_controls_content_offset() -
          top_controls_manager_->top_controls_height());
    }

    UpdateViewportContainerSizes();
  } else {
    active_tree_->ProcessUIResourceRequestQueue();
  }

  active_tree_->DidBecomeActive();
  ActivateAnimations();
  if (settings_.impl_side_painting)
    client_->RenewTreePriority();

  client_->OnCanDrawStateChanged(CanDraw());
  client_->DidActivateSyncTree();
  if (!tree_activation_callback_.is_null())
    tree_activation_callback_.Run();

  if (debug_state_.continuous_painting) {
    const RenderingStats& stats =
        rendering_stats_instrumentation_->GetRenderingStats();
    // TODO(hendrikw): This requires a different metric when we commit directly
    // to the active tree.  See crbug.com/429311.
    paint_time_counter_->SavePaintTime(
        stats.impl_stats.commit_to_activate_duration.GetLastTimeDelta() +
        stats.impl_stats.draw_duration.GetLastTimeDelta());
  }

  if (time_source_client_adapter_ && time_source_client_adapter_->Active())
    DCHECK(active_tree_->root_layer());

  scoped_ptr<PageScaleAnimation> page_scale_animation =
      active_tree_->TakePageScaleAnimation();
  if (page_scale_animation) {
    page_scale_animation_ = page_scale_animation.Pass();
    SetNeedsAnimate();
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
  }
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(proxy_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;
  DidVisibilityChange(this, visible_);
  EnforceManagedMemoryPolicy(ActualManagedMemoryPolicy());

  // If we just became visible, we have to ensure that we draw high res tiles,
  // to prevent checkerboard/low res flashes.
  if (visible_)
    SetRequiresHighResToDraw();
  else
    EvictAllUIResources();

  // Evict tiles immediately if invisible since this tab may never get another
  // draw or timer tick.
  if (!visible_)
    ManageTiles();

  if (!renderer_)
    return;

  renderer_->SetVisible(visible);
}

void LayerTreeHostImpl::SetNeedsAnimate() {
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsAnimateOnImplThread();
}

void LayerTreeHostImpl::SetNeedsRedraw() {
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsRedrawOnImplThread();
}

ManagedMemoryPolicy LayerTreeHostImpl::ActualManagedMemoryPolicy() const {
  ManagedMemoryPolicy actual = cached_managed_memory_policy_;
  if (debug_state_.rasterize_only_visible_content) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;
  } else if (use_gpu_rasterization()) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
  }

  if (zero_budget_) {
    actual.bytes_limit_when_visible = 0;
  }

  return actual;
}

size_t LayerTreeHostImpl::memory_allocation_limit_bytes() const {
  return ActualManagedMemoryPolicy().bytes_limit_when_visible;
}

int LayerTreeHostImpl::memory_allocation_priority_cutoff() const {
  return ManagedMemoryPolicy::PriorityCutoffToValue(
      ActualManagedMemoryPolicy().priority_cutoff_when_visible);
}

void LayerTreeHostImpl::ReleaseTreeResources() {
  active_tree_->ReleaseResources();
  if (pending_tree_)
    pending_tree_->ReleaseResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseResources();

  EvictAllUIResources();
}

void LayerTreeHostImpl::CreateAndSetRenderer() {
  DCHECK(!renderer_);
  DCHECK(output_surface_);
  DCHECK(resource_provider_);

  if (output_surface_->capabilities().delegated_rendering) {
    renderer_ = DelegatingRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider_.get());
  } else if (output_surface_->context_provider()) {
    renderer_ = GLRenderer::Create(this,
                                   &settings_,
                                   output_surface_.get(),
                                   resource_provider_.get(),
                                   texture_mailbox_deleter_.get(),
                                   settings_.highp_threshold_min);
  } else if (output_surface_->software_device()) {
    renderer_ = SoftwareRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider_.get());
  }
  DCHECK(renderer_);

  renderer_->SetVisible(visible_);
  SetFullRootLayerDamage();

  // See note in LayerTreeImpl::UpdateDrawProperties.  Renderer needs to be
  // initialized to get max texture size.  Also, after releasing resources,
  // trees need another update to generate new ones.
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();
  client_->UpdateRendererCapabilitiesOnImplThread();
}

void LayerTreeHostImpl::CreateAndSetTileManager() {
  DCHECK(!tile_manager_);
  DCHECK(settings_.impl_side_painting);
  DCHECK(output_surface_);
  DCHECK(resource_provider_);

  CreateResourceAndRasterWorkerPool(
      &raster_worker_pool_, &resource_pool_, &staging_resource_pool_);
  DCHECK(raster_worker_pool_);
  DCHECK(resource_pool_);

  base::SingleThreadTaskRunner* task_runner =
      proxy_->HasImplThread() ? proxy_->ImplThreadTaskRunner()
                              : proxy_->MainThreadTaskRunner();
  DCHECK(task_runner);
  size_t scheduled_raster_task_limit =
      IsSynchronousSingleThreaded() ? std::numeric_limits<size_t>::max()
                                    : settings_.scheduled_raster_task_limit;
  tile_manager_ = TileManager::Create(this,
                                      task_runner,
                                      resource_pool_.get(),
                                      raster_worker_pool_->AsRasterizer(),
                                      rendering_stats_instrumentation_,
                                      scheduled_raster_task_limit);

  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  need_to_update_visible_tiles_before_draw_ = false;
}

void LayerTreeHostImpl::CreateResourceAndRasterWorkerPool(
    scoped_ptr<RasterWorkerPool>* raster_worker_pool,
    scoped_ptr<ResourcePool>* resource_pool,
    scoped_ptr<ResourcePool>* staging_resource_pool) {
  base::SingleThreadTaskRunner* task_runner =
      proxy_->HasImplThread() ? proxy_->ImplThreadTaskRunner()
                              : proxy_->MainThreadTaskRunner();
  DCHECK(task_runner);

  ContextProvider* context_provider = output_surface_->context_provider();
  bool should_use_zero_copy_rasterizer =
      settings_.use_zero_copy || IsSynchronousSingleThreaded();

  if (!context_provider) {
    *resource_pool =
        ResourcePool::Create(resource_provider_.get(),
                             GL_TEXTURE_2D,
                             resource_provider_->best_texture_format());

    *raster_worker_pool =
        BitmapRasterWorkerPool::Create(task_runner,
                                       RasterWorkerPool::GetTaskGraphRunner(),
                                       resource_provider_.get());
  } else if (use_gpu_rasterization_) {
    *resource_pool =
        ResourcePool::Create(resource_provider_.get(),
                             GL_TEXTURE_2D,
                             resource_provider_->best_texture_format());

    *raster_worker_pool =
        GpuRasterWorkerPool::Create(task_runner,
                                    context_provider,
                                    resource_provider_.get(),
                                    settings_.use_distance_field_text);
  } else if (should_use_zero_copy_rasterizer && CanUseZeroCopyRasterizer()) {
    *resource_pool = ResourcePool::Create(
        resource_provider_.get(),
        GetMapImageTextureTarget(context_provider->ContextCapabilities()),
        resource_provider_->best_texture_format());

    TaskGraphRunner* task_graph_runner;
    if (IsSynchronousSingleThreaded()) {
      DCHECK(!single_thread_synchronous_task_graph_runner_);
      single_thread_synchronous_task_graph_runner_.reset(new TaskGraphRunner);
      task_graph_runner = single_thread_synchronous_task_graph_runner_.get();
    } else {
      task_graph_runner = RasterWorkerPool::GetTaskGraphRunner();
    }

    *raster_worker_pool = ZeroCopyRasterWorkerPool::Create(
        task_runner, task_graph_runner, resource_provider_.get());
  } else if (settings_.use_one_copy && CanUseOneCopyRasterizer()) {
    // We need to create a staging resource pool when using copy rasterizer.
    *staging_resource_pool = ResourcePool::Create(
        resource_provider_.get(),
        GetMapImageTextureTarget(context_provider->ContextCapabilities()),
        resource_provider_->best_texture_format());
    *resource_pool =
        ResourcePool::Create(resource_provider_.get(),
                             GL_TEXTURE_2D,
                             resource_provider_->best_texture_format());

    *raster_worker_pool =
        OneCopyRasterWorkerPool::Create(task_runner,
                                        RasterWorkerPool::GetTaskGraphRunner(),
                                        context_provider,
                                        resource_provider_.get(),
                                        staging_resource_pool_.get());
  } else {
    *resource_pool = ResourcePool::Create(
        resource_provider_.get(),
        GL_TEXTURE_2D,
        resource_provider_->memory_efficient_texture_format());

    *raster_worker_pool = PixelBufferRasterWorkerPool::Create(
        task_runner,
        RasterWorkerPool::GetTaskGraphRunner(),
        context_provider,
        resource_provider_.get(),
        GetMaxTransferBufferUsageBytes(context_provider->ContextCapabilities(),
                                       settings_.refresh_rate));
  }
}

void LayerTreeHostImpl::DestroyTileManager() {
  tile_manager_ = nullptr;
  resource_pool_ = nullptr;
  staging_resource_pool_ = nullptr;
  raster_worker_pool_ = nullptr;
  single_thread_synchronous_task_graph_runner_ = nullptr;
}

bool LayerTreeHostImpl::UsePendingTreeForSync() const {
  // In impl-side painting, synchronize to the pending tree so that it has
  // time to raster before being displayed.
  return settings_.impl_side_painting;
}

bool LayerTreeHostImpl::IsSynchronousSingleThreaded() const {
  return !proxy_->HasImplThread() && !settings_.single_thread_proxy_scheduler;
}

bool LayerTreeHostImpl::CanUseZeroCopyRasterizer() const {
  return GetRendererCapabilities().using_image;
}

bool LayerTreeHostImpl::CanUseOneCopyRasterizer() const {
  // Sync query support is required by one-copy rasterizer.
  return GetRendererCapabilities().using_image &&
         resource_provider_->use_sync_query();
}

void LayerTreeHostImpl::EnforceZeroBudget(bool zero_budget) {
  SetManagedMemoryPolicy(cached_managed_memory_policy_, zero_budget);
}

bool LayerTreeHostImpl::InitializeRenderer(
    scoped_ptr<OutputSurface> output_surface) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::InitializeRenderer");

  // Since we will create a new resource provider, we cannot continue to use
  // the old resources (i.e. render_surfaces and texture IDs). Clear them
  // before we destroy the old resource provider.
  ReleaseTreeResources();

  // Note: order is important here.
  renderer_ = nullptr;
  DestroyTileManager();
  resource_provider_ = nullptr;
  output_surface_ = nullptr;

  if (!output_surface->BindToClient(this))
    return false;

  output_surface_ = output_surface.Pass();
  resource_provider_ =
      ResourceProvider::Create(output_surface_.get(),
                               shared_bitmap_manager_,
                               gpu_memory_buffer_manager_,
                               proxy_->blocking_main_thread_task_runner(),
                               settings_.highp_threshold_min,
                               settings_.use_rgba_4444_textures,
                               settings_.texture_id_allocation_chunk_size);

  if (output_surface_->capabilities().deferred_gl_initialization)
    EnforceZeroBudget(true);

  CreateAndSetRenderer();

  if (settings_.impl_side_painting)
    CreateAndSetTileManager();

  // Initialize vsync parameters to sane values.
  const base::TimeDelta display_refresh_interval =
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        settings_.refresh_rate);
  CommitVSyncParameters(base::TimeTicks(), display_refresh_interval);

  // TODO(brianderson): Don't use a hard-coded parent draw time.
  base::TimeDelta parent_draw_time =
      (!settings_.begin_frame_scheduling_enabled &&
       output_surface_->capabilities().adjust_deadline_for_parent)
          ? BeginFrameArgs::DefaultEstimatedParentDrawTime()
          : base::TimeDelta();
  client_->SetEstimatedParentDrawTime(parent_draw_time);

  int max_frames_pending = output_surface_->capabilities().max_frames_pending;
  if (max_frames_pending <= 0)
    max_frames_pending = OutputSurface::DEFAULT_MAX_FRAMES_PENDING;
  client_->SetMaxSwapsPendingOnImplThread(max_frames_pending);
  client_->OnCanDrawStateChanged(CanDraw());

  // There will not be anything to draw here, so set high res
  // to avoid checkerboards, typically when we are recovering
  // from lost context.
  SetRequiresHighResToDraw();

  return true;
}

void LayerTreeHostImpl::CommitVSyncParameters(base::TimeTicks timebase,
                                              base::TimeDelta interval) {
  client_->CommitVSyncParameters(timebase, interval);
}

void LayerTreeHostImpl::DeferredInitialize() {
  DCHECK(output_surface_->capabilities().deferred_gl_initialization);
  DCHECK(settings_.impl_side_painting);
  DCHECK(output_surface_->context_provider());

  ReleaseTreeResources();
  renderer_ = nullptr;
  DestroyTileManager();

  resource_provider_->InitializeGL();

  CreateAndSetRenderer();
  EnforceZeroBudget(false);
  CreateAndSetTileManager();

  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::ReleaseGL() {
  DCHECK(output_surface_->capabilities().deferred_gl_initialization);
  DCHECK(settings_.impl_side_painting);
  DCHECK(output_surface_->context_provider());

  ReleaseTreeResources();
  renderer_ = nullptr;
  DestroyTileManager();

  resource_provider_->InitializeSoftware();
  output_surface_->ReleaseContextProvider();

  CreateAndSetRenderer();
  EnforceZeroBudget(true);
  CreateAndSetTileManager();

  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;

  if (pending_tree_)
    active_tree_->SetViewportSizeInvalid();

  device_viewport_size_ = device_viewport_size;

  UpdateViewportContainerSizes();
  client_->OnCanDrawStateChanged(CanDraw());
  SetFullRootLayerDamage();
  active_tree_->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::SetOverhangUIResource(
    UIResourceId overhang_ui_resource_id,
    const gfx::Size& overhang_ui_resource_size) {
  overhang_ui_resource_id_ = overhang_ui_resource_id;
  overhang_ui_resource_size_ = overhang_ui_resource_size;
}

void LayerTreeHostImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  SetFullRootLayerDamage();
}

const gfx::Rect LayerTreeHostImpl::ViewportRectForTilePriority() const {
  if (viewport_rect_for_tile_priority_.IsEmpty())
    return DeviceViewport();

  return viewport_rect_for_tile_priority_;
}

gfx::Size LayerTreeHostImpl::DrawViewportSize() const {
  return DeviceViewport().size();
}

gfx::Rect LayerTreeHostImpl::DeviceViewport() const {
  if (external_viewport_.IsEmpty())
    return gfx::Rect(device_viewport_size_);

  return external_viewport_;
}

gfx::Rect LayerTreeHostImpl::DeviceClip() const {
  if (external_clip_.IsEmpty())
    return DeviceViewport();

  return external_clip_;
}

const gfx::Transform& LayerTreeHostImpl::DrawTransform() const {
  return external_transform_;
}

void LayerTreeHostImpl::DidChangeTopControlsPosition() {
  UpdateViewportContainerSizes();
  SetNeedsRedraw();
  SetNeedsAnimate();
  active_tree_->set_needs_update_draw_properties();
  SetFullRootLayerDamage();
}

void LayerTreeHostImpl::SetControlsTopOffset(float offset) {
  float current_top_offset = active_tree_->top_controls_content_offset() -
      top_controls_manager_->top_controls_height();
  active_tree_->set_top_controls_delta(offset - current_top_offset);
}

float LayerTreeHostImpl::ControlsTopOffset() const {
  return active_tree_->total_top_controls_content_offset() -
      top_controls_manager_->top_controls_height();
}

void LayerTreeHostImpl::BindToClient(InputHandlerClient* client) {
  DCHECK(input_handler_client_ == NULL);
  input_handler_client_ = client;
}

static LayerImpl* NextScrollLayer(LayerImpl* layer) {
  if (LayerImpl* scroll_parent = layer->scroll_parent())
    return scroll_parent;
  return layer->parent();
}

LayerImpl* LayerTreeHostImpl::FindScrollLayerForDeviceViewportPoint(
    const gfx::PointF& device_viewport_point,
    InputHandler::ScrollInputType type,
    LayerImpl* layer_impl,
    bool* scroll_on_main_thread,
    bool* optional_has_ancestor_scroll_handler) const {
  DCHECK(scroll_on_main_thread);

  // Walk up the hierarchy and look for a scrollable layer.
  LayerImpl* potentially_scrolling_layer_impl = NULL;
  for (; layer_impl; layer_impl = NextScrollLayer(layer_impl)) {
    // The content layer can also block attempts to scroll outside the main
    // thread.
    ScrollStatus status = layer_impl->TryScroll(device_viewport_point, type);
    if (status == ScrollOnMainThread) {
      *scroll_on_main_thread = true;
      return NULL;
    }

    LayerImpl* scroll_layer_impl = FindScrollLayerForContentLayer(layer_impl);
    if (!scroll_layer_impl)
      continue;

    status = scroll_layer_impl->TryScroll(device_viewport_point, type);
    // If any layer wants to divert the scroll event to the main thread, abort.
    if (status == ScrollOnMainThread) {
      *scroll_on_main_thread = true;
      return NULL;
    }

    if (optional_has_ancestor_scroll_handler &&
        scroll_layer_impl->have_scroll_event_handlers())
      *optional_has_ancestor_scroll_handler = true;

    if (status == ScrollStarted && !potentially_scrolling_layer_impl)
      potentially_scrolling_layer_impl = scroll_layer_impl;
  }

  // Falling back to the root scroll layer ensures generation of root overscroll
  // notifications while preventing scroll updates from being unintentionally
  // forwarded to the main thread.
  if (!potentially_scrolling_layer_impl)
    potentially_scrolling_layer_impl = OuterViewportScrollLayer()
                                           ? OuterViewportScrollLayer()
                                           : InnerViewportScrollLayer();

  return potentially_scrolling_layer_impl;
}

// Similar to LayerImpl::HasAncestor, but walks up the scroll parents.
static bool HasScrollAncestor(LayerImpl* child, LayerImpl* scroll_ancestor) {
  DCHECK(scroll_ancestor);
  for (LayerImpl* ancestor = child; ancestor;
       ancestor = NextScrollLayer(ancestor)) {
    if (ancestor->scrollable())
      return ancestor == scroll_ancestor;
  }
  return false;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBegin(
    const gfx::Point& viewport_point,
    InputHandler::ScrollInputType type) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBegin");

  if (top_controls_manager_)
    top_controls_manager_->ScrollBegin();

  DCHECK(!CurrentlyScrollingLayer());
  ClearCurrentlyScrollingLayer();

  gfx::PointF device_viewport_point = gfx::ScalePoint(viewport_point,
                                                      device_scale_factor_);
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  if (layer_impl) {
    LayerImpl* scroll_layer_impl =
        active_tree_->FindFirstScrollingLayerThatIsHitByPoint(
            device_viewport_point);
    if (scroll_layer_impl && !HasScrollAncestor(layer_impl, scroll_layer_impl))
      return ScrollUnknown;
  }

  bool scroll_on_main_thread = false;
  LayerImpl* scrolling_layer_impl =
      FindScrollLayerForDeviceViewportPoint(device_viewport_point,
                                            type,
                                            layer_impl,
                                            &scroll_on_main_thread,
                                            &scroll_affects_scroll_handler_);

  if (scroll_on_main_thread) {
    UMA_HISTOGRAM_BOOLEAN("TryScroll.SlowScroll", true);
    return ScrollOnMainThread;
  }

  if (scrolling_layer_impl) {
    active_tree_->SetCurrentlyScrollingLayer(scrolling_layer_impl);
    should_bubble_scrolls_ = (type != NonBubblingGesture);
    wheel_scrolling_ = (type == Wheel);
    client_->RenewTreePriority();
    UMA_HISTOGRAM_BOOLEAN("TryScroll.SlowScroll", false);
    return ScrollStarted;
  }
  return ScrollIgnored;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollAnimated(
    const gfx::Point& viewport_point,
    const gfx::Vector2dF& scroll_delta) {
  if (LayerImpl* layer_impl = CurrentlyScrollingLayer()) {
    Animation* animation =
        layer_impl->layer_animation_controller()->GetAnimation(
            Animation::ScrollOffset);
    if (!animation)
      return ScrollIgnored;

    ScrollOffsetAnimationCurve* curve =
        animation->curve()->ToScrollOffsetAnimationCurve();

    gfx::ScrollOffset new_target =
        gfx::ScrollOffsetWithDelta(curve->target_value(), scroll_delta);
    new_target.SetToMax(gfx::ScrollOffset());
    new_target.SetToMin(layer_impl->MaxScrollOffset());

    curve->UpdateTarget(animation->TrimTimeToCurrentIteration(
                            CurrentBeginFrameArgs().frame_time),
                        new_target);

    return ScrollStarted;
  }
  // ScrollAnimated is only used for wheel scrolls. We use the same bubbling
  // behavior as ScrollBy to determine which layer to animate, but we do not
  // do the Android-specific things in ScrollBy like showing top controls.
  InputHandler::ScrollStatus scroll_status = ScrollBegin(viewport_point, Wheel);
  if (scroll_status == ScrollStarted) {
    gfx::Vector2dF pending_delta = scroll_delta;
    for (LayerImpl* layer_impl = CurrentlyScrollingLayer(); layer_impl;
         layer_impl = layer_impl->parent()) {
      if (!layer_impl->scrollable())
        continue;

      gfx::ScrollOffset current_offset = layer_impl->TotalScrollOffset();
      gfx::ScrollOffset target_offset =
          ScrollOffsetWithDelta(current_offset, pending_delta);
      target_offset.SetToMax(gfx::ScrollOffset());
      target_offset.SetToMin(layer_impl->MaxScrollOffset());
      gfx::Vector2dF actual_delta = target_offset.DeltaFrom(current_offset);

      const float kEpsilon = 0.1f;
      bool can_layer_scroll = (std::abs(actual_delta.x()) > kEpsilon ||
                               std::abs(actual_delta.y()) > kEpsilon);

      if (!can_layer_scroll) {
        layer_impl->ScrollBy(actual_delta);
        pending_delta -= actual_delta;
        continue;
      }

      active_tree_->SetCurrentlyScrollingLayer(layer_impl);

      scoped_ptr<ScrollOffsetAnimationCurve> curve =
          ScrollOffsetAnimationCurve::Create(target_offset,
                                             EaseInOutTimingFunction::Create());
      curve->SetInitialValue(current_offset);

      scoped_ptr<Animation> animation =
          Animation::Create(curve.Pass(),
                            AnimationIdProvider::NextAnimationId(),
                            AnimationIdProvider::NextGroupId(),
                            Animation::ScrollOffset);
      animation->set_is_impl_only(true);

      layer_impl->layer_animation_controller()->AddAnimation(animation.Pass());

      SetNeedsAnimate();
      return ScrollStarted;
    }
  }
  ScrollEnd();
  return scroll_status;
}

gfx::Vector2dF LayerTreeHostImpl::ScrollLayerWithViewportSpaceDelta(
    LayerImpl* layer_impl,
    float scale_from_viewport_to_screen_space,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta) {
  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  DCHECK(layer_impl->screen_space_transform().IsInvertible());
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert = layer_impl->screen_space_transform().GetInverse(
      &inverse_screen_space_transform);
  // TODO(shawnsingh): With the advent of impl-side crolling for non-root
  // layers, we may need to explicitly handle uninvertible transforms here.
  DCHECK(did_invert);

  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // First project the scroll start and end points to local layer space to find
  // the scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_point,
                             &start_clipped);
  gfx::PointF local_end_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_end_point,
                             &end_clipped);

  // In general scroll point coordinates should not get clipped.
  DCHECK(!start_clipped);
  DCHECK(!end_clipped);
  if (start_clipped || end_clipped)
    return gfx::Vector2dF();

  // local_start_point and local_end_point are in content space but we want to
  // move them to layer space for scrolling.
  float width_scale = 1.f / layer_impl->contents_scale_x();
  float height_scale = 1.f / layer_impl->contents_scale_y();
  local_start_point.Scale(width_scale, height_scale);
  local_end_point.Scale(width_scale, height_scale);

  // Apply the scroll delta.
  gfx::Vector2dF previous_delta = layer_impl->ScrollDelta();
  layer_impl->ScrollBy(local_end_point - local_start_point);

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point = local_start_point +
                                       layer_impl->ScrollDelta() -
                                       previous_delta;
  gfx::PointF actual_local_content_end_point =
      gfx::ScalePoint(actual_local_end_point,
                      1.f / width_scale,
                      1.f / height_scale);

  // Calculate the applied scroll delta in viewport space coordinates.
  gfx::PointF actual_screen_space_end_point =
      MathUtil::MapPoint(layer_impl->screen_space_transform(),
                         actual_local_content_end_point,
                         &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();
  gfx::PointF actual_viewport_end_point =
      gfx::ScalePoint(actual_screen_space_end_point,
                      1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

static gfx::Vector2dF ScrollLayerWithLocalDelta(LayerImpl* layer_impl,
    const gfx::Vector2dF& local_delta) {
  gfx::Vector2dF previous_delta(layer_impl->ScrollDelta());
  layer_impl->ScrollBy(local_delta);
  return layer_impl->ScrollDelta() - previous_delta;
}

bool LayerTreeHostImpl::ShouldTopControlsConsumeScroll(
    const gfx::Vector2dF& scroll_delta) const {
  DCHECK(CurrentlyScrollingLayer());

  if (!top_controls_manager_)
    return false;

  // Always consume if it's in the direction to show the top controls.
  if (scroll_delta.y() < 0)
    return true;

  if (CurrentlyScrollingLayer() != InnerViewportScrollLayer() &&
      CurrentlyScrollingLayer() != OuterViewportScrollLayer())
    return false;

  if (InnerViewportScrollLayer()->MaxScrollOffset().y() > 0)
    return true;

  if (OuterViewportScrollLayer() &&
      OuterViewportScrollLayer()->MaxScrollOffset().y() > 0)
    return true;

  return false;
}

InputHandlerScrollResult LayerTreeHostImpl::ScrollBy(
    const gfx::Point& viewport_point,
    const gfx::Vector2dF& scroll_delta) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBy");
  if (!CurrentlyScrollingLayer())
    return InputHandlerScrollResult();

  gfx::Vector2dF pending_delta = scroll_delta;
  gfx::Vector2dF unused_root_delta;
  bool did_scroll_x = false;
  bool did_scroll_y = false;
  bool did_scroll_top_controls = false;

  bool consume_by_top_controls = ShouldTopControlsConsumeScroll(scroll_delta);

  // There's an edge case where the outer viewport isn't scrollable when the
  // scroll starts, however, as the top controls show the outer viewport becomes
  // scrollable. Therefore, always try scrolling the outer viewport before the
  // inner.
  // TODO(bokan): Move the top controls logic out of the loop since the scroll
  // that causes the outer viewport to become scrollable will still be applied
  // to the inner viewport.
  LayerImpl* start_layer = CurrentlyScrollingLayer();
  if (start_layer == InnerViewportScrollLayer() && OuterViewportScrollLayer())
      start_layer = OuterViewportScrollLayer();

  for (LayerImpl* layer_impl = start_layer;
       layer_impl;
       layer_impl = layer_impl->parent()) {
    if (!layer_impl->scrollable())
      continue;

    if (layer_impl == InnerViewportScrollLayer() ||
        layer_impl == OuterViewportScrollLayer()) {
      if (consume_by_top_controls) {
        gfx::Vector2dF excess_delta =
            top_controls_manager_->ScrollBy(pending_delta);
        gfx::Vector2dF applied_delta = pending_delta - excess_delta;
        pending_delta = excess_delta;
        // Force updating of vertical adjust values if needed.
        if (applied_delta.y() != 0)
          did_scroll_top_controls = true;
      }
      // Track root layer deltas for reporting overscroll.
      if (layer_impl == InnerViewportScrollLayer())
        unused_root_delta = pending_delta;
    }

    gfx::Vector2dF applied_delta;
    // Gesture events need to be transformed from viewport coordinates to local
    // layer coordinates so that the scrolling contents exactly follow the
    // user's finger. In contrast, wheel events represent a fixed amount of
    // scrolling so we can just apply them directly.
    if (!wheel_scrolling_) {
      float scale_from_viewport_to_screen_space = device_scale_factor_;
      applied_delta =
          ScrollLayerWithViewportSpaceDelta(layer_impl,
                                            scale_from_viewport_to_screen_space,
                                            viewport_point, pending_delta);
    } else {
      applied_delta = ScrollLayerWithLocalDelta(layer_impl, pending_delta);
    }

    const float kEpsilon = 0.1f;
    if (layer_impl == InnerViewportScrollLayer()) {
      unused_root_delta.Subtract(applied_delta);
      if (std::abs(unused_root_delta.x()) < kEpsilon)
        unused_root_delta.set_x(0.0f);
      if (std::abs(unused_root_delta.y()) < kEpsilon)
        unused_root_delta.set_y(0.0f);
      // Disable overscroll on axes which is impossible to scroll.
      if (settings_.report_overscroll_only_for_scrollable_axes) {
        if (std::abs(active_tree_->TotalMaxScrollOffset().x()) <= kEpsilon ||
            !layer_impl->user_scrollable_horizontal())
          unused_root_delta.set_x(0.0f);
        if (std::abs(active_tree_->TotalMaxScrollOffset().y()) <= kEpsilon ||
            !layer_impl->user_scrollable_vertical())
          unused_root_delta.set_y(0.0f);
      }
    }

    // Scrolls should bubble perfectly between the outer and inner viewports.
    bool allow_unrestricted_bubbling_for_current_layer =
        layer_impl == OuterViewportScrollLayer();
    bool allow_bubbling_for_current_layer =
        allow_unrestricted_bubbling_for_current_layer || should_bubble_scrolls_;

    // If the layer wasn't able to move, try the next one in the hierarchy.
    bool did_move_layer_x = std::abs(applied_delta.x()) > kEpsilon;
    bool did_move_layer_y = std::abs(applied_delta.y()) > kEpsilon;
    did_scroll_x |= did_move_layer_x;
    did_scroll_y |= did_move_layer_y;
    if (!did_move_layer_x && !did_move_layer_y) {
      if (allow_bubbling_for_current_layer || !did_lock_scrolling_layer_)
        continue;
      else
        break;
    }

    did_lock_scrolling_layer_ = true;

    // When scrolls are allowed to bubble, it's important that the original
    // scrolling layer be preserved. This ensures that, after a scroll bubbles,
    // the user can reverse scroll directions and immediately resume scrolling
    // the original layer that scrolled.
    if (!should_bubble_scrolls_)
      active_tree_->SetCurrentlyScrollingLayer(layer_impl);

    if (!allow_bubbling_for_current_layer)
      break;

    if (allow_unrestricted_bubbling_for_current_layer) {
      pending_delta -= applied_delta;
    } else {
      // If the applied delta is within 45 degrees of the input delta, bail out
      // to make it easier to scroll just one layer in one direction without
      // affecting any of its parents.
      float angle_threshold = 45;
      if (MathUtil::SmallestAngleBetweenVectors(applied_delta, pending_delta) <
          angle_threshold) {
        pending_delta = gfx::Vector2dF();
        break;
      }

      // Allow further movement only on an axis perpendicular to the direction
      // in which the layer moved.
      gfx::Vector2dF perpendicular_axis(-applied_delta.y(), applied_delta.x());
      pending_delta =
          MathUtil::ProjectVector(pending_delta, perpendicular_axis);
    }

    if (gfx::ToRoundedVector2d(pending_delta).IsZero())
      break;
  }

  bool did_scroll_content = did_scroll_x || did_scroll_y;
  if (did_scroll_content) {
    // If we are scrolling with an active scroll handler, forward latency
    // tracking information to the main thread so the delay introduced by the
    // handler is accounted for.
    if (scroll_affects_scroll_handler())
      NotifySwapPromiseMonitorsOfForwardingToMainThread();
    client_->SetNeedsCommitOnImplThread();
    SetNeedsRedraw();
    client_->RenewTreePriority();
  }

  // Scrolling along an axis resets accumulated root overscroll for that axis.
  if (did_scroll_x)
    accumulated_root_overscroll_.set_x(0);
  if (did_scroll_y)
    accumulated_root_overscroll_.set_y(0);
  accumulated_root_overscroll_ += unused_root_delta;

  InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = did_scroll_content || did_scroll_top_controls;
  scroll_result.did_overscroll_root = !unused_root_delta.IsZero();
  scroll_result.accumulated_root_overscroll = accumulated_root_overscroll_;
  scroll_result.unused_scroll_delta = unused_root_delta;
  return scroll_result;
}

// This implements scrolling by page as described here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms645601(v=vs.85).aspx#_win32_The_Mouse_Wheel
// for events with WHEEL_PAGESCROLL set.
bool LayerTreeHostImpl::ScrollVerticallyByPage(const gfx::Point& viewport_point,
                                               ScrollDirection direction) {
  DCHECK(wheel_scrolling_);

  for (LayerImpl* layer_impl = CurrentlyScrollingLayer();
       layer_impl;
       layer_impl = layer_impl->parent()) {
    if (!layer_impl->scrollable())
      continue;

    if (!layer_impl->HasScrollbar(VERTICAL))
      continue;

    float height = layer_impl->clip_height();

    // These magical values match WebKit and are designed to scroll nearly the
    // entire visible content height but leave a bit of overlap.
    float page = std::max(height * 0.875f, 1.f);
    if (direction == SCROLL_BACKWARD)
      page = -page;

    gfx::Vector2dF delta = gfx::Vector2dF(0.f, page);

    gfx::Vector2dF applied_delta = ScrollLayerWithLocalDelta(layer_impl, delta);

    if (!applied_delta.IsZero()) {
      client_->SetNeedsCommitOnImplThread();
      SetNeedsRedraw();
      client_->RenewTreePriority();
      return true;
    }

    active_tree_->SetCurrentlyScrollingLayer(layer_impl);
  }

  return false;
}

void LayerTreeHostImpl::SetRootLayerScrollOffsetDelegate(
      LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) {
  root_layer_scroll_offset_delegate_ = root_layer_scroll_offset_delegate;
  active_tree_->SetRootLayerScrollOffsetDelegate(
      root_layer_scroll_offset_delegate_);
}

void LayerTreeHostImpl::OnRootLayerDelegatedScrollOffsetChanged() {
  DCHECK(root_layer_scroll_offset_delegate_);
  client_->SetNeedsCommitOnImplThread();
  SetNeedsRedraw();
  active_tree_->OnRootLayerDelegatedScrollOffsetChanged();
  active_tree_->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::ClearCurrentlyScrollingLayer() {
  active_tree_->ClearCurrentlyScrollingLayer();
  did_lock_scrolling_layer_ = false;
  scroll_affects_scroll_handler_ = false;
  accumulated_root_overscroll_ = gfx::Vector2dF();
}

void LayerTreeHostImpl::ScrollEnd() {
  if (top_controls_manager_)
    top_controls_manager_->ScrollEnd();
  ClearCurrentlyScrollingLayer();
}

InputHandler::ScrollStatus LayerTreeHostImpl::FlingScrollBegin() {
  if (!active_tree_->CurrentlyScrollingLayer())
    return ScrollIgnored;

  if (settings_.ignore_root_layer_flings &&
      (active_tree_->CurrentlyScrollingLayer() == InnerViewportScrollLayer() ||
       active_tree_->CurrentlyScrollingLayer() == OuterViewportScrollLayer())) {
    ClearCurrentlyScrollingLayer();
    return ScrollIgnored;
  }

  if (!wheel_scrolling_) {
    // Allow the fling to lock to the first layer that moves after the initial
    // fling |ScrollBy()| event.
    did_lock_scrolling_layer_ = false;
    should_bubble_scrolls_ = false;
  }

  return ScrollStarted;
}

float LayerTreeHostImpl::DeviceSpaceDistanceToLayer(
    const gfx::PointF& device_viewport_point,
    LayerImpl* layer_impl) {
  if (!layer_impl)
    return std::numeric_limits<float>::max();

  gfx::Rect layer_impl_bounds(
      layer_impl->content_bounds());

  gfx::RectF device_viewport_layer_impl_bounds = MathUtil::MapClippedRect(
      layer_impl->screen_space_transform(),
      layer_impl_bounds);

  return device_viewport_layer_impl_bounds.ManhattanDistanceToPoint(
      device_viewport_point);
}

void LayerTreeHostImpl::MouseMoveAt(const gfx::Point& viewport_point) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(viewport_point,
                                                      device_scale_factor_);
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);
  if (HandleMouseOverScrollbar(layer_impl, device_viewport_point))
    return;

  if (scroll_layer_id_when_mouse_over_scrollbar_) {
    LayerImpl* scroll_layer_impl = active_tree_->LayerById(
        scroll_layer_id_when_mouse_over_scrollbar_);

    // The check for a null scroll_layer_impl below was added to see if it will
    // eliminate the crashes described in http://crbug.com/326635.
    // TODO(wjmaclean) Add a unit test if this fixes the crashes.
    ScrollbarAnimationController* animation_controller =
        scroll_layer_impl ? scroll_layer_impl->scrollbar_animation_controller()
                          : NULL;
    if (animation_controller)
      animation_controller->DidMouseMoveOffScrollbar();
    scroll_layer_id_when_mouse_over_scrollbar_ = 0;
  }

  bool scroll_on_main_thread = false;
  LayerImpl* scroll_layer_impl =
      FindScrollLayerForDeviceViewportPoint(device_viewport_point,
                                            InputHandler::Gesture,
                                            layer_impl,
                                            &scroll_on_main_thread,
                                            NULL);
  if (scroll_on_main_thread || !scroll_layer_impl)
    return;

  ScrollbarAnimationController* animation_controller =
      scroll_layer_impl->scrollbar_animation_controller();
  if (!animation_controller)
    return;

  // TODO(wjmaclean) Is it ok to choose distance from more than two scrollbars?
  float distance_to_scrollbar = std::numeric_limits<float>::max();
  for (LayerImpl::ScrollbarSet::iterator it =
           scroll_layer_impl->scrollbars()->begin();
       it != scroll_layer_impl->scrollbars()->end();
       ++it)
    distance_to_scrollbar =
        std::min(distance_to_scrollbar,
                 DeviceSpaceDistanceToLayer(device_viewport_point, *it));

  animation_controller->DidMouseMoveNear(distance_to_scrollbar /
                                         device_scale_factor_);
}

bool LayerTreeHostImpl::HandleMouseOverScrollbar(LayerImpl* layer_impl,
    const gfx::PointF& device_viewport_point) {
  if (layer_impl && layer_impl->ToScrollbarLayer()) {
    int scroll_layer_id = layer_impl->ToScrollbarLayer()->ScrollLayerId();
    layer_impl = active_tree_->LayerById(scroll_layer_id);
    if (layer_impl && layer_impl->scrollbar_animation_controller()) {
      scroll_layer_id_when_mouse_over_scrollbar_ = scroll_layer_id;
      layer_impl->scrollbar_animation_controller()->DidMouseMoveNear(0);
    } else {
      scroll_layer_id_when_mouse_over_scrollbar_ = 0;
    }

    return true;
  }

  return false;
}

void LayerTreeHostImpl::PinchGestureBegin() {
  pinch_gesture_active_ = true;
  previous_pinch_anchor_ = gfx::Point();
  client_->RenewTreePriority();
  pinch_gesture_end_should_clear_scrolling_layer_ = !CurrentlyScrollingLayer();
  if (active_tree_->OuterViewportScrollLayer()) {
    active_tree_->SetCurrentlyScrollingLayer(
        active_tree_->OuterViewportScrollLayer());
  } else {
    active_tree_->SetCurrentlyScrollingLayer(
        active_tree_->InnerViewportScrollLayer());
  }
  if (top_controls_manager_)
    top_controls_manager_->PinchBegin();
}

void LayerTreeHostImpl::PinchGestureUpdate(float magnify_delta,
                                           const gfx::Point& anchor) {
  if (!InnerViewportScrollLayer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::PinchGestureUpdate");

  // For a moment the scroll offset ends up being outside of the max range. This
  // confuses the delegate so we switch it off till after we're done processing
  // the pinch update.
  active_tree_->SetRootLayerScrollOffsetDelegate(NULL);

  // Keep the center-of-pinch anchor specified by (x, y) in a stable
  // position over the course of the magnify.
  float page_scale_delta = active_tree_->page_scale_delta();
  gfx::PointF previous_scale_anchor =
      gfx::ScalePoint(anchor, 1.f / page_scale_delta);
  active_tree_->SetPageScaleDelta(page_scale_delta * magnify_delta);
  page_scale_delta = active_tree_->page_scale_delta();
  gfx::PointF new_scale_anchor =
      gfx::ScalePoint(anchor, 1.f / page_scale_delta);
  gfx::Vector2dF move = previous_scale_anchor - new_scale_anchor;

  previous_pinch_anchor_ = anchor;

  move.Scale(1 / active_tree_->page_scale_factor());
  // If clamping the inner viewport scroll offset causes a change, it should
  // be accounted for from the intended move.
  move -= InnerViewportScrollLayer()->ClampScrollToMaxScrollOffset();

  // We manually manage the bubbling behaviour here as it is different to that
  // implemented in LayerTreeHostImpl::ScrollBy(). Specifically:
  // 1) we want to explicit limit the bubbling to the outer/inner viewports,
  // 2) we don't want the directional limitations on the unused parts that
  //    ScrollBy() implements, and
  // 3) pinching should not engage the top controls manager.
  gfx::Vector2dF unused = OuterViewportScrollLayer()
                              ? OuterViewportScrollLayer()->ScrollBy(move)
                              : move;

  if (!unused.IsZero()) {
    InnerViewportScrollLayer()->ScrollBy(unused);
    InnerViewportScrollLayer()->ClampScrollToMaxScrollOffset();
  }

  active_tree_->SetRootLayerScrollOffsetDelegate(
      root_layer_scroll_offset_delegate_);

  client_->SetNeedsCommitOnImplThread();
  SetNeedsRedraw();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::PinchGestureEnd() {
  pinch_gesture_active_ = false;
  if (pinch_gesture_end_should_clear_scrolling_layer_) {
    pinch_gesture_end_should_clear_scrolling_layer_ = false;
    ClearCurrentlyScrollingLayer();
  }
  if (top_controls_manager_)
    top_controls_manager_->PinchEnd();
  client_->SetNeedsCommitOnImplThread();
  // When a pinch ends, we may be displaying content cached at incorrect scales,
  // so updating draw properties and drawing will ensure we are using the right
  // scales that we want when we're not inside a pinch.
  active_tree_->set_needs_update_draw_properties();
  SetNeedsRedraw();
  // TODO(danakj): Don't set root damage. Just updating draw properties and
  // getting new tiles rastered should be enough! crbug.com/427423
  SetFullRootLayerDamage();
}

static void CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                                LayerImpl* layer_impl) {
  if (!layer_impl)
    return;

  gfx::Vector2d scroll_delta =
      gfx::ToFlooredVector2d(layer_impl->ScrollDelta());
  if (!scroll_delta.IsZero()) {
    LayerTreeHostCommon::ScrollUpdateInfo scroll;
    scroll.layer_id = layer_impl->id();
    scroll.scroll_delta = scroll_delta;
    scroll_info->scrolls.push_back(scroll);
    layer_impl->SetSentScrollDelta(scroll_delta);
  }

  for (size_t i = 0; i < layer_impl->children().size(); ++i)
    CollectScrollDeltas(scroll_info, layer_impl->children()[i]);
}

scoped_ptr<ScrollAndScaleSet> LayerTreeHostImpl::ProcessScrollDeltas() {
  scoped_ptr<ScrollAndScaleSet> scroll_info(new ScrollAndScaleSet());

  CollectScrollDeltas(scroll_info.get(), active_tree_->root_layer());
  scroll_info->page_scale_delta = active_tree_->page_scale_delta();
  active_tree_->set_sent_page_scale_delta(scroll_info->page_scale_delta);
  scroll_info->swap_promises.swap(swap_promises_for_main_thread_scroll_update_);
  scroll_info->top_controls_delta = active_tree()->top_controls_delta();
  active_tree_->set_sent_top_controls_delta(scroll_info->top_controls_delta);

  return scroll_info.Pass();
}

void LayerTreeHostImpl::SetFullRootLayerDamage() {
  SetViewportDamage(gfx::Rect(DrawViewportSize()));
}

void LayerTreeHostImpl::ScrollViewportInnerFirst(gfx::Vector2dF scroll_delta) {
  DCHECK(InnerViewportScrollLayer());
  LayerImpl* scroll_layer = InnerViewportScrollLayer();

  gfx::Vector2dF unused_delta = scroll_layer->ScrollBy(scroll_delta);
  if (!unused_delta.IsZero() && OuterViewportScrollLayer())
    OuterViewportScrollLayer()->ScrollBy(unused_delta);
}

void LayerTreeHostImpl::ScrollViewportBy(gfx::Vector2dF scroll_delta) {
  DCHECK(InnerViewportScrollLayer());
  LayerImpl* scroll_layer = OuterViewportScrollLayer()
                                ? OuterViewportScrollLayer()
                                : InnerViewportScrollLayer();

  gfx::Vector2dF unused_delta = scroll_layer->ScrollBy(scroll_delta);

  if (!unused_delta.IsZero() && (scroll_layer == OuterViewportScrollLayer()))
    InnerViewportScrollLayer()->ScrollBy(unused_delta);
}

void LayerTreeHostImpl::AnimatePageScale(base::TimeTicks monotonic_time) {
  if (!page_scale_animation_)
    return;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();

  if (!page_scale_animation_->IsAnimationStarted())
    page_scale_animation_->StartAnimation(monotonic_time);

  active_tree_->SetPageScaleDelta(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time) /
      active_tree_->page_scale_factor());
  gfx::ScrollOffset next_scroll = gfx::ScrollOffset(
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time));

  ScrollViewportInnerFirst(next_scroll.DeltaFrom(scroll_total));
  SetNeedsRedraw();

  if (page_scale_animation_->IsAnimationCompleteAtTime(monotonic_time)) {
    page_scale_animation_ = nullptr;
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
  } else {
    SetNeedsAnimate();
  }
}

void LayerTreeHostImpl::AnimateTopControls(base::TimeTicks time) {
  if (!top_controls_manager_ || !top_controls_manager_->animation())
    return;

  gfx::Vector2dF scroll = top_controls_manager_->Animate(time);

  if (top_controls_manager_->animation())
    SetNeedsAnimate();

  if (active_tree_->TotalScrollOffset().y() == 0.f)
    return;

  if (scroll.IsZero())
    return;

  ScrollViewportBy(gfx::ScaleVector2d(
      scroll, 1.f / active_tree_->total_page_scale_factor()));
  SetNeedsRedraw();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time) {
  if (!settings_.accelerated_animation_enabled ||
      !needs_animate_layers() ||
      !active_tree_->root_layer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::AnimateLayers");
  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->Animate(monotonic_time);

  SetNeedsAnimate();
}

void LayerTreeHostImpl::UpdateAnimationState(bool start_ready_animations) {
  if (!settings_.accelerated_animation_enabled ||
      !needs_animate_layers() ||
      !active_tree_->root_layer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::UpdateAnimationState");
  scoped_ptr<AnimationEventsVector> events =
      make_scoped_ptr(new AnimationEventsVector);
  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->UpdateState(start_ready_animations, events.get());

  if (!events->empty()) {
    client_->PostAnimationEventsToMainThreadOnImplThread(events.Pass());
  }

  SetNeedsAnimate();
}

void LayerTreeHostImpl::ActivateAnimations() {
  if (!settings_.accelerated_animation_enabled || !needs_animate_layers() ||
      !active_tree_->root_layer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::ActivateAnimations");
  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->ActivateAnimations();
}

base::TimeDelta LayerTreeHostImpl::LowFrequencyAnimationInterval() const {
  return base::TimeDelta::FromSeconds(1);
}

std::string LayerTreeHostImpl::LayerTreeAsJson() const {
  std::string str;
  if (active_tree_->root_layer()) {
    scoped_ptr<base::Value> json(active_tree_->root_layer()->LayerTreeAsJson());
    base::JSONWriter::WriteWithOptions(
        json.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
  }
  return str;
}

int LayerTreeHostImpl::SourceAnimationFrameNumber() const {
  return fps_counter_->current_frame_number();
}

void LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks time) {
  AnimateScrollbarsRecursive(active_tree_->root_layer(), time);
}

void LayerTreeHostImpl::AnimateScrollbarsRecursive(LayerImpl* layer,
                                                   base::TimeTicks time) {
  if (!layer)
    return;

  ScrollbarAnimationController* scrollbar_controller =
      layer->scrollbar_animation_controller();
  if (scrollbar_controller)
    scrollbar_controller->Animate(time);

  for (size_t i = 0; i < layer->children().size(); ++i)
    AnimateScrollbarsRecursive(layer->children()[i], time);
}

void LayerTreeHostImpl::PostDelayedScrollbarFade(
    const base::Closure& start_fade,
    base::TimeDelta delay) {
  client_->PostDelayedScrollbarFadeOnImplThread(start_fade, delay);
}

void LayerTreeHostImpl::SetNeedsScrollbarAnimationFrame() {
  TRACE_EVENT_INSTANT0(
      "cc",
      "LayerTreeHostImpl::SetNeedsRedraw due to scrollbar fade",
      TRACE_EVENT_SCOPE_THREAD);
  SetNeedsAnimate();
}

void LayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  if (!tile_manager_)
    return;

  if (global_tile_state_.tree_priority == priority)
    return;
  global_tile_state_.tree_priority = priority;
  DidModifyTilePriorities();
}

TreePriority LayerTreeHostImpl::GetTreePriority() const {
  return global_tile_state_.tree_priority;
}

void LayerTreeHostImpl::UpdateCurrentBeginFrameArgs(
    const BeginFrameArgs& args) {
  DCHECK(!current_begin_frame_args_.IsValid());
  current_begin_frame_args_ = args;
  // TODO(skyostil): Stop overriding the frame time once the usage of frame
  // timing is unified.
  current_begin_frame_args_.frame_time = gfx::FrameTime::Now();
}

void LayerTreeHostImpl::ResetCurrentBeginFrameArgsForNextFrame() {
  current_begin_frame_args_ = BeginFrameArgs();
}

BeginFrameArgs LayerTreeHostImpl::CurrentBeginFrameArgs() const {
  // Try to use the current frame time to keep animations non-jittery.  But if
  // we're not in a frame (because this is during an input event or a delayed
  // task), fall back to physical time.  This should still be monotonic.
  if (current_begin_frame_args_.IsValid())
    return current_begin_frame_args_;
  return BeginFrameArgs::Create(gfx::FrameTime::Now(),
                                base::TimeTicks(),
                                BeginFrameArgs::DefaultInterval());
}

void LayerTreeHostImpl::AsValueInto(base::debug::TracedValue* value) const {
  return AsValueWithFrameInto(NULL, value);
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
LayerTreeHostImpl::AsValue() const {
  return AsValueWithFrame(NULL);
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
LayerTreeHostImpl::AsValueWithFrame(FrameData* frame) const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  AsValueWithFrameInto(frame, state.get());
  return state;
}

void LayerTreeHostImpl::AsValueWithFrameInto(
    FrameData* frame,
    base::debug::TracedValue* state) const {
  if (this->pending_tree_) {
    state->BeginDictionary("activation_state");
    ActivationStateAsValueInto(state);
    state->EndDictionary();
  }
  state->BeginDictionary("device_viewport_size");
  MathUtil::AddToTracedValue(device_viewport_size_, state);
  state->EndDictionary();

  std::set<const Tile*> tiles;
  active_tree_->GetAllTilesForTracing(&tiles);
  if (pending_tree_)
    pending_tree_->GetAllTilesForTracing(&tiles);

  state->BeginArray("active_tiles");
  for (std::set<const Tile*>::const_iterator it = tiles.begin();
       it != tiles.end();
       ++it) {
    const Tile* tile = *it;

    state->BeginDictionary();
    tile->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();

  if (tile_manager_) {
    state->BeginDictionary("tile_manager_basic_state");
    tile_manager_->BasicStateAsValueInto(state);
    state->EndDictionary();
  }
  state->BeginDictionary("active_tree");
  active_tree_->AsValueInto(state);
  state->EndDictionary();
  if (pending_tree_) {
    state->BeginDictionary("pending_tree");
    pending_tree_->AsValueInto(state);
    state->EndDictionary();
  }
  if (frame) {
    state->BeginDictionary("frame");
    frame->AsValueInto(state);
    state->EndDictionary();
  }
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
LayerTreeHostImpl::ActivationStateAsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  ActivationStateAsValueInto(state.get());
  return state;
}

void LayerTreeHostImpl::ActivationStateAsValueInto(
    base::debug::TracedValue* state) const {
  TracedValue::SetIDRef(this, state, "lthi");
  if (tile_manager_) {
    state->BeginDictionary("tile_manager");
    tile_manager_->BasicStateAsValueInto(state);
    state->EndDictionary();
  }
}

void LayerTreeHostImpl::SetDebugState(
    const LayerTreeDebugState& new_debug_state) {
  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;
  if (debug_state_.continuous_painting != new_debug_state.continuous_painting)
    paint_time_counter_->ClearHistory();

  debug_state_ = new_debug_state;
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  SetFullRootLayerDamage();
}

void LayerTreeHostImpl::CreateUIResource(UIResourceId uid,
                                         const UIResourceBitmap& bitmap) {
  DCHECK_GT(uid, 0);

  GLint wrap_mode = 0;
  switch (bitmap.GetWrapMode()) {
    case UIResourceBitmap::CLAMP_TO_EDGE:
      wrap_mode = GL_CLAMP_TO_EDGE;
      break;
    case UIResourceBitmap::REPEAT:
      wrap_mode = GL_REPEAT;
      break;
  }

  // Allow for multiple creation requests with the same UIResourceId.  The
  // previous resource is simply deleted.
  ResourceProvider::ResourceId id = ResourceIdForUIResource(uid);
  if (id)
    DeleteUIResource(uid);

  ResourceFormat format = resource_provider_->best_texture_format();
  switch (bitmap.GetFormat()) {
    case UIResourceBitmap::RGBA8:
      break;
    case UIResourceBitmap::ALPHA_8:
      format = ALPHA_8;
      break;
    case UIResourceBitmap::ETC1:
      format = ETC1;
      break;
  }
  id =
      resource_provider_->CreateResource(bitmap.GetSize(),
                                         wrap_mode,
                                         ResourceProvider::TextureHintImmutable,
                                         format);

  UIResourceData data;
  data.resource_id = id;
  data.size = bitmap.GetSize();
  data.opaque = bitmap.GetOpaque();

  ui_resource_map_[uid] = data;

  AutoLockUIResourceBitmap bitmap_lock(bitmap);
  resource_provider_->SetPixels(id,
                                bitmap_lock.GetPixels(),
                                gfx::Rect(bitmap.GetSize()),
                                gfx::Rect(bitmap.GetSize()),
                                gfx::Vector2d(0, 0));
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  ResourceProvider::ResourceId id = ResourceIdForUIResource(uid);
  if (id) {
    resource_provider_->DeleteResource(id);
    ui_resource_map_.erase(uid);
  }
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::EvictAllUIResources() {
  if (ui_resource_map_.empty())
    return;

  for (UIResourceMap::const_iterator iter = ui_resource_map_.begin();
      iter != ui_resource_map_.end();
      ++iter) {
    evicted_ui_resources_.insert(iter->first);
    resource_provider_->DeleteResource(iter->second.resource_id);
  }
  ui_resource_map_.clear();

  client_->SetNeedsCommitOnImplThread();
  client_->OnCanDrawStateChanged(CanDraw());
  client_->RenewTreePriority();
}

ResourceProvider::ResourceId LayerTreeHostImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  UIResourceMap::const_iterator iter = ui_resource_map_.find(uid);
  if (iter != ui_resource_map_.end())
    return iter->second.resource_id;
  return 0;
}

bool LayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid) const {
  UIResourceMap::const_iterator iter = ui_resource_map_.find(uid);
  DCHECK(iter != ui_resource_map_.end());
  return iter->second.opaque;
}

bool LayerTreeHostImpl::EvictedUIResourcesExist() const {
  return !evicted_ui_resources_.empty();
}

void LayerTreeHostImpl::MarkUIResourceNotEvicted(UIResourceId uid) {
  std::set<UIResourceId>::iterator found_in_evicted =
      evicted_ui_resources_.find(uid);
  if (found_in_evicted == evicted_ui_resources_.end())
    return;
  evicted_ui_resources_.erase(found_in_evicted);
  if (evicted_ui_resources_.empty())
    client_->OnCanDrawStateChanged(CanDraw());
}

void LayerTreeHostImpl::ScheduleMicroBenchmark(
    scoped_ptr<MicroBenchmarkImpl> benchmark) {
  micro_benchmark_controller_.ScheduleRun(benchmark.Pass());
}

void LayerTreeHostImpl::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHostImpl::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfSetNeedsRedraw() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsRedrawOnImpl();
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfForwardingToMainThread() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnForwardScrollUpdateToMainThreadOnImpl();
}

void LayerTreeHostImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  DCHECK(std::find(picture_layers_.begin(), picture_layers_.end(), layer) ==
         picture_layers_.end());
  picture_layers_.push_back(layer);
}

void LayerTreeHostImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  std::vector<PictureLayerImpl*>::iterator it =
      std::find(picture_layers_.begin(), picture_layers_.end(), layer);
  DCHECK(it != picture_layers_.end());
  picture_layers_.erase(it);
}

}  // namespace cc
