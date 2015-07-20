// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling_set.h"

#include <map>
#include <vector>

#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

TEST(PictureLayerTilingSetTest, NoResources) {
  FakePictureLayerTilingClient client;
  gfx::Size layer_bounds(1000, 800);
  PictureLayerTilingSet set(&client);
  client.SetTileSize(gfx::Size(256, 256));

  set.AddTiling(1.0, layer_bounds);
  set.AddTiling(1.5, layer_bounds);
  set.AddTiling(2.0, layer_bounds);

  float contents_scale = 2.0;
  gfx::Size content_bounds(
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));
  gfx::Rect content_rect(content_bounds);

  Region remaining(content_rect);
  PictureLayerTilingSet::CoverageIterator iter(
      &set,
      contents_scale,
      content_rect,
      contents_scale);
  for (; iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    EXPECT_TRUE(content_rect.Contains(geometry_rect));
    ASSERT_TRUE(remaining.Contains(geometry_rect));
    remaining.Subtract(geometry_rect);

    // No tiles have resources, so no iter represents a real tile.
    EXPECT_FALSE(*iter);
  }
  EXPECT_TRUE(remaining.IsEmpty());
}

TEST(PictureLayerTilingSetTest, TilingRange) {
  FakePictureLayerTilingClient client;
  gfx::Size layer_bounds(10, 10);
  PictureLayerTilingSet::TilingRange higher_than_high_res_range(0, 0);
  PictureLayerTilingSet::TilingRange high_res_range(0, 0);
  PictureLayerTilingSet::TilingRange between_high_and_low_res_range(0, 0);
  PictureLayerTilingSet::TilingRange low_res_range(0, 0);
  PictureLayerTilingSet::TilingRange lower_than_low_res_range(0, 0);
  PictureLayerTiling* high_res_tiling;
  PictureLayerTiling* low_res_tiling;

  PictureLayerTilingSet set(&client);
  set.AddTiling(2.0, layer_bounds);
  high_res_tiling = set.AddTiling(1.0, layer_bounds);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  set.AddTiling(0.5, layer_bounds);
  low_res_tiling = set.AddTiling(0.25, layer_bounds);
  low_res_tiling->set_resolution(LOW_RESOLUTION);
  set.AddTiling(0.125, layer_bounds);

  higher_than_high_res_range =
      set.GetTilingRange(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u, higher_than_high_res_range.start);
  EXPECT_EQ(1u, higher_than_high_res_range.end);

  high_res_range = set.GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(1u, high_res_range.start);
  EXPECT_EQ(2u, high_res_range.end);

  between_high_and_low_res_range =
      set.GetTilingRange(PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(2u, between_high_and_low_res_range.start);
  EXPECT_EQ(3u, between_high_and_low_res_range.end);

  low_res_range = set.GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(3u, low_res_range.start);
  EXPECT_EQ(4u, low_res_range.end);

  lower_than_low_res_range =
      set.GetTilingRange(PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(4u, lower_than_low_res_range.start);
  EXPECT_EQ(5u, lower_than_low_res_range.end);

  PictureLayerTilingSet set_without_low_res(&client);
  set_without_low_res.AddTiling(2.0, layer_bounds);
  high_res_tiling = set_without_low_res.AddTiling(1.0, layer_bounds);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  set_without_low_res.AddTiling(0.5, layer_bounds);
  set_without_low_res.AddTiling(0.25, layer_bounds);

  higher_than_high_res_range = set_without_low_res.GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u, higher_than_high_res_range.start);
  EXPECT_EQ(1u, higher_than_high_res_range.end);

  high_res_range =
      set_without_low_res.GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(1u, high_res_range.start);
  EXPECT_EQ(2u, high_res_range.end);

  between_high_and_low_res_range = set_without_low_res.GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(2u, between_high_and_low_res_range.start);
  EXPECT_EQ(4u, between_high_and_low_res_range.end);

  low_res_range =
      set_without_low_res.GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(0u, low_res_range.end - low_res_range.start);

  lower_than_low_res_range = set_without_low_res.GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);

  PictureLayerTilingSet set_with_only_high_and_low_res(&client);
  high_res_tiling = set_with_only_high_and_low_res.AddTiling(1.0, layer_bounds);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  low_res_tiling = set_with_only_high_and_low_res.AddTiling(0.5, layer_bounds);
  low_res_tiling->set_resolution(LOW_RESOLUTION);

  higher_than_high_res_range = set_with_only_high_and_low_res.GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u,
            higher_than_high_res_range.end - higher_than_high_res_range.start);

  high_res_range = set_with_only_high_and_low_res.GetTilingRange(
      PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(0u, high_res_range.start);
  EXPECT_EQ(1u, high_res_range.end);

  between_high_and_low_res_range =
      set_with_only_high_and_low_res.GetTilingRange(
          PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(0u,
            between_high_and_low_res_range.end -
                between_high_and_low_res_range.start);

  low_res_range = set_with_only_high_and_low_res.GetTilingRange(
      PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(1u, low_res_range.start);
  EXPECT_EQ(2u, low_res_range.end);

  lower_than_low_res_range = set_with_only_high_and_low_res.GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);

  PictureLayerTilingSet set_with_only_high_res(&client);
  high_res_tiling = set_with_only_high_res.AddTiling(1.0, layer_bounds);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);

  higher_than_high_res_range = set_with_only_high_res.GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u,
            higher_than_high_res_range.end - higher_than_high_res_range.start);

  high_res_range =
      set_with_only_high_res.GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(0u, high_res_range.start);
  EXPECT_EQ(1u, high_res_range.end);

  between_high_and_low_res_range = set_with_only_high_res.GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(0u,
            between_high_and_low_res_range.end -
                between_high_and_low_res_range.start);

  low_res_range =
      set_with_only_high_res.GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(0u, low_res_range.end - low_res_range.start);

  lower_than_low_res_range = set_with_only_high_res.GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);
}

