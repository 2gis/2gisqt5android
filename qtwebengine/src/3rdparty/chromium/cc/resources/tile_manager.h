// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <deque>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/base/unique_notifier.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/eviction_tile_priority_queue.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class PictureLayerImpl;
class ResourceProvider;

class CC_EXPORT TileManagerClient {
 public:
  // Returns the set of layers that the tile manager should consider for raster.
  // TODO(vmpstr): Change the way we determine if we are ready to activate, so
  // that this can be removed.
  virtual const std::vector<PictureLayerImpl*>& GetPictureLayers() const = 0;

  // Called when all tiles marked as required for activation are ready to draw.
  virtual void NotifyReadyToActivate() = 0;

  // Called when the visible representation of a tile might have changed. Some
  // examples are:
  // - Tile version initialized.
  // - Tile resources freed.
  // - Tile marked for on-demand raster.
  virtual void NotifyTileStateChanged(const Tile* tile) = 0;

  // Given an empty raster tile priority queue, this will build a priority queue
  // that will return tiles in order in which they should be rasterized.
  // Note if the queue was previous built, Reset must be called on it.
  virtual void BuildRasterQueue(RasterTilePriorityQueue* queue,
                                TreePriority tree_priority) = 0;

  // Given an empty eviction tile priority queue, this will build a priority
  // queue that will return tiles in order in which they should be evicted.
  // Note if the queue was previous built, Reset must be called on it.
  virtual void BuildEvictionQueue(EvictionTilePriorityQueue* queue,
                                  TreePriority tree_priority) = 0;

 protected:
  virtual ~TileManagerClient() {}
};

struct RasterTaskCompletionStats {
  RasterTaskCompletionStats();

