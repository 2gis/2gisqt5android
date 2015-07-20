// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/browser/android/compositor_client.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/base/android/window_android.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/frame_time.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace content {

namespace {

const unsigned int kMaxSwapBuffers = 2U;

// Used to override capabilities_.adjust_deadline_for_parent to false
class OutputSurfaceWithoutParent : public cc::OutputSurface {
 public:
  OutputSurfaceWithoutParent(
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      base::WeakPtr<CompositorImpl> compositor_impl)
      : cc::OutputSurface(context_provider),
        swap_buffers_completion_callback_(
            base::Bind(&OutputSurfaceWithoutParent::OnSwapBuffersCompleted,
                       base::Unretained(this))) {
    capabilities_.adjust_deadline_for_parent = false;
    compositor_impl_ = compositor_impl;
    main_thread_ = base::MessageLoopProxy::current();
  }

  virtual void SwapBuffers(cc::CompositorFrame* frame) override {
    GetCommandBufferProxy()->SetLatencyInfo(frame->metadata.latency_info);
    DCHECK(frame->gl_frame_data->sub_buffer_rect ==
           gfx::Rect(frame->gl_frame_data->size));
    context_provider_->ContextSupport()->Swap();
    client_->DidSwapBuffers();
  }

  virtual bool BindToClient(cc::OutputSurfaceClient* client) override {
    if (!OutputSurface::BindToClient(client))
      return false;

    GetCommandBufferProxy()->SetSwapBuffersCompletionCallback(
        swap_buffers_completion_callback_.callback());

    main_thread_->PostTask(
        FROM_HERE,
        base::Bind(&CompositorImpl::PopulateGpuCapabilities,
                   compositor_impl_,
                   context_provider_->ContextCapabilities().gpu));

    return true;
  }

 private:
  CommandBufferProxyImpl* GetCommandBufferProxy() {
    ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<content::ContextProviderCommandBuffer*>(
            context_provider_.get());
    CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    return command_buffer_proxy;
  }

  void OnSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info) {
    RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
    OutputSurface::OnSwapBuffersComplete();
  }

  base::CancelableCallback<void(const std::vector<ui::LatencyInfo>&)>
      swap_buffers_completion_callback_;

  scoped_refptr<base::MessageLoopProxy> main_thread_;
  base::WeakPtr<CompositorImpl> compositor_impl_;
};

static bool g_initialized = false;

} // anonymous namespace

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : root_layer_(cc::Layer::Create()),
      has_transparent_background_(false),
      device_scale_factor_(1),
      window_(NULL),
      surface_id_(0),
      client_(client),
      root_window_(root_window),
      did_post_swapbuffers_(false),
      ignore_schedule_composite_(false),
      needs_composite_(false),
      needs_animate_(false),
      will_composite_immediately_(false),
      composite_on_vsync_trigger_(DO_NOT_COMPOSITE),
      pending_swapbuffers_(0U),
      weak_factory_(this) {
  DCHECK(client);
  DCHECK(root_window);
  ImageTransportFactoryAndroid::AddObserver(this);
  root_window->AttachCompositor(this);
}

CompositorImpl::~CompositorImpl() {
  root_window_->DetachCompositor();
  ImageTransportFactoryAndroid::RemoveObserver(this);
  // Clean-up any surface references.
  SetSurface(NULL);
}

