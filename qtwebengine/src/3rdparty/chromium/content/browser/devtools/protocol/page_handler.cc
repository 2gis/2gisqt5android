// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/page_handler.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/protocol/color_picker.h"
#include "content/browser/devtools/protocol/usage_and_quota_query.h"
#include "content/browser/geolocation/geolocation_dispatcher_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace content {
namespace devtools {
namespace page {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;
static int kFrameRateThresholdMs = 100;
static int kCaptureRetryLimit = 2;

void QueryUsageAndQuotaCompletedOnIOThread(
  const UsageAndQuotaQuery::Callback& callback,
  scoped_ptr<QueryUsageAndQuotaResponse> response) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, base::Passed(&response)));
}

void QueryUsageAndQuotaOnIOThread(
    scoped_refptr<storage::QuotaManager> quota_manager,
    const GURL& security_origin,
    const UsageAndQuotaQuery::Callback& callback) {
  new UsageAndQuotaQuery(
      quota_manager,
      security_origin,
      base::Bind(&QueryUsageAndQuotaCompletedOnIOThread,
                 callback));
}

}  // namespace

typedef DevToolsProtocolClient::Response Response;

PageHandler::PageHandler()
    : enabled_(false),
      touch_emulation_enabled_(false),
      screencast_enabled_(false),
      screencast_quality_(kDefaultScreenshotQuality),
      screencast_max_width_(-1),
      screencast_max_height_(-1),
      capture_retry_count_(0),
      has_last_compositor_frame_metadata_(false),
      color_picker_(new ColorPicker(base::Bind(
          &PageHandler::OnColorPicked, base::Unretained(this)))),
      host_(nullptr),
      weak_factory_(this) {
}

PageHandler::~PageHandler() {
}

void PageHandler::SetRenderViewHost(RenderViewHostImpl* host) {
  if (host_ == host)
    return;

  color_picker_->SetRenderViewHost(host);
  host_ = host;
  UpdateTouchEventEmulationState();
}

void PageHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void PageHandler::Detached() {
  Disable();
}

void PageHandler::OnSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  last_compositor_frame_metadata_ = frame_metadata;
  has_last_compositor_frame_metadata_ = true;

  if (screencast_enabled_)
    InnerSwapCompositorFrame();
  color_picker_->OnSwapCompositorFrame();
}

void PageHandler::OnVisibilityChanged(bool visible) {
  if (!screencast_enabled_)
    return;
  NotifyScreencastVisibility(visible);
}

void PageHandler::DidAttachInterstitialPage() {
  if (!enabled_)
    return;
  InterstitialShownParams params;
  client_->InterstitialShown(params);
}

void PageHandler::DidDetachInterstitialPage() {
  if (!enabled_)
    return;
  InterstitialHiddenParams params;
  client_->InterstitialHidden(params);
}

Response PageHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

Response PageHandler::Disable() {
  enabled_ = false;
  touch_emulation_enabled_ = false;
  screencast_enabled_ = false;
  UpdateTouchEventEmulationState();
  color_picker_->SetEnabled(false);
  return Response::FallThrough();
}

Response PageHandler::Reload(const bool* ignoreCache,
                             const std::string* script_to_evaluate_on_load,
                             const std::string* script_preprocessor) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (!web_contents)
    return Response::InternalError("No WebContents to reload");

  // Handle in browser only if it is crashed.
  if (!web_contents->IsCrashed())
    return Response::FallThrough();

  web_contents->GetController().Reload(false);
  return Response::OK();
}

Response PageHandler::Navigate(const std::string& url,
                               FrameId* frame_id) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return Response::InternalError("Cannot navigate to invalid URL");

  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (!web_contents)
    return Response::InternalError("No WebContents to navigate");

  web_contents->GetController()
      .LoadURL(gurl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  return Response::FallThrough();
}

Response PageHandler::GetNavigationHistory(
    int* current_index,
    std::vector<NavigationEntry>* entries) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (!web_contents)
    return Response::InternalError("No WebContents to navigate");

  NavigationController& controller = web_contents->GetController();
  *current_index = controller.GetCurrentEntryIndex();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    NavigationEntry entry;
    entry.set_id(controller.GetEntryAtIndex(i)->GetUniqueID());
    entry.set_url(controller.GetEntryAtIndex(i)->GetURL().spec());
    entry.set_title(
        base::UTF16ToUTF8(controller.GetEntryAtIndex(i)->GetTitle()));
    entries->push_back(entry);
  }
  return Response::OK();
}

