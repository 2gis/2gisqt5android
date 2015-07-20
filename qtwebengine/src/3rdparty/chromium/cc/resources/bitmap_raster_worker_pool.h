// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_BITMAP_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_BITMAP_RASTER_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
}
}

namespace cc {
class ResourceProvider;

class CC_EXPORT BitmapRasterWorkerPool : public RasterWorkerPool,
                                         public Rasterizer,
                                         public RasterizerTaskClient {
 public:
  ~BitmapRasterWorkerPool() override;

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ResourceProvider* resource_provider);

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

 protected:
  BitmapRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                         TaskGraphRunner* task_graph_runner,
                         ResourceProvider* resource_provider);

 private:
  void OnRasterFinished(TaskSet task_set);
  scoped_refptr<base::debug::ConvertableToTraceFormat> StateAsValue() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ResourceProvider* resource_provider_;

  TaskSetCollection raster_pending_;

  scoped_refptr<RasterizerTask> raster_finished_tasks_[kNumberOfTaskSets];

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  TaskGraph graph_;
  Task::Vector completed_tasks_;

  base::WeakPtrFactory<BitmapRasterWorkerPool>
      raster_finished_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BitmapRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_BITMAP_RASTER_WORKER_POOL_H_
