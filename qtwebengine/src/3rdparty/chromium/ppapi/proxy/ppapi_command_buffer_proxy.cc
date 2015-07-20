// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

PpapiCommandBufferProxy::PpapiCommandBufferProxy(
    const ppapi::HostResource& resource,
    ProxyChannel* channel,
    const gpu::Capabilities& capabilities,
    const SerializedHandle& shared_state)
    : capabilities_(capabilities),
      resource_(resource),
      channel_(channel) {
  shared_state_shm_.reset(
      new base::SharedMemory(shared_state.shmem(), false));
  shared_state_shm_->Map(shared_state.size());
}

PpapiCommandBufferProxy::~PpapiCommandBufferProxy() {
  // gpu::Buffers are no longer referenced, allowing shared memory objects to be
  // deleted, closing the handle in this process.
}

bool PpapiCommandBufferProxy::Initialize() {
  return true;
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetLastState() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  return last_state_;
}

int32 PpapiCommandBufferProxy::GetLastToken() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  TryUpdateState();
  return last_state_.token;
}

void PpapiCommandBufferProxy::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);
}

void PpapiCommandBufferProxy::WaitForTokenInRange(int32 start, int32 end) {
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange(
             ppapi::API_ID_PPB_GRAPHICS_3D,
             resource_,
             start,
             end,
             &state,
             &success)))
      UpdateState(state, success);
  }
  DCHECK(InRange(start, end, last_state_.token) ||
         last_state_.error != gpu::error::kNoError);
}

void PpapiCommandBufferProxy::WaitForGetOffsetInRange(int32 start, int32 end) {
  TryUpdateState();
  if (!InRange(start, end, last_state_.get_offset) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange(
             ppapi::API_ID_PPB_GRAPHICS_3D,
             resource_,
             start,
             end,
             &state,
             &success)))
      UpdateState(state, success);
  }
  DCHECK(InRange(start, end, last_state_.get_offset) ||
         last_state_.error != gpu::error::kNoError);
}

void PpapiCommandBufferProxy::SetGetBuffer(int32 transfer_buffer_id) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_SetGetBuffer(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, transfer_buffer_id));
  }
}

scoped_refptr<gpu::Buffer> PpapiCommandBufferProxy::CreateTransferBuffer(
    size_t size,
    int32* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  ppapi::proxy::SerializedHandle handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY);
  if (!Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_, size, id, &handle))) {
    return NULL;
  }

  if (*id <= 0 || !handle.is_shmem())
    return NULL;

  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.shmem(), false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(handle.size())) {
      *id = -1;
      return NULL;
    }
  }

  return gpu::MakeBufferFromSharedMemory(shared_memory.Pass(), handle.size());
}

void PpapiCommandBufferProxy::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id));
}

uint32 PpapiCommandBufferProxy::CreateStreamTexture(uint32 texture_id) {
  NOTREACHED();
  return 0;
}

uint32 PpapiCommandBufferProxy::InsertSyncPoint() {
  uint32 sync_point = 0;
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_InsertSyncPoint(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, &sync_point));
  }
  return sync_point;
}

uint32 PpapiCommandBufferProxy::InsertFutureSyncPoint() {
  uint32 sync_point = 0;
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_InsertFutureSyncPoint(
        ppapi::API_ID_PPB_GRAPHICS_3D, resource_, &sync_point));
  }
  return sync_point;
}

void PpapiCommandBufferProxy::RetireSyncPoint(uint32 sync_point) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_RetireSyncPoint(
        ppapi::API_ID_PPB_GRAPHICS_3D, resource_, sync_point));
  }
}

void PpapiCommandBufferProxy::SignalSyncPoint(uint32 sync_point,
                                              const base::Closure& callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SignalQuery(uint32 query,
                                          const base::Closure& callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetSurfaceVisible(bool visible) {
  NOTREACHED();
}

gpu::Capabilities PpapiCommandBufferProxy::GetCapabilities() {
  return capabilities_;
}

int32 PpapiCommandBufferProxy::CreateImage(ClientBuffer buffer,
                                           size_t width,
                                           size_t height,
                                           unsigned internalformat) {
  NOTREACHED();
  return -1;
}

void PpapiCommandBufferProxy::DestroyImage(int32 id) {
  NOTREACHED();
}

int32 PpapiCommandBufferProxy::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  NOTREACHED();
  return -1;
}

bool PpapiCommandBufferProxy::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (channel_->Send(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void PpapiCommandBufferProxy::UpdateState(
    const gpu::CommandBuffer::State& state,
    bool success) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (success) {
    if (state.generation - last_state_.generation < 0x80000000U) {
      last_state_ = state;
    }
  } else {
    last_state_.error = gpu::error::kLostContext;
    ++last_state_.generation;
  }
}

void PpapiCommandBufferProxy::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* PpapiCommandBufferProxy::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_shm_->memory());
}

}  // namespace proxy
}  // namespace ppapi