Response PageHandler::NavigateToHistoryEntry(int entry_id) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (!web_contents)
    return Response::InternalError("No WebContents to navigate");

  NavigationController& controller = web_contents->GetController();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
      controller.GoToIndex(i);
      return Response::OK();
    }
  }

  return Response::InvalidParams("No entry with passed id");
}

Response PageHandler::SetGeolocationOverride(double* latitude,
                                             double* longitude,
                                             double* accuracy) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(host_));
  if (!web_contents)
    return Response::InternalError("No WebContents to override");

  GeolocationDispatcherHost* geolocation_host =
      web_contents->geolocation_dispatcher_host();
  scoped_ptr<Geoposition> geoposition(new Geoposition());
  if (latitude && longitude && accuracy) {
    geoposition->latitude = *latitude;
    geoposition->longitude = *longitude;
    geoposition->accuracy = *accuracy;
    geoposition->timestamp = base::Time::Now();
    if (!geoposition->Validate()) {
      return Response::InternalError("Invalid geolocation");
    }
  } else {
    geoposition->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  }
  geolocation_host->SetOverride(geoposition.Pass());
  return Response::OK();
}

Response PageHandler::ClearGeolocationOverride() {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(host_));
  if (!web_contents)
    return Response::InternalError("No WebContents to override");

  GeolocationDispatcherHost* geolocation_host =
      web_contents->geolocation_dispatcher_host();
  geolocation_host->ClearOverride();
  return Response::OK();
}

Response PageHandler::SetTouchEmulationEnabled(bool enabled) {
  touch_emulation_enabled_ = enabled;
  UpdateTouchEventEmulationState();
  return Response::FallThrough();
}

Response PageHandler::SetTouchEmulationEnabled(
    bool enabled, const std::string* configuration) {
  touch_emulation_enabled_ = enabled;
  touch_emulation_configuration_ =
      configuration ? *configuration : std::string();
  UpdateTouchEventEmulationState();
  return Response::FallThrough();
}

scoped_refptr<DevToolsProtocol::Response> PageHandler::CaptureScreenshot(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_ || !host_->GetView())
    return command->InternalErrorResponse("Could not connect to view");

  host_->GetSnapshotFromBrowser(
      base::Bind(&PageHandler::ScreenshotCaptured,
          weak_factory_.GetWeakPtr(), command));
  return command->AsyncResponsePromise();
}

Response PageHandler::CanScreencast(bool* result) {
#if defined(OS_ANDROID)
  *result = true;
#else
  *result = false;
#endif  // defined(OS_ANDROID)
  return Response::OK();
}

Response PageHandler::CanEmulate(bool* result) {
#if defined(OS_ANDROID)
  *result = false;
#else
  if (host_) {
    if (WebContents* web_contents = WebContents::FromRenderViewHost(host_)) {
      *result = !web_contents->GetVisibleURL().SchemeIs(kChromeDevToolsScheme);
    } else {
      *result = true;
    }
  } else {
    *result = true;
  }
#endif  // defined(OS_ANDROID)
  return Response::OK();
}

Response PageHandler::StartScreencast(const std::string* format,
                                      const int* quality,
                                      const int* max_width,
                                      const int* max_height) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  screencast_enabled_ = true;
  screencast_format_ = format ? *format : kPng;
  screencast_quality_ = quality ? *quality : kDefaultScreenshotQuality;
  if (screencast_quality_ < 0 || screencast_quality_ > 100)
    screencast_quality_ = kDefaultScreenshotQuality;
  screencast_max_width_ = max_width ? *max_width : -1;
  screencast_max_height_ = max_height ? *max_height : -1;

  UpdateTouchEventEmulationState();
  bool visible = !host_->is_hidden();
  NotifyScreencastVisibility(visible);
  if (visible) {
    if (has_last_compositor_frame_metadata_)
      InnerSwapCompositorFrame();
    else
      host_->Send(new ViewMsg_ForceRedraw(host_->GetRoutingID(), 0));
  }
  return Response::FallThrough();
}

