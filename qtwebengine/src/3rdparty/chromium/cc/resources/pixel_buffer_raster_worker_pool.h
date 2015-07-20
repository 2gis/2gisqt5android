// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <deque>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/output/context_provider.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class ResourceProvider;

class CC_EXPORT PixelBufferRasterWorkerPool : public RasterWorkerPool,
                                              public Rasterizer,
                                              public RasterizerTaskClient {
 public:
  ~PixelBufferRasterWorkerPool() override;

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider,
      size_t max_transfer_buffer_usage_bytes);

  // Overridden from RasterWorkerPool:
  Rasterizer* AsRasterizer() override;

  // Overridden from Rasterizer:
  void SetClient(RasterizerClient* client) override;
  void Shutdown() override;
  void ScheduleTasks(RasterTaskQueue* queue) override;
  void CheckForCompletedTasks() override;

  // Overridden from RasterizerTaskClient:
  scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource) override;
  void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) override;

 private:
  struct RasterTaskState {
    class TaskComparator {
     public:
      explicit TaskComparator(const RasterTask* task) : task_(task) {}

      bool operator()(const RasterTaskState& state) const {
        return state.task == task_;
      }

     private:
      const RasterTask* task_;
    };

    typedef std::vector<RasterTaskState> Vector;

    RasterTaskState(RasterTask* task, const TaskSetCollection& task_sets);

    enum { UNSCHEDULED, SCHEDULED, UPLOADING, COMPLETED } type;
    RasterTask* task;
    TaskSetCollection task_sets;
  };

  typedef std::deque<scoped_refptr<RasterTask>> RasterTaskDeque;

  PixelBufferRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                              TaskGraphRunner* task_graph_runner,
                              ContextProvider* context_provider,
                              ResourceProvider* resource_provider,
                              size_t max_transfer_buffer_usage_bytes);

  void OnRasterFinished(TaskSet task_set);
  void FlushUploads();
  void CheckForCompletedUploads();
  void CheckForCompletedRasterTasks();
  void ScheduleMoreTasks();
  unsigned PendingRasterTaskCount() const;
  TaskSetCollection PendingTasks() const;
  void CheckForCompletedRasterizerTasks();

  const char* StateName() const;
  scoped_refptr<base::debug::ConvertableToTraceFormat> StateAsValue() const;
  void ThrottleStateAsValueInto(base::debug::TracedValue* throttle_state) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;

  bool shutdown_;

  RasterTaskQueue raster_tasks_;
  RasterTaskState::Vector raster_task_states_;
  RasterTaskDeque raster_tasks_with_pending_upload_;
  RasterTask::Vector completed_raster_tasks_;
  RasterizerTask::Vector completed_image_decode_tasks_;

  size_t scheduled_raster_task_count_;
  size_t task_counts_[kNumberOfTaskSets];
  size_t bytes_pending_upload_;
  size_t max_bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;

  TaskSetCollection should_notify_client_if_no_tasks_are_pending_;
  TaskSetCollection raster_finished_tasks_pending_;

  DelayedUniqueNotifier check_for_completed_raster_task_notifier_;

  scoped_refptr<RasterizerTask> raster_finished_tasks_[kNumberOfTaskSets];

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  TaskGraph graph_;
  Task::Vector completed_tasks_;

  base::WeakPtrFactory<PixelBufferRasterWorkerPool>
      raster_finished_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
