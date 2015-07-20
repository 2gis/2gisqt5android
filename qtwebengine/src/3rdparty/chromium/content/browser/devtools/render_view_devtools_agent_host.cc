// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_view_devtools_agent_host.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"
#include "content/browser/devtools/protocol/dom_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/power_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "content/browser/power_save_blocker_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#endif

namespace content {

typedef std::vector<RenderViewDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

//Returns RenderViewDevToolsAgentHost attached to any of RenderViewHost
//instances associated with |web_contents|
static RenderViewDevToolsAgentHost* FindAgentHost(WebContents* web_contents) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->GetWebContents() == web_contents)
      return *it;
  }
  return NULL;
}

}  // namespace

scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  RenderViewDevToolsAgentHost* result = FindAgentHost(web_contents);
  if (!result)
    result = new RenderViewDevToolsAgentHost(web_contents->GetRenderViewHost());
  return result;
}

// static
bool DevToolsAgentHost::HasFor(WebContents* web_contents) {
  return FindAgentHost(web_contents) != NULL;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  RenderViewDevToolsAgentHost* agent_host = FindAgentHost(web_contents);
  return agent_host && agent_host->IsAttached();
}

//static
std::vector<WebContents*> DevToolsAgentHostImpl::GetInspectableWebContents() {
  std::set<WebContents*> set;
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed contents.
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
      continue;

    RenderViewHost* rvh = RenderViewHost::From(widget);
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (web_contents)
      set.insert(web_contents);
  }
  std::vector<WebContents*> result(set.size());
  std::copy(set.begin(), set.end(), result.begin());
  return result;
}

// static
void RenderViewDevToolsAgentHost::OnCancelPendingNavigation(
    RenderViewHost* pending,
    RenderViewHost* current) {
  WebContents* web_contents = WebContents::FromRenderViewHost(pending);
  RenderViewDevToolsAgentHost* agent_host = FindAgentHost(web_contents);
  if (!agent_host)
    return;
  agent_host->DisconnectRenderViewHost();
  agent_host->ConnectRenderViewHost(current);
}

RenderViewDevToolsAgentHost::RenderViewDevToolsAgentHost(RenderViewHost* rvh)
    : render_view_host_(NULL),
      dom_handler_(new devtools::dom::DOMHandler()),
      input_handler_(new devtools::input::InputHandler()),
      network_handler_(new devtools::network::NetworkHandler()),
      page_handler_(new devtools::page::PageHandler()),
      power_handler_(new devtools::power::PowerHandler()),
      tracing_handler_(new devtools::tracing::TracingHandler(
          devtools::tracing::TracingHandler::Renderer)),
      handler_impl_(new DevToolsProtocolHandlerImpl()),
      reattaching_(false) {
  handler_impl_->SetDOMHandler(dom_handler_.get());
  handler_impl_->SetInputHandler(input_handler_.get());
  handler_impl_->SetNetworkHandler(network_handler_.get());
  handler_impl_->SetPageHandler(page_handler_.get());
  handler_impl_->SetPowerHandler(power_handler_.get());
  handler_impl_->SetTracingHandler(tracing_handler_.get());
  SetRenderViewHost(rvh);
  DevToolsProtocol::Notifier notifier(base::Bind(
      &RenderViewDevToolsAgentHost::DispatchOnInspectorFrontend,
      base::Unretained(this)));
  handler_impl_->SetNotifier(notifier);
  g_instances.Get().push_back(this);
  AddRef();  // Balanced in RenderViewHostDestroyed.
  DevToolsManager::GetInstance()->AgentHostChanged(this);
}

WebContents* RenderViewDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

void RenderViewDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  std::string error_message;

  scoped_ptr<base::DictionaryValue> message_dict(
      DevToolsProtocol::ParseMessage(message, &error_message));
  scoped_refptr<DevToolsProtocol::Command> command =
      DevToolsProtocol::ParseCommand(message_dict.get(), &error_message);

  if (command.get()) {
    scoped_refptr<DevToolsProtocol::Response> overridden_response;

    DevToolsManagerDelegate* delegate =
        DevToolsManager::GetInstance()->delegate();
    if (delegate) {
      scoped_ptr<base::DictionaryValue> overridden_response_value(
          delegate->HandleCommand(this, message_dict.get()));
      if (overridden_response_value)
        overridden_response = DevToolsProtocol::ParseResponse(
            overridden_response_value.get());
    }
    if (!overridden_response.get())
      overridden_response = handler_impl_->HandleCommand(command);
    if (overridden_response.get()) {
      if (!overridden_response->is_async_promise())
        DispatchOnInspectorFrontend(overridden_response->Serialize());
      return;
    }
  }

  IPCDevToolsAgentHost::DispatchProtocolMessage(message);
}

void RenderViewDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  if (!render_view_host_)
    return;
  msg->set_routing_id(render_view_host_->GetRoutingID());
  render_view_host_->Send(msg);
}

void RenderViewDevToolsAgentHost::OnClientAttached() {
  if (!render_view_host_)
    return;

  InnerOnClientAttached();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsAgentHostImpl::NotifyCallbacks(this, true);
}

void RenderViewDevToolsAgentHost::InnerOnClientAttached() {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      render_view_host_->GetProcess()->GetID());

#if defined(OS_ANDROID)
  power_save_blocker_.reset(
      static_cast<PowerSaveBlockerImpl*>(
          PowerSaveBlocker::Create(
              PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
              "DevTools").release()));
  if (render_view_host_->GetView()) {
    power_save_blocker_.get()->
        InitDisplaySleepBlocker(render_view_host_->GetView()->GetNativeView());
  }
#endif
}

void RenderViewDevToolsAgentHost::OnClientDetached() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset();
#endif
  page_handler_->Detached();
  power_handler_->Detached();
  tracing_handler_->Detached();
  ClientDetachedFromRenderer();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsAgentHostImpl::NotifyCallbacks(this, false);
}

void RenderViewDevToolsAgentHost::ClientDetachedFromRenderer() {
  if (!render_view_host_)
    return;

  InnerClientDetachedFromRenderer();
}

void RenderViewDevToolsAgentHost::InnerClientDetachedFromRenderer() {
  bool process_has_agents = false;
  RenderProcessHost* render_process_host = render_view_host_->GetProcess();
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (*it == this || !(*it)->IsAttached())
      continue;
    RenderViewHost* rvh = (*it)->render_view_host_;
    if (rvh && rvh->GetProcess() == render_process_host)
      process_has_agents = true;
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        render_process_host->GetID());
  }
}

RenderViewDevToolsAgentHost::~RenderViewDevToolsAgentHost() {
  Instances::iterator it = std::find(g_instances.Get().begin(),
                                     g_instances.Get().end(),
                                     this);
  if (it != g_instances.Get().end())
    g_instances.Get().erase(it);
}

void RenderViewDevToolsAgentHost::AboutToNavigateRenderView(
    RenderViewHost* dest_rvh) {
  if (!render_view_host_)
    return;

  if (render_view_host_ == dest_rvh &&
          render_view_host_->render_view_termination_status() ==
              base::TERMINATION_STATUS_STILL_RUNNING)
    return;
  ReattachToRenderViewHost(dest_rvh);
}

void RenderViewDevToolsAgentHost::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  if (new_host != render_view_host_) {
    // AboutToNavigateRenderView was not called for renderer-initiated
    // navigation.
    ReattachToRenderViewHost(new_host);
  }
}

void
RenderViewDevToolsAgentHost::ReattachToRenderViewHost(RenderViewHost* rvh) {
  DCHECK(!reattaching_);
  reattaching_ = true;
  DisconnectRenderViewHost();
  ConnectRenderViewHost(rvh);
  reattaching_ = false;
}

void RenderViewDevToolsAgentHost::RenderViewDeleted(RenderViewHost* rvh) {
  if (rvh != render_view_host_)
    return;

  DCHECK(render_view_host_);
  scoped_refptr<RenderViewDevToolsAgentHost> protect(this);
  HostClosed();
  ClearRenderViewHost();
  DevToolsManager::GetInstance()->AgentHostChanged(this);
  Release();
}

void RenderViewDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      RenderViewCrashed();
      break;
    default:
      break;
  }
}

bool RenderViewDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  return DispatchIPCMessage(message);
}

bool RenderViewDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message) {
  return DispatchIPCMessage(message);
}

void RenderViewDevToolsAgentHost::DidAttachInterstitialPage() {
  page_handler_->DidAttachInterstitialPage();

  if (!render_view_host_)
    return;
  // The rvh set in AboutToNavigateRenderView turned out to be interstitial.
  // Connect back to the real one.
  WebContents* web_contents =
    WebContents::FromRenderViewHost(render_view_host_);
  if (!web_contents)
    return;
  DisconnectRenderViewHost();
  ConnectRenderViewHost(web_contents->GetRenderViewHost());
}

void RenderViewDevToolsAgentHost::DidDetachInterstitialPage() {
  page_handler_->DidDetachInterstitialPage();
}