Response PageHandler::StopScreencast() {
  last_frame_time_ = base::TimeTicks();
  screencast_enabled_ = false;
  UpdateTouchEventEmulationState();
  return Response::FallThrough();
}

Response PageHandler::HandleJavaScriptDialog(bool accept,
                                             const std::string* prompt_text) {
  base::string16 prompt_override;
  if (prompt_text)
    prompt_override = base::UTF8ToUTF16(*prompt_text);

  if (!host_)
    return Response::InternalError("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (!web_contents)
    return Response::InternalError("No JavaScript dialog to handle");

  JavaScriptDialogManager* manager =
      web_contents->GetDelegate()->GetJavaScriptDialogManager();
  if (manager && manager->HandleJavaScriptDialog(
          web_contents, accept, prompt_text ? &prompt_override : nullptr)) {
    return Response::OK();
  }

  return Response::InternalError("Could not handle JavaScript dialog");
}

scoped_refptr<DevToolsProtocol::Response> PageHandler::QueryUsageAndQuota(
    const std::string& security_origin,
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  scoped_refptr<storage::QuotaManager> quota_manager =
      host_->GetProcess()->GetStoragePartition()->GetQuotaManager();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&QueryUsageAndQuotaOnIOThread,
                 quota_manager,
                 GURL(security_origin),
                 base::Bind(&PageHandler::QueryUsageAndQuotaCompleted,
                            weak_factory_.GetWeakPtr(),
                            command)));

  return command->AsyncResponsePromise();
}

Response PageHandler::SetColorPickerEnabled(bool enabled) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  color_picker_->SetEnabled(enabled);
  return Response::OK();
}

void PageHandler::UpdateTouchEventEmulationState() {
  if (!host_)
    return;
  bool enabled = touch_emulation_enabled_ || screencast_enabled_;
  // TODO(dgozman): pass |touch_emulation_configuration_| once supported.
  host_->SetTouchEventEmulationEnabled(enabled);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(host_));
  if (web_contents)
    web_contents->SetForceDisableOverscrollContent(enabled);
}

void PageHandler::NotifyScreencastVisibility(bool visible) {
  if (visible)
    capture_retry_count_ = kCaptureRetryLimit;
  ScreencastVisibilityChangedParams params;
  params.set_visible(visible);
  client_->ScreencastVisibilityChanged(params);
}

void PageHandler::InnerSwapCompositorFrame() {
  if ((base::TimeTicks::Now() - last_frame_time_).InMilliseconds() <
          kFrameRateThresholdMs) {
    return;
  }

  if (!host_ || !host_->GetView())
    return;

  last_frame_time_ = base::TimeTicks::Now();

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  // TODO(vkuzkokov): do not use previous frame metadata.
  cc::CompositorFrameMetadata& metadata = last_compositor_frame_metadata_;

  gfx::SizeF viewport_size_dip = gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor);
  gfx::SizeF screen_size_dip = gfx::ScaleSize(view->GetPhysicalBackingSize(),
                                              1 / metadata.device_scale_factor);

  blink::WebScreenInfo screen_info;
  view->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.deviceScaleFactor;
  double scale = 1;

  if (screencast_max_width_ > 0) {
    double max_width_dip = screencast_max_width_ / device_scale_factor;
    scale = std::min(scale, max_width_dip / screen_size_dip.width());
  }
  if (screencast_max_height_ > 0) {
    double max_height_dip = screencast_max_height_ / device_scale_factor;
    scale = std::min(scale, max_height_dip / screen_size_dip.height());
  }

  if (scale <= 0)
    scale = 0.1;

  gfx::Size snapshot_size_dip(gfx::ToRoundedSize(
      gfx::ScaleSize(viewport_size_dip, scale)));

  if (snapshot_size_dip.width() > 0 && snapshot_size_dip.height() > 0) {
    gfx::Rect viewport_bounds_dip(gfx::ToRoundedSize(viewport_size_dip));
    view->CopyFromCompositingSurface(
        viewport_bounds_dip,
        snapshot_size_dip,
        base::Bind(&PageHandler::ScreencastFrameCaptured,
                   weak_factory_.GetWeakPtr(),
                   screencast_format_,
                   screencast_quality_,
                   last_compositor_frame_metadata_),
        kN32_SkColorType);
  }
}

