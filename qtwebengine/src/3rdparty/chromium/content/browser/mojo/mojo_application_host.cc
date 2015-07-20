// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_application_host.h"

#include "content/common/mojo/mojo_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_sender.h"
#include "mojo/edk/embedder/platform_channel_pair.h"

namespace content {
namespace {

base::PlatformFile PlatformFileFromScopedPlatformHandle(
    mojo::embedder::ScopedPlatformHandle handle) {
#if defined(OS_POSIX)
  return handle.release().fd;
#elif defined(OS_WIN)
  return handle.release().handle;
#endif
}

}  // namespace

MojoApplicationHost::MojoApplicationHost() : did_activate_(false) {
#if defined(OS_ANDROID)
  service_registry_android_.reset(
      new ServiceRegistryAndroid(&service_registry_));
#endif
}

MojoApplicationHost::~MojoApplicationHost() {
}

bool MojoApplicationHost::Init() {
  DCHECK(!client_handle_.is_valid()) << "Already initialized!";

  mojo::embedder::PlatformChannelPair channel_pair;

  mojo::ScopedMessagePipeHandle message_pipe = channel_init_.Init(
      PlatformFileFromScopedPlatformHandle(channel_pair.PassServerHandle()),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  if (!message_pipe.is_valid())
    return false;

  // Forward this to the client once we know its process handle.
  client_handle_ = channel_pair.PassClientHandle();

  service_registry_.BindRemoteServiceProvider(message_pipe.Pass());
  return true;
}

void MojoApplicationHost::Activate(IPC::Sender* sender,
                                   base::ProcessHandle process_handle) {
  DCHECK(!did_activate_);
  DCHECK(client_handle_.is_valid());

  base::PlatformFile client_file =
      PlatformFileFromScopedPlatformHandle(client_handle_.Pass());
  did_activate_ = sender->Send(new MojoMsg_Activate(
      IPC::GetFileHandleForProcess(client_file, process_handle, true)));
}

void MojoApplicationHost::WillDestroySoon() {
  channel_init_.WillDestroySoon();
}

}  // namespace content