void CompositorImpl::PostComposite(CompositingTrigger trigger) {
  DCHECK(needs_composite_);
  DCHECK(trigger == COMPOSITE_IMMEDIATELY || trigger == COMPOSITE_EVENTUALLY);

  if (will_composite_immediately_ ||
      (trigger == COMPOSITE_EVENTUALLY && WillComposite())) {
    // We will already composite soon enough.
    DCHECK(WillComposite());
    return;
  }

  if (DidCompositeThisFrame()) {
    DCHECK(!WillCompositeThisFrame());
    if (composite_on_vsync_trigger_ != COMPOSITE_IMMEDIATELY) {
      composite_on_vsync_trigger_ = trigger;
      root_window_->RequestVSyncUpdate();
    }
    DCHECK(WillComposite());
    return;
  }

  base::TimeDelta delay;
  if (trigger == COMPOSITE_IMMEDIATELY) {
    will_composite_immediately_ = true;
    composite_on_vsync_trigger_ = DO_NOT_COMPOSITE;
  } else {
    DCHECK(!WillComposite());
    const base::TimeDelta estimated_composite_time = vsync_period_ / 4;
    const base::TimeTicks now = base::TimeTicks::Now();

    if (!last_vsync_.is_null() && (now - last_vsync_) < vsync_period_) {
      base::TimeTicks next_composite =
          last_vsync_ + vsync_period_ - estimated_composite_time;
      if (next_composite < now) {
        // It's too late, we will reschedule composite as needed on the next
        // vsync.
        composite_on_vsync_trigger_ = COMPOSITE_EVENTUALLY;
        root_window_->RequestVSyncUpdate();
        DCHECK(WillComposite());
        return;
      }

      delay = next_composite - now;
    }
  }
  TRACE_EVENT2("cc", "CompositorImpl::PostComposite",
               "trigger", trigger,
               "delay", delay.InMillisecondsF());

  DCHECK(composite_on_vsync_trigger_ == DO_NOT_COMPOSITE);
  if (current_composite_task_)
    current_composite_task_->Cancel();

  // Unretained because we cancel the task on shutdown.
  current_composite_task_.reset(new base::CancelableClosure(
      base::Bind(&CompositorImpl::Composite, base::Unretained(this), trigger)));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, current_composite_task_->callback(), delay);
}