class PictureLayerTilingSetTestWithResources : public testing::Test {
 public:
  void runTest(
      int num_tilings,
      float min_scale,
      float scale_increment,
      float ideal_contents_scale,
      float expected_scale) {
    FakeOutputSurfaceClient output_surface_client;
    scoped_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::Create3d();
    CHECK(output_surface->BindToClient(&output_surface_client));

    scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
        new TestSharedBitmapManager());
    scoped_ptr<ResourceProvider> resource_provider =
        ResourceProvider::Create(output_surface.get(),
                                 shared_bitmap_manager.get(),
                                 NULL,
                                 NULL,
                                 0,
                                 false,
                                 1);

    FakePictureLayerTilingClient client(resource_provider.get());
    client.SetTileSize(gfx::Size(256, 256));
    client.set_tree(PENDING_TREE);
    gfx::Size layer_bounds(1000, 800);
    PictureLayerTilingSet set(&client);

    float scale = min_scale;
    for (int i = 0; i < num_tilings; ++i, scale += scale_increment) {
      PictureLayerTiling* tiling = set.AddTiling(scale, layer_bounds);
      tiling->CreateAllTilesForTesting();
      std::vector<Tile*> tiles = tiling->AllTilesForTesting();
      client.tile_manager()->InitializeTilesWithResourcesForTesting(tiles);
    }

    float max_contents_scale = scale;
    gfx::Size content_bounds(
        gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, max_contents_scale)));
    gfx::Rect content_rect(content_bounds);

    Region remaining(content_rect);
    PictureLayerTilingSet::CoverageIterator iter(
        &set,
        max_contents_scale,
        content_rect,
        ideal_contents_scale);
    for (; iter; ++iter) {
      gfx::Rect geometry_rect = iter.geometry_rect();
      EXPECT_TRUE(content_rect.Contains(geometry_rect));
      ASSERT_TRUE(remaining.Contains(geometry_rect));
      remaining.Subtract(geometry_rect);

      float scale = iter.CurrentTiling()->contents_scale();
      EXPECT_EQ(expected_scale, scale);

      if (num_tilings)
        EXPECT_TRUE(*iter);
      else
        EXPECT_FALSE(*iter);
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }
};

