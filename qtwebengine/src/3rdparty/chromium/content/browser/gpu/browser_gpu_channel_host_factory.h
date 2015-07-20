// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "ipc/message_filter.h"

namespace content {
class BrowserGpuMemoryBufferManager;
class GpuMemoryBufferFactoryHostImpl;

class CONTENT_EXPORT BrowserGpuChannelHostFactory
    : public GpuChannelHostFactory {
 public:
  static void Initialize(bool establish_gpu_channel);
  static void Terminate();
  static BrowserGpuChannelHostFactory* instance() { return instance_; }

  // GpuChannelHostFactory implementation.
  bool IsMainThread() override;
  base::MessageLoop* GetMainLoop() override;
  scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() override;
  scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) override;
  CreateCommandBufferResult CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) override;

  // Specify a task runner and callback to be used for a set of messages. The
  // callback will be set up on the current GpuProcessHost, identified by
  // GpuProcessHostId().
  virtual void SetHandlerForControlMessages(
      const uint32* message_ids,
      size_t num_messages,
      const base::Callback<void(const IPC::Message&)>& handler,
      base::TaskRunner* target_task_runner);
  int GpuProcessHostId() { return gpu_host_id_; }
  GpuChannelHost* EstablishGpuChannelSync(
      CauseForGpuLaunch cause_for_gpu_launch);
  void EstablishGpuChannel(CauseForGpuLaunch cause_for_gpu_launch,
                           const base::Closure& callback);
  GpuChannelHost* GetGpuChannel();
  int GetGpuChannelId() { return gpu_client_id_; }

  // Used to skip GpuChannelHost tests when there can be no GPU process.
  static bool CanUseForTesting();

 private:
  struct CreateRequest;
  class EstablishRequest;

  BrowserGpuChannelHostFactory();
  ~BrowserGpuChannelHostFactory() override;

  void GpuChannelEstablished();
  void CreateViewCommandBufferOnIO(
      CreateRequest* request,
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params);
  static void CommandBufferCreatedOnIO(CreateRequest* request,
                                       CreateCommandBufferResult result);
  static void AddFilterOnIO(int gpu_host_id,
                            scoped_refptr<IPC::MessageFilter> filter);

  const int gpu_client_id_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  scoped_refptr<GpuChannelHost> gpu_channel_;
  scoped_ptr<GpuMemoryBufferFactoryHostImpl> gpu_memory_buffer_factory_host_;
  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  int gpu_host_id_;
  scoped_refptr<EstablishRequest> pending_request_;
  std::vector<base::Closure> established_callbacks_;

  static BrowserGpuChannelHostFactory* instance_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuChannelHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CHANNEL_HOST_FACTORY_H_
