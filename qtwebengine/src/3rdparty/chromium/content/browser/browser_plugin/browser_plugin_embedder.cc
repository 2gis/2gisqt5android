// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/values.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents),
      guest_drag_ending_(false),
      weak_ptr_factory_(this) {
}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
}

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents) {
  return new BrowserPluginEmbedder(web_contents);
}

void BrowserPluginEmbedder::DragEnteredGuest(BrowserPluginGuest* guest) {
  guest_dragging_over_ = guest->AsWeakPtr();
}

void BrowserPluginEmbedder::DragLeftGuest(BrowserPluginGuest* guest) {
  // Avoid race conditions in switching between guests being hovered over by
  // only un-setting if the caller is marked as the guest being dragged over.
  if (guest_dragging_over_.get() == guest) {
    guest_dragging_over_.reset();
  }
}

void BrowserPluginEmbedder::StartDrag(BrowserPluginGuest* guest) {
  guest_started_drag_ = guest->AsWeakPtr();
  guest_drag_ending_ = false;
}

WebContentsImpl* BrowserPluginEmbedder::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

BrowserPluginGuestManager*
BrowserPluginEmbedder::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

void BrowserPluginEmbedder::ClearGuestDragStateIfApplicable() {
  // The order at which we observe SystemDragEnded() and DragSourceEndedAt() is
  // platform dependent.
  // In OSX, we see SystemDragEnded() first, where in aura, we see
  // DragSourceEndedAt() first. For this reason, we check if both methods were
  // called before resetting |guest_started_drag_|.
  if (guest_drag_ending_) {
    if (guest_started_drag_)
      guest_started_drag_.reset();
  } else {
    guest_drag_ending_ = true;
  }
}

bool BrowserPluginEmbedder::DidSendScreenRectsCallback(
   WebContents* guest_web_contents) {
  static_cast<RenderViewHostImpl*>(
      guest_web_contents->GetRenderViewHost())->SendScreenRects();
  // Not handled => Iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::DidSendScreenRects() {
  GetBrowserPluginGuestManager()->ForEachGuest(
          GetWebContents(), base::Bind(
              &BrowserPluginEmbedder::DidSendScreenRectsCallback,
              base::Unretained(this)));
}

bool BrowserPluginEmbedder::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedder, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER_GENERIC(DragHostMsg_UpdateDragCursor,
                                OnUpdateDragCursor(&handled));
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, blink::WebDragOperation operation) {
  if (guest_started_drag_) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceEndedAt(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y, operation);
  }
  ClearGuestDragStateIfApplicable();
}

void BrowserPluginEmbedder::SystemDragEnded() {
  // When the embedder's drag/drop operation ends, we need to pass the message
  // to the guest that initiated the drag/drop operation. This will ensure that
  // the guest's RVH state is reset properly.
  if (guest_started_drag_)
    guest_started_drag_->EndSystemDrag();
  guest_dragging_over_.reset();
  ClearGuestDragStateIfApplicable();
}

void BrowserPluginEmbedder::OnUpdateDragCursor(bool* handled) {
  *handled = (guest_dragging_over_.get() != NULL);
}

void BrowserPluginEmbedder::OnAttach(
    int browser_plugin_instance_id,
    const BrowserPluginHostMsg_Attach_Params& params) {
  WebContents* guest_web_contents =
      GetBrowserPluginGuestManager()->GetGuestByInstanceID(
          GetWebContents(), browser_plugin_instance_id);
  if (!guest_web_contents)
    return;
  BrowserPluginGuest* guest = static_cast<WebContentsImpl*>(guest_web_contents)
                                  ->GetBrowserPluginGuest();
  guest->Attach(browser_plugin_instance_id, GetWebContents(), params);
}

bool BrowserPluginEmbedder::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ((event.windowsKeyCode != ui::VKEY_ESCAPE) ||
      (event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return false;
  }

  bool event_consumed = false;
  GetBrowserPluginGuestManager()->ForEachGuest(
      GetWebContents(),
      base::Bind(&BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback,
                 base::Unretained(this),
                 &event_consumed));

  return event_consumed;
}

bool BrowserPluginEmbedder::Find(int request_id,
                                 const base::string16& search_text,
                                 const blink::WebFindOptions& options) {
  return GetBrowserPluginGuestManager()->ForEachGuest(
      GetWebContents(),
      base::Bind(&BrowserPluginEmbedder::FindInGuest,
                 base::Unretained(this),
                 request_id,
                 search_text,
                 options));
}

bool BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback(bool* mouse_unlocked,
                                                           WebContents* guest) {
  *mouse_unlocked |= static_cast<WebContentsImpl*>(guest)
                         ->GetBrowserPluginGuest()
                         ->mouse_locked();
  guest->GotResponseToLockMouseRequest(false);

  // Returns false to iterate over all guests.
  return false;
}

bool BrowserPluginEmbedder::FindInGuest(int request_id,
                                        const base::string16& search_text,
                                        const blink::WebFindOptions& options,
                                        WebContents* guest) {
  if (static_cast<WebContentsImpl*>(guest)->GetBrowserPluginGuest()->Find(
          request_id, search_text, options)) {
    // There can only ever currently be one browser plugin that handles find so
    // we can break the iteration at this point.
    return true;
  }
  return false;
}

}  // namespace content