TEST_F(PictureLayerTilingSetTestWithResources, NoTilings) {
  runTest(0, 0.f, 0.f, 2.f, 0.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Smaller) {
  runTest(1, 1.f, 0.f, 2.f, 1.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Larger) {
  runTest(1, 3.f, 0.f, 2.f, 3.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Smaller) {
  runTest(2, 1.f, 1.f, 3.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_SmallerEqual) {
  runTest(2, 1.f, 1.f, 2.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_LargerEqual) {
  runTest(2, 1.f, 1.f, 1.f, 1.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Larger) {
  runTest(2, 2.f, 8.f, 1.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, ManyTilings_Equal) {
  runTest(10, 1.f, 1.f, 5.f, 5.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, ManyTilings_NotEqual) {
  runTest(10, 1.f, 1.f, 4.5f, 5.f);
}

class PictureLayerTilingSetSyncTest : public testing::Test {
 public:
  PictureLayerTilingSetSyncTest()
      : tile_size_(gfx::Size(10, 10)),
        source_bounds_(gfx::Size(30, 20)),
        target_bounds_(gfx::Size(30, 30)) {
    source_client_.SetTileSize(tile_size_);
    source_client_.set_tree(PENDING_TREE);
    target_client_.SetTileSize(tile_size_);
    target_client_.set_tree(PENDING_TREE);
    source_.reset(new PictureLayerTilingSet(&source_client_));
    target_.reset(new PictureLayerTilingSet(&target_client_));
  }

  // Sync from source to target.
  void SyncTilings(const gfx::Size& new_bounds,
                   const Region& invalidation,
                   float minimum_scale) {
    for (size_t i = 0; i < source_->num_tilings(); ++i)
      source_->tiling_at(i)->CreateAllTilesForTesting();
    for (size_t i = 0; i < target_->num_tilings(); ++i)
      target_->tiling_at(i)->CreateAllTilesForTesting();

    target_->SyncTilings(
        *source_.get(), new_bounds, invalidation, minimum_scale);
  }
  void SyncTilings(const gfx::Size& new_bounds) {
    Region invalidation;
    SyncTilings(new_bounds, invalidation, 0.f);
  }
  void SyncTilings(const gfx::Size& new_bounds, const Region& invalidation) {
    SyncTilings(new_bounds, invalidation, 0.f);
  }
  void SyncTilings(const gfx::Size& new_bounds, float minimum_scale) {
    Region invalidation;
    SyncTilings(new_bounds, invalidation, minimum_scale);
  }

  void VerifyTargetEqualsSource(const gfx::Size& new_bounds) {
    ASSERT_FALSE(new_bounds.IsEmpty());
    EXPECT_EQ(target_->num_tilings(), source_->num_tilings());

    for (size_t i = 0; i < target_->num_tilings(); ++i) {
      ASSERT_GT(source_->num_tilings(), i);
      const PictureLayerTiling* source_tiling = source_->tiling_at(i);
      const PictureLayerTiling* target_tiling = target_->tiling_at(i);
      EXPECT_EQ(target_tiling->layer_bounds().ToString(),
                new_bounds.ToString());
      EXPECT_EQ(source_tiling->contents_scale(),
                target_tiling->contents_scale());
    }

    EXPECT_EQ(source_->client(), &source_client_);
    EXPECT_EQ(target_->client(), &target_client_);
    ValidateTargetTilingSet();
  }

  void ValidateTargetTilingSet() {
    // Tilings should be sorted largest to smallest.
    if (target_->num_tilings() > 0) {
      float last_scale = target_->tiling_at(0)->contents_scale();
      for (size_t i = 1; i < target_->num_tilings(); ++i) {
        const PictureLayerTiling* target_tiling = target_->tiling_at(i);
        EXPECT_LT(target_tiling->contents_scale(), last_scale);
        last_scale = target_tiling->contents_scale();
      }
    }

    for (size_t i = 0; i < target_->num_tilings(); ++i)
      ValidateTiling(target_->tiling_at(i), target_client_.GetRasterSource());
  }

  void ValidateTiling(const PictureLayerTiling* tiling,
                      const RasterSource* raster_source) {
    if (tiling->tiling_size().IsEmpty()) {
      EXPECT_TRUE(tiling->live_tiles_rect().IsEmpty());
    } else if (!tiling->live_tiles_rect().IsEmpty()) {
      gfx::Rect tiling_rect(tiling->tiling_size());
      EXPECT_TRUE(tiling_rect.Contains(tiling->live_tiles_rect()));
    }

    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i) {
      const Tile* tile = tiles[i];
      ASSERT_TRUE(!!tile);
      EXPECT_EQ(tile->raster_source(), raster_source);
      EXPECT_TRUE(tile->content_rect().Intersects(tiling->live_tiles_rect()))
          << "All tiles must be inside the live tiles rect."
          << " Tile rect: " << tile->content_rect().ToString()
          << " Live rect: " << tiling->live_tiles_rect().ToString()
          << " Scale: " << tiling->contents_scale();
    }

    for (PictureLayerTiling::CoverageIterator iter(
             tiling, tiling->contents_scale(), tiling->live_tiles_rect());
         iter;
         ++iter) {
      EXPECT_TRUE(*iter) << "The live tiles rect must be full.";
    }
  }

  gfx::Size tile_size_;

  FakePictureLayerTilingClient source_client_;
  gfx::Size source_bounds_;
  scoped_ptr<PictureLayerTilingSet> source_;

  FakePictureLayerTilingClient target_client_;
  gfx::Size target_bounds_;
  scoped_ptr<PictureLayerTilingSet> target_;
};

TEST_F(PictureLayerTilingSetSyncTest, EmptyBounds) {
  float source_scales[] = {1.f, 1.2f};
  for (size_t i = 0; i < arraysize(source_scales); ++i)
    source_->AddTiling(source_scales[i], source_bounds_);

  gfx::Size empty_bounds;
  SyncTilings(empty_bounds);
  EXPECT_EQ(target_->num_tilings(), 0u);
}

TEST_F(PictureLayerTilingSetSyncTest, AllNew) {
  float source_scales[] = {0.5f, 1.f, 1.2f};
  for (size_t i = 0; i < arraysize(source_scales); ++i)
    source_->AddTiling(source_scales[i], source_bounds_);
  float target_scales[] = {0.75f, 1.4f, 3.f};
  for (size_t i = 0; i < arraysize(target_scales); ++i)
    target_->AddTiling(target_scales[i], target_bounds_);

  gfx::Size new_bounds(15, 40);
  SyncTilings(new_bounds);
  VerifyTargetEqualsSource(new_bounds);
}

Tile* FindTileAtOrigin(PictureLayerTiling* tiling) {
  std::vector<Tile*> tiles = tiling->AllTilesForTesting();
  for (size_t i = 0; i < tiles.size(); ++i) {
    if (tiles[i]->content_rect().origin() == gfx::Point())
      return tiles[i];
  }
  return NULL;
}

TEST_F(PictureLayerTilingSetSyncTest, KeepExisting) {
  float source_scales[] = {0.7f, 1.f, 1.1f, 2.f};
  for (size_t i = 0; i < arraysize(source_scales); ++i)
    source_->AddTiling(source_scales[i], source_bounds_);
  float target_scales[] = {0.5f, 1.f, 2.f};
  for (size_t i = 0; i < arraysize(target_scales); ++i)
    target_->AddTiling(target_scales[i], target_bounds_);

  PictureLayerTiling* tiling1 = source_->TilingAtScale(1.f);
  ASSERT_TRUE(tiling1);
  tiling1->CreateAllTilesForTesting();
  EXPECT_EQ(tiling1->contents_scale(), 1.f);
  Tile* tile1 = FindTileAtOrigin(tiling1);
  ASSERT_TRUE(tile1);

  PictureLayerTiling* tiling2 = source_->TilingAtScale(2.f);
  tiling2->CreateAllTilesForTesting();
  ASSERT_TRUE(tiling2);
  EXPECT_EQ(tiling2->contents_scale(), 2.f);
  Tile* tile2 = FindTileAtOrigin(tiling2);
  ASSERT_TRUE(tile2);

  gfx::Size new_bounds(15, 40);
  SyncTilings(new_bounds);
  VerifyTargetEqualsSource(new_bounds);

  EXPECT_EQ(tiling1, source_->TilingAtScale(1.f));
  EXPECT_EQ(tile1, FindTileAtOrigin(tiling1));
  EXPECT_FALSE(tiling1->live_tiles_rect().IsEmpty());

  EXPECT_EQ(tiling2, source_->TilingAtScale(2.f));
  EXPECT_EQ(tile2, FindTileAtOrigin(tiling2));
  EXPECT_FALSE(tiling2->live_tiles_rect().IsEmpty());
}

TEST_F(PictureLayerTilingSetSyncTest, EmptySet) {
  float target_scales[] = {0.2f, 1.f};
  for (size_t i = 0; i < arraysize(target_scales); ++i)
    target_->AddTiling(target_scales[i], target_bounds_);

  gfx::Size new_bounds(15, 40);
  SyncTilings(new_bounds);
  VerifyTargetEqualsSource(new_bounds);
}

TEST_F(PictureLayerTilingSetSyncTest, MinimumScale) {
  float source_scales[] = {0.7f, 1.f, 1.1f, 2.f};
  for (size_t i = 0; i < arraysize(source_scales); ++i)
    source_->AddTiling(source_scales[i], source_bounds_);
  float target_scales[] = {0.5f, 0.7f, 1.f, 1.1f, 2.f};
  for (size_t i = 0; i < arraysize(target_scales); ++i)
    target_->AddTiling(target_scales[i], target_bounds_);

  gfx::Size new_bounds(15, 40);
  float minimum_scale = 1.5f;
  SyncTilings(new_bounds, minimum_scale);

  EXPECT_EQ(target_->num_tilings(), 1u);
  EXPECT_EQ(target_->tiling_at(0)->contents_scale(), 2.f);
  ValidateTargetTilingSet();
}

TEST_F(PictureLayerTilingSetSyncTest, Invalidation) {
  source_->AddTiling(2.f, source_bounds_);
  target_->AddTiling(2.f, target_bounds_);
  target_->tiling_at(0)->CreateAllTilesForTesting();

  Region layer_invalidation;
  layer_invalidation.Union(gfx::Rect(0, 0, 1, 1));
  layer_invalidation.Union(gfx::Rect(0, 15, 1, 1));
  // Out of bounds layer_invalidation.
  layer_invalidation.Union(gfx::Rect(100, 100, 1, 1));

  Region content_invalidation;
  for (Region::Iterator iter(layer_invalidation); iter.has_rect();
       iter.next()) {
    gfx::Rect content_rect = gfx::ScaleToEnclosingRect(iter.rect(), 2.f);
    content_invalidation.Union(content_rect);
  }

  std::vector<Tile*> old_tiles = target_->tiling_at(0)->AllTilesForTesting();
  std::map<gfx::Point, scoped_refptr<Tile>> old_tile_map;
  for (size_t i = 0; i < old_tiles.size(); ++i)
    old_tile_map[old_tiles[i]->content_rect().origin()] = old_tiles[i];

  SyncTilings(target_bounds_, layer_invalidation);
  VerifyTargetEqualsSource(target_bounds_);

  std::vector<Tile*> new_tiles = target_->tiling_at(0)->AllTilesForTesting();
  for (size_t i = 0; i < new_tiles.size(); ++i) {
    const Tile* tile = new_tiles[i];
    std::map<gfx::Point, scoped_refptr<Tile>>::iterator find =
        old_tile_map.find(tile->content_rect().origin());
    if (content_invalidation.Intersects(tile->content_rect()))
      EXPECT_NE(tile, find->second.get());
    else
      EXPECT_EQ(tile, find->second.get());
  }
}

TEST_F(PictureLayerTilingSetSyncTest, TileSizeChange) {
  source_->AddTiling(1.f, source_bounds_);
  target_->AddTiling(1.f, target_bounds_);

  target_->tiling_at(0)->CreateAllTilesForTesting();
  std::vector<Tile*> original_tiles =
      target_->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(original_tiles.size(), 0u);
  gfx::Size new_tile_size(100, 100);
  target_client_.SetTileSize(new_tile_size);
  EXPECT_NE(target_->tiling_at(0)->tile_size().ToString(),
            new_tile_size.ToString());

  gfx::Size new_bounds(15, 40);
  SyncTilings(new_bounds);
  VerifyTargetEqualsSource(new_bounds);

  EXPECT_EQ(target_->tiling_at(0)->tile_size().ToString(),
            new_tile_size.ToString());

  // All old tiles should not be present in new tiles.
  std::vector<Tile*> new_tiles = target_->tiling_at(0)->AllTilesForTesting();
  for (size_t i = 0; i < original_tiles.size(); ++i) {
    std::vector<Tile*>::iterator find =
        std::find(new_tiles.begin(), new_tiles.end(), original_tiles[i]);
    EXPECT_TRUE(find == new_tiles.end());
  }
}

}  // namespace
}  // namespace cc