void PageHandler::ScreencastFrameCaptured(
    const std::string& format,
    int quality,
    const cc::CompositorFrameMetadata& metadata,
    bool success,
    const SkBitmap& bitmap) {
  if (!success) {
    if (capture_retry_count_) {
      --capture_retry_count_;
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PageHandler::InnerSwapCompositorFrame,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kFrameRateThresholdMs));
    }
    return;
  }

  std::vector<unsigned char> data;
  SkAutoLockPixels lock_image(bitmap);
  bool encoded;
  if (format == kPng) {
    encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(bitmap.width(), bitmap.height()),
        bitmap.width() * bitmap.bytesPerPixel(),
        false, std::vector<gfx::PNGCodec::Comment>(), &data);
  } else if (format == kJpeg) {
    encoded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bitmap.width(),
        bitmap.height(),
        bitmap.width() * bitmap.bytesPerPixel(),
        quality, &data);
  } else {
    encoded = false;
  }

  if (!encoded)
    return;

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
      &base_64_data);

  ScreencastFrameMetadata param_metadata;
  // Consider metadata empty in case it has no device scale factor.
  if (metadata.device_scale_factor != 0 && host_) {
    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        host_->GetView());
    if (!view)
      return;

    gfx::SizeF viewport_size_dip = gfx::ScaleSize(
        metadata.scrollable_viewport_size, metadata.page_scale_factor);
    gfx::SizeF screen_size_dip = gfx::ScaleSize(
        view->GetPhysicalBackingSize(), 1 / metadata.device_scale_factor);

    param_metadata.set_device_scale_factor(metadata.device_scale_factor);
    param_metadata.set_page_scale_factor(metadata.page_scale_factor);
    param_metadata.set_page_scale_factor_min(metadata.min_page_scale_factor);
    param_metadata.set_page_scale_factor_max(metadata.max_page_scale_factor);
    param_metadata.set_offset_top(
        metadata.location_bar_content_translation.y());
    param_metadata.set_offset_bottom(screen_size_dip.height() -
        metadata.location_bar_content_translation.y() -
        viewport_size_dip.height());
    param_metadata.set_device_width(screen_size_dip.width());
    param_metadata.set_device_height(screen_size_dip.height());
    param_metadata.set_scroll_offset_x(metadata.root_scroll_offset.x());
    param_metadata.set_scroll_offset_y(metadata.root_scroll_offset.y());

    devtools::dom::Rect viewport;
    viewport.set_x(metadata.root_scroll_offset.x());
    viewport.set_y(metadata.root_scroll_offset.y());
    viewport.set_width(metadata.scrollable_viewport_size.width());
    viewport.set_height(metadata.scrollable_viewport_size.height());
    param_metadata.set_viewport(viewport);
  }

  ScreencastFrameParams params;
  params.set_data(base_64_data);
  params.set_metadata(param_metadata);
  client_->ScreencastFrame(params);
}

void PageHandler::ScreenshotCaptured(
    scoped_refptr<DevToolsProtocol::Command> command,
    const unsigned char* png_data,
    size_t png_size) {
  if (!png_data || !png_size) {
    client_->SendInternalErrorResponse(command,
                                       "Unable to capture screenshot");
    return;
  }

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(png_data), png_size),
      &base_64_data);

  CaptureScreenshotResponse response;
  response.set_data(base_64_data);
  client_->SendCaptureScreenshotResponse(command, response);
}

void PageHandler::OnColorPicked(int r, int g, int b, int a) {
  dom::RGBA color;
  color.set_r(r);
  color.set_g(g);
  color.set_b(b);
  color.set_a(a);
  ColorPickedParams params;
  params.set_color(color);
  client_->ColorPicked(params);
}

void PageHandler::QueryUsageAndQuotaCompleted(
    scoped_refptr<DevToolsProtocol::Command> command,
    scoped_ptr<QueryUsageAndQuotaResponse> response_data) {
  client_->SendQueryUsageAndQuotaResponse(command, *response_data);
}

}  // namespace page
}  // namespace devtools
}  // namespace content
