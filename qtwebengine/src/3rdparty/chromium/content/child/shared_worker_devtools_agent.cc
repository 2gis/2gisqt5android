// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_worker_devtools_agent.h"

#include "content/child/child_thread.h"
#include "content/common/devtools_messages.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"

using blink::WebSharedWorker;
using blink::WebString;

namespace content {

static const size_t kMaxMessageChunkSize =
    IPC::Channel::kMaximumMessageSize / 4;

SharedWorkerDevToolsAgent::SharedWorkerDevToolsAgent(
    int route_id,
    WebSharedWorker* webworker)
    : route_id_(route_id),
      webworker_(webworker) {
}

SharedWorkerDevToolsAgent::~SharedWorkerDevToolsAgent() {
}

// Called on the Worker thread.
bool SharedWorkerDevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedWorkerDevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SharedWorkerDevToolsAgent::SendDevToolsMessage(
    const blink::WebString& message) {
  std::string msg(message.utf8());
  if (message.length() < kMaxMessageChunkSize) {
    Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_, msg, msg.size()));
    return;
  }

  for (size_t pos = 0; pos < msg.length(); pos += kMaxMessageChunkSize) {
    Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_,
        msg.substr(pos, kMaxMessageChunkSize),
        pos ? 0 : msg.size()));
  }
}

void SharedWorkerDevToolsAgent::SaveDevToolsAgentState(
    const blink::WebString& state) {
  Send(new DevToolsHostMsg_SaveAgentRuntimeState(route_id_,
                                                 state.utf8()));
}

void SharedWorkerDevToolsAgent::OnAttach(const std::string& host_id) {
  webworker_->attachDevTools(WebString::fromUTF8(host_id));
}

void SharedWorkerDevToolsAgent::OnReattach(const std::string& host_id,
                                           const std::string& state) {
  webworker_->reattachDevTools(WebString::fromUTF8(host_id),
                               WebString::fromUTF8(state));
}

void SharedWorkerDevToolsAgent::OnDetach() {
  webworker_->detachDevTools();
}

void SharedWorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    const std::string& message) {
  webworker_->dispatchDevToolsMessage(WebString::fromUTF8(message));
}

bool SharedWorkerDevToolsAgent::Send(IPC::Message* message) {
  return ChildThread::current()->Send(message);
}

}  // namespace content