  size_t completed_count;
  size_t canceled_count;
};
scoped_refptr<base::debug::ConvertableToTraceFormat>
    RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats);

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public RasterizerClient,
                              public RefCountedManager<Tile> {
 public:
  enum NamedTaskSet {
    REQUIRED_FOR_ACTIVATION = 0,
    ALL = 1,
    // Adding additional values requires increasing kNumberOfTaskSets in
    // rasterizer.h
  };

  static scoped_ptr<TileManager> Create(
      TileManagerClient* client,
      base::SequencedTaskRunner* task_runner,
      ResourcePool* resource_pool,
      Rasterizer* rasterizer,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      size_t scheduled_raster_task_limit);
  ~TileManager() override;

  void ManageTiles(const GlobalStateThatImpactsTilePriority& state);

  // Returns true when visible tiles have been initialized.
  bool UpdateVisibleTiles();

  scoped_refptr<Tile> CreateTile(RasterSource* raster_source,
                                 const gfx::Size& tile_size,
                                 const gfx::Rect& content_rect,
                                 float contents_scale,
                                 int layer_id,
                                 int source_frame_number,
                                 int flags);

  scoped_refptr<base::debug::ConvertableToTraceFormat> BasicStateAsValue()
      const;
  void BasicStateAsValueInto(base::debug::TracedValue* dict) const;
  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  void InitializeTilesWithResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      ManagedTileState& mts = tiles[i]->managed_state();

      mts.draw_info.resource_ =
          resource_pool_->AcquireResource(tiles[i]->size());
    }
  }

  void ReleaseTileResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      Tile* tile = tiles[i];
      FreeResourcesForTile(tile);
    }
  }

  void SetGlobalStateForTesting(
      const GlobalStateThatImpactsTilePriority& state) {
    global_state_ = state;
  }

  void SetRasterizerForTesting(Rasterizer* rasterizer);

  void FreeResourcesAndCleanUpReleasedTilesForTesting() {
    FreeResourcesForReleasedTiles();
    CleanUpReleasedTiles();
  }

  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> tiles;
    for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end();
         ++it) {
      tiles.push_back(it->second);
    }
    return tiles;
  }

 protected:
  TileManager(TileManagerClient* client,
              const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              ResourcePool* resource_pool,
              Rasterizer* rasterizer,
              RenderingStatsInstrumentation* rendering_stats_instrumentation,
              size_t scheduled_raster_task_limit);

  void FreeResourcesForReleasedTiles();
  void CleanUpReleasedTiles();

  // Overriden from RefCountedManager<Tile>:
  friend class Tile;
  void Release(Tile* tile) override;

  // Overriden from RasterizerClient:
  void DidFinishRunningTasks(TaskSet task_set) override;
  TaskSetCollection TasksThatShouldBeForcedToComplete() const override;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;

  // Virtual for test
  virtual void ScheduleTasks(
      const TileVector& tiles_that_need_to_be_rasterized);

  void AssignGpuMemoryToTiles(TileVector* tiles_that_need_to_be_rasterized);

 private:
  class MemoryUsage {
   public:
    MemoryUsage();
    MemoryUsage(int64 memory_bytes, int resource_count);

    static MemoryUsage FromConfig(const gfx::Size& size, ResourceFormat format);
    static MemoryUsage FromTile(const Tile* tile);

    MemoryUsage& operator+=(const MemoryUsage& other);
    MemoryUsage& operator-=(const MemoryUsage& other);
    MemoryUsage operator-(const MemoryUsage& other);

    bool Exceeds(const MemoryUsage& limit) const;
    int64 memory_bytes() const { return memory_bytes_; }

   private:
    int64 memory_bytes_;
    int resource_count_;
  };

  void OnImageDecodeTaskCompleted(int layer_id,
                                  SkPixelRef* pixel_ref,
                                  bool was_canceled);
  void OnRasterTaskCompleted(Tile::Id tile,
                             scoped_ptr<ScopedResource> resource,
                             const RasterSource::SolidColorAnalysis& analysis,
                             bool was_canceled);

  void FreeResourcesForTile(Tile* tile);
  void FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(Tile* tile);
  scoped_refptr<ImageDecodeTask> CreateImageDecodeTask(Tile* tile,
                                                       SkPixelRef* pixel_ref);
  scoped_refptr<RasterTask> CreateRasterTask(Tile* tile);

  void RebuildEvictionQueueIfNeeded();
  bool FreeTileResourcesUntilUsageIsWithinLimit(const MemoryUsage& limit,
                                                MemoryUsage* usage);
  bool FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
      const MemoryUsage& limit,
      const TilePriority& oother_priority,
      MemoryUsage* usage);
  bool TilePriorityViolatesMemoryPolicy(const TilePriority& priority);
  bool IsReadyToActivate() const;
  void CheckIfReadyToActivate();

  TileManagerClient* client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ResourcePool* resource_pool_;
  Rasterizer* rasterizer_;
  GlobalStateThatImpactsTilePriority global_state_;
  const size_t scheduled_raster_task_limit_;

  typedef base::hash_map<Tile::Id, Tile*> TileMap;
  TileMap tiles_;

  bool all_tiles_that_need_to_be_rasterized_are_scheduled_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool did_initialize_visible_tile_;
  bool did_check_for_completed_tasks_since_last_schedule_tasks_;
  bool did_oom_on_last_assign_;

  typedef base::hash_map<uint32_t, scoped_refptr<ImageDecodeTask>>
      PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  typedef base::hash_map<int, int> LayerCountMap;
  LayerCountMap used_layer_counts_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  std::vector<Tile*> released_tiles_;

  ResourceFormat resource_format_;

  // Queue used when scheduling raster tasks.
  RasterTaskQueue raster_queue_;

  std::vector<scoped_refptr<RasterTask>> orphan_raster_tasks_;

  UniqueNotifier ready_to_activate_check_notifier_;

  RasterTilePriorityQueue raster_priority_queue_;
  EvictionTilePriorityQueue eviction_priority_queue_;
  bool eviction_priority_queue_is_up_to_date_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