void CompositorImpl::Composite(CompositingTrigger trigger) {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  if (!factory->GetGpuChannel() || factory->GetGpuChannel()->IsLost()) {
    CauseForGpuLaunch cause =
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
    factory->EstablishGpuChannel(cause,
                                 base::Bind(&CompositorImpl::ScheduleComposite,
                                            weak_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(host_);
  DCHECK(trigger == COMPOSITE_IMMEDIATELY || trigger == COMPOSITE_EVENTUALLY);
  DCHECK(needs_composite_);
  DCHECK(!DidCompositeThisFrame());

  if (trigger == COMPOSITE_IMMEDIATELY)
    will_composite_immediately_ = false;

  DCHECK_LE(pending_swapbuffers_, kMaxSwapBuffers);
  if (pending_swapbuffers_ == kMaxSwapBuffers) {
    TRACE_EVENT0("compositor", "CompositorImpl_SwapLimit");
    return;
  }

  // Reset state before Layout+Composite since that might create more
  // requests to Composite that we need to respect.
  needs_composite_ = false;

  // Only allow compositing once per vsync.
  current_composite_task_->Cancel();
  DCHECK(DidCompositeThisFrame() && !WillComposite());

  // Ignore ScheduleComposite() from layer tree changes during layout and
  // animation updates that will already be reflected in the current frame
  // we are about to draw.
  ignore_schedule_composite_ = true;

  const base::TimeTicks frame_time = gfx::FrameTime::Now();
  if (needs_animate_) {
    needs_animate_ = false;
    root_window_->Animate(frame_time);
  }
  ignore_schedule_composite_ = false;

  did_post_swapbuffers_ = false;
  host_->Composite(frame_time);
  if (did_post_swapbuffers_)
    pending_swapbuffers_++;

  // Need to track vsync to avoid compositing more than once per frame.
  root_window_->RequestVSyncUpdate();
}

UIResourceProvider& CompositorImpl::GetUIResourceProvider() {
  return ui_resource_provider_;
}

ui::SystemUIResourceManager& CompositorImpl::GetSystemUIResourceManager() {
  return ui_resource_provider_.GetSystemUIResourceManager();
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (subroot_layer_.get()) {
    subroot_layer_->RemoveFromParent();
    subroot_layer_ = NULL;
  }
  if (root_layer.get()) {
    subroot_layer_ = root_layer;
    root_layer_->AddChild(root_layer);
  }
}

void CompositorImpl::SetWindowSurface(ANativeWindow* window) {
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  if (window_) {
    tracker->RemoveSurface(surface_id_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_id_ = 0;
    SetVisible(false);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    surface_id_ = tracker->AddSurfaceForNativeWidget(window);
    tracker->SetSurfaceHandle(
        surface_id_,
        gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT));
    SetVisible(true);
  }
}

void CompositorImpl::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_surface(env, surface);

  // First, cleanup any existing surface references.
  if (surface_id_)
    UnregisterViewSurface(surface_id_);
  SetWindowSurface(NULL);

  // Now, set the new surface if we have one.
  ANativeWindow* window = NULL;
  if (surface) {
    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window = ANativeWindow_fromSurface(env, surface);
  }
  if (window) {
    SetWindowSurface(window);
    ANativeWindow_release(window);
    RegisterViewSurface(surface_id_, j_surface.obj());
  }
}

void CompositorImpl::SetVisible(bool visible) {
  if (!visible) {
    DCHECK(host_);
    // Look for any layers that were attached to the root for readback
    // and are waiting for Composite() to happen.
    bool readback_pending = false;
    for (size_t i = 0; i < root_layer_->children().size(); ++i) {
      if (root_layer_->children()[i]->HasCopyRequest()) {
        readback_pending = true;
        break;
      }
    }
    if (readback_pending) {
      ignore_schedule_composite_ = true;
      host_->Composite(base::TimeTicks::Now());
      ignore_schedule_composite_ = false;
    }
    if (WillComposite())
      CancelComposite();
    ui_resource_provider_.SetLayerTreeHost(NULL);
    host_.reset();
  } else if (!host_) {
    DCHECK(!WillComposite());
    needs_composite_ = false;
    pending_swapbuffers_ = 0;
    cc::LayerTreeSettings settings;
    settings.refresh_rate = 60.0;
    settings.impl_side_painting = false;
    settings.allow_antialiasing = false;
    settings.calculate_top_controls_position = false;
    settings.top_controls_height = 0.f;
    settings.highp_threshold_min = 2048;

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    settings.initial_debug_state.SetRecordRenderingStats(
        command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
    settings.initial_debug_state.show_fps_counter =
        command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
    // TODO(enne): Update this this compositor to use the scheduler.
    settings.single_thread_proxy_scheduler = false;

    host_ = cc::LayerTreeHost::CreateSingleThreaded(
        this,
        this,
        HostSharedBitmapManager::current(),
        BrowserGpuMemoryBufferManager::current(),
        settings,
        base::MessageLoopProxy::current());
    host_->SetRootLayer(root_layer_);

    host_->SetVisible(true);
    host_->SetLayerTreeHostClientReady();
    host_->SetViewportSize(size_);
    host_->set_has_transparent_background(has_transparent_background_);
    host_->SetDeviceScaleFactor(device_scale_factor_);
    ui_resource_provider_.SetLayerTreeHost(host_.get());
  }
}

void CompositorImpl::setDeviceScaleFactor(float factor) {
  device_scale_factor_ = factor;
  if (host_)
    host_->SetDeviceScaleFactor(factor);
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->SetViewportSize(size);
  root_layer_->SetBounds(size);
}

void CompositorImpl::SetHasTransparentBackground(bool flag) {
  has_transparent_background_ = flag;
  if (host_)
    host_->set_has_transparent_background(flag);
}

void CompositorImpl::SetNeedsComposite() {
  if (!host_.get())
    return;
  DCHECK(!needs_composite_ || WillComposite());

  needs_composite_ = true;
  PostComposite(COMPOSITE_IMMEDIATELY);
}

static scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
CreateGpuProcessViewContext(
    const scoped_refptr<GpuChannelHost>& gpu_channel_host,
    const blink::WebGraphicsContext3D::Attributes attributes,
    int surface_id) {
  DCHECK(gpu_channel_host.get());

  GURL url("chrome://gpu/Compositor::createContext3D");
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes =
      display_info.GetDisplayHeight() *
      display_info.GetDisplayWidth() *
      kBytesPerPixel;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  limits.command_buffer_size = 64 * 1024;
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = std::min(
      3 * full_screen_texture_size_in_bytes, kDefaultMaxTransferBufferSize);
  limits.mapped_memory_reclaim_limit = 2 * 1024 * 1024;
  bool lose_context_when_out_of_memory = true;
  return make_scoped_ptr(
      new WebGraphicsContext3DCommandBufferImpl(surface_id,
                                                url,
                                                gpu_channel_host.get(),
                                                attributes,
                                                lose_context_when_out_of_memory,
                                                limits,
                                                NULL));
}

void CompositorImpl::Layout() {
  ignore_schedule_composite_ = true;
  client_->Layout();
  ignore_schedule_composite_ = false;
}

void CompositorImpl::RequestNewOutputSurface(bool fallback) {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  if (!factory->GetGpuChannel() || factory->GetGpuChannel()->IsLost()) {
    CauseForGpuLaunch cause =
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
    factory->EstablishGpuChannel(
        cause,
        base::Bind(&CompositorImpl::CreateOutputSurface,
                   weak_factory_.GetWeakPtr(),
                   fallback));
    return;
  }

  CreateOutputSurface(fallback);
}

void CompositorImpl::CreateOutputSurface(bool fallback) {
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.noAutomaticFlushes = true;
  pending_swapbuffers_ = 0;

  DCHECK(window_);
  DCHECK(surface_id_);

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  scoped_refptr<GpuChannelHost> gpu_channel_host = factory->GetGpuChannel();
  if (gpu_channel_host.get() && !gpu_channel_host->IsLost()) {
    context_provider = ContextProviderCommandBuffer::Create(
        CreateGpuProcessViewContext(gpu_channel_host, attrs, surface_id_),
        "BrowserCompositor");
  }
  if (!context_provider.get()) {
    LOG(ERROR) << "Failed to create 3D context for compositor.";
    host_->SetOutputSurface(scoped_ptr<cc::OutputSurface>());
    return;
  }

  host_->SetOutputSurface(
      scoped_ptr<cc::OutputSurface>(new OutputSurfaceWithoutParent(
          context_provider, weak_factory_.GetWeakPtr())));
}

void CompositorImpl::PopulateGpuCapabilities(
    gpu::Capabilities gpu_capabilities) {
  ui_resource_provider_.SetSupportsETC1NonPowerOfTwo(
      gpu_capabilities.texture_format_etc1_npot);
}

void CompositorImpl::OnLostResources() {
  client_->DidLoseResources();
}

void CompositorImpl::ScheduleComposite() {
  DCHECK(!needs_composite_ || WillComposite());
  if (ignore_schedule_composite_)
    return;

  needs_composite_ = true;
  // We currently expect layer tree invalidations at most once per frame
  // during normal operation and therefore try to composite immediately
  // to minimize latency.
  PostComposite(COMPOSITE_IMMEDIATELY);
}

void CompositorImpl::ScheduleAnimation() {
  DCHECK(!needs_composite_ || WillComposite());
  needs_animate_ = true;

  if (needs_composite_)
    return;

  TRACE_EVENT0("cc", "CompositorImpl::ScheduleAnimation");
  needs_composite_ = true;
  PostComposite(COMPOSITE_EVENTUALLY);
}

void CompositorImpl::DidPostSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidPostSwapBuffers");
  did_post_swapbuffers_ = true;
}

void CompositorImpl::DidCompleteSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidCompleteSwapBuffers");
  DCHECK_GT(pending_swapbuffers_, 0U);
  if (pending_swapbuffers_-- == kMaxSwapBuffers && needs_composite_)
    PostComposite(COMPOSITE_IMMEDIATELY);
  client_->OnSwapBuffersCompleted(pending_swapbuffers_);
}

void CompositorImpl::DidAbortSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidAbortSwapBuffers");
  // This really gets called only once from
  // SingleThreadProxy::DidLoseOutputSurfaceOnImplThread() when the
  // context was lost.
  ScheduleComposite();
  client_->OnSwapBuffersCompleted(0);
}

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