void RenderViewDevToolsAgentHost::TitleWasSet(
    NavigationEntry* entry, bool explicit_set) {
  DevToolsManager::GetInstance()->AgentHostChanged(this);
}

void RenderViewDevToolsAgentHost::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  DevToolsManager::GetInstance()->AgentHostChanged(this);
}

void RenderViewDevToolsAgentHost::Observe(int type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
    bool visible = *Details<bool>(details).ptr();
    page_handler_->OnVisibilityChanged(visible);
  }
}

void RenderViewDevToolsAgentHost::SetRenderViewHost(RenderViewHost* rvh) {
  DCHECK(!render_view_host_);
  render_view_host_ = static_cast<RenderViewHostImpl*>(rvh);

  WebContentsObserver::Observe(WebContents::FromRenderViewHost(rvh));
  dom_handler_->SetRenderViewHost(render_view_host_);
  input_handler_->SetRenderViewHost(render_view_host_);
  network_handler_->SetRenderViewHost(render_view_host_);
  page_handler_->SetRenderViewHost(render_view_host_);

  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(render_view_host_));
}

void RenderViewDevToolsAgentHost::ClearRenderViewHost() {
  DCHECK(render_view_host_);
  registrar_.Remove(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(render_view_host_));
  render_view_host_ = nullptr;
  dom_handler_->SetRenderViewHost(nullptr);
  input_handler_->SetRenderViewHost(nullptr);
  network_handler_->SetRenderViewHost(nullptr);
  page_handler_->SetRenderViewHost(nullptr);
}

void RenderViewDevToolsAgentHost::DisconnectWebContents() {
  DisconnectRenderViewHost();
}

void RenderViewDevToolsAgentHost::ConnectWebContents(WebContents* wc) {
  ConnectRenderViewHost(wc->GetRenderViewHost());
}

DevToolsAgentHost::Type RenderViewDevToolsAgentHost::GetType() {
  return TYPE_WEB_CONTENTS;
}

std::string RenderViewDevToolsAgentHost::GetTitle() {
  if (WebContents* web_contents = GetWebContents())
    return base::UTF16ToUTF8(web_contents->GetTitle());
  return "";
}

GURL RenderViewDevToolsAgentHost::GetURL() {
  if (WebContents* web_contents = GetWebContents())
    return web_contents->GetVisibleURL();
  return render_view_host_ ?
      render_view_host_->GetMainFrame()->GetLastCommittedURL() : GURL();
}

bool RenderViewDevToolsAgentHost::Activate() {
  if (render_view_host_) {
    render_view_host_->GetDelegate()->Activate();
    return true;
  }
  return false;
}

bool RenderViewDevToolsAgentHost::Close() {
  if (render_view_host_) {
    render_view_host_->ClosePage();
    return true;
  }
  return false;
}

void RenderViewDevToolsAgentHost::ConnectRenderViewHost(RenderViewHost* rvh) {
  SetRenderViewHost(rvh);
  if (IsAttached())
    Reattach(state_);
}

void RenderViewDevToolsAgentHost::DisconnectRenderViewHost() {
  ClientDetachedFromRenderer();
  ClearRenderViewHost();
}

void RenderViewDevToolsAgentHost::RenderViewCrashed() {
  scoped_refptr<DevToolsProtocol::Notification> notification =
      DevToolsProtocol::CreateNotification(
          devtools::Inspector::targetCrashed::kName, NULL);
  SendMessageToClient(notification->Serialize());
}

bool RenderViewDevToolsAgentHost::DispatchIPCMessage(
    const IPC::Message& msg) {
  if (!render_view_host_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRuntimeState)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
                                handled = false; OnSwapCompositorFrame(msg))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderViewDevToolsAgentHost::OnSwapCompositorFrame(
    const IPC::Message& message) {
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return;
  page_handler_->OnSwapCompositorFrame(param.b.metadata);
}

void RenderViewDevToolsAgentHost::SynchronousSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!render_view_host_)
    return;
  page_handler_->OnSwapCompositorFrame(frame_metadata);
}

void RenderViewDevToolsAgentHost::OnSaveAgentRuntimeState(
    const std::string& state) {
  if (!render_view_host_)
    return;
  state_ = state;
}

void RenderViewDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const std::string& message,
    uint32 total_size) {
  if (!IsAttached() || !render_view_host_)
    return;
  ProcessChunkedMessageFromAgent(message, total_size);
}

void RenderViewDevToolsAgentHost::DispatchOnInspectorFrontend(
    const std::string& message) {
  if (!IsAttached() || !render_view_host_)
    return;
  SendMessageToClient(message);
}

}  // namespace content