void CompositorImpl::AttachLayerForReadback(scoped_refptr<cc::Layer> layer) {
  root_layer_->AddChild(layer);
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    scoped_ptr<cc::CopyOutputRequest> request) {
  root_layer_->RequestCopyOfOutput(request.Pass());
}

void CompositorImpl::OnVSync(base::TimeTicks frame_time,
                             base::TimeDelta vsync_period) {
  vsync_period_ = vsync_period;
  last_vsync_ = frame_time;

  if (WillCompositeThisFrame()) {
    // We somehow missed the last vsync interval, so reschedule for deadline.
    // We cannot schedule immediately, or will get us out-of-phase with new
    // renderer frames.
    CancelComposite();
    composite_on_vsync_trigger_ = COMPOSITE_EVENTUALLY;
  } else {
    current_composite_task_.reset();
  }

  DCHECK(!DidCompositeThisFrame() && !WillCompositeThisFrame());
  if (composite_on_vsync_trigger_ != DO_NOT_COMPOSITE) {
    CompositingTrigger trigger = composite_on_vsync_trigger_;
    composite_on_vsync_trigger_ = DO_NOT_COMPOSITE;
    PostComposite(trigger);
  }
}

void CompositorImpl::SetNeedsAnimate() {
  if (!host_)
    return;

  host_->SetNeedsAnimate();
}

}  // namespace content
