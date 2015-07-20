// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_pile_base.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/debug/trace_event_argument.h"
#include "base/logging.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {
// Dimensions of the tiles in this picture pile as well as the dimensions of
// the base picture in each tile.
const int kBasePictureSize = 512;
const int kTileGridBorderPixels = 1;
#ifdef NDEBUG
const bool kDefaultClearCanvasSetting = false;
#else
const bool kDefaultClearCanvasSetting = true;
#endif

// Invalidation frequency settings. kInvalidationFrequencyThreshold is a value
// between 0 and 1 meaning invalidation frequency between 0% and 100% that
// indicates when to stop invalidating offscreen regions.
// kFrequentInvalidationDistanceThreshold defines what it means to be
// "offscreen" in terms of distance to visible in css pixels.
const float kInvalidationFrequencyThreshold = 0.75f;
const int kFrequentInvalidationDistanceThreshold = 512;

}  // namespace

namespace cc {

PicturePileBase::PicturePileBase()
    : min_contents_scale_(0),
      background_color_(SkColorSetARGBInline(0, 0, 0, 0)),
      slow_down_raster_scale_factor_for_debug_(0),
      contents_opaque_(false),
      contents_fill_bounds_completely_(false),
      show_debug_picture_borders_(false),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      has_any_recordings_(false),
      is_mask_(false),
      is_solid_color_(false),
      solid_color_(SK_ColorTRANSPARENT) {
  tiling_.SetMaxTextureSize(gfx::Size(kBasePictureSize, kBasePictureSize));
  tile_grid_info_.fTileInterval.setEmpty();
  tile_grid_info_.fMargin.setEmpty();
  tile_grid_info_.fOffset.setZero();
}

PicturePileBase::PicturePileBase(const PicturePileBase* other)
    : picture_map_(other->picture_map_),
      tiling_(other->tiling_),
      recorded_viewport_(other->recorded_viewport_),
      min_contents_scale_(other->min_contents_scale_),
      tile_grid_info_(other->tile_grid_info_),
      background_color_(other->background_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      contents_opaque_(other->contents_opaque_),
      contents_fill_bounds_completely_(other->contents_fill_bounds_completely_),
      show_debug_picture_borders_(other->show_debug_picture_borders_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      has_any_recordings_(other->has_any_recordings_),
      is_mask_(other->is_mask_),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_) {
}

PicturePileBase::~PicturePileBase() {
}

void PicturePileBase::SetMinContentsScale(float min_contents_scale) {
  DCHECK(min_contents_scale);
  if (min_contents_scale_ == min_contents_scale)
    return;

  // Picture contents are played back scaled. When the final contents scale is
  // less than 1 (i.e. low res), then multiple recorded pixels will be used
  // to raster one final pixel.  To avoid splitting a final pixel across
  // pictures (which would result in incorrect rasterization due to blending), a
  // buffer margin is added so that any picture can be snapped to integral
  // final pixels.
  //
  // For example, if a 1/4 contents scale is used, then that would be 3 buffer
  // pixels, since that's the minimum number of pixels to add so that resulting
  // content can be snapped to a four pixel aligned grid.
  int buffer_pixels = static_cast<int>(ceil(1 / min_contents_scale) - 1);
  buffer_pixels = std::max(0, buffer_pixels);
  SetBufferPixels(buffer_pixels);
  min_contents_scale_ = min_contents_scale;
}

// static
void PicturePileBase::ComputeTileGridInfo(
    const gfx::Size& tile_grid_size,
    SkTileGridFactory::TileGridInfo* info) {
  DCHECK(info);
  info->fTileInterval.set(tile_grid_size.width() - 2 * kTileGridBorderPixels,
                          tile_grid_size.height() - 2 * kTileGridBorderPixels);
  DCHECK_GT(info->fTileInterval.width(), 0);
  DCHECK_GT(info->fTileInterval.height(), 0);
  info->fMargin.set(kTileGridBorderPixels, kTileGridBorderPixels);
  // Offset the tile grid coordinate space to take into account the fact
  // that the top-most and left-most tiles do not have top and left borders
  // respectively.
  info->fOffset.set(-kTileGridBorderPixels, -kTileGridBorderPixels);
}

void PicturePileBase::SetTileGridSize(const gfx::Size& tile_grid_size) {
  ComputeTileGridInfo(tile_grid_size, &tile_grid_info_);
}

void PicturePileBase::SetBufferPixels(int new_buffer_pixels) {
  if (new_buffer_pixels == buffer_pixels())
    return;

  Clear();
  tiling_.SetBorderTexels(new_buffer_pixels);
}

void PicturePileBase::Clear() {
  picture_map_.clear();
  recorded_viewport_ = gfx::Rect();
  has_any_recordings_ = false;
  is_solid_color_ = false;
}

bool PicturePileBase::HasRecordingAt(int x, int y) {
  PictureMap::const_iterator found = picture_map_.find(PictureMapKey(x, y));
  if (found == picture_map_.end())
    return false;
  return !!found->second.GetPicture();
}

gfx::Rect PicturePileBase::PaddedRect(const PictureMapKey& key) const {
  gfx::Rect tile = tiling_.TileBounds(key.first, key.second);
  return PadRect(tile);
}

gfx::Rect PicturePileBase::PadRect(const gfx::Rect& rect) const {
  gfx::Rect padded_rect = rect;
  padded_rect.Inset(
      -buffer_pixels(), -buffer_pixels(), -buffer_pixels(), -buffer_pixels());
  return padded_rect;
}

PicturePileBase::PictureInfo::PictureInfo() : last_frame_number_(0) {}

PicturePileBase::PictureInfo::~PictureInfo() {}

void PicturePileBase::PictureInfo::AdvanceInvalidationHistory(
    int frame_number) {
  DCHECK_GE(frame_number, last_frame_number_);
  if (frame_number == last_frame_number_)
    return;

  invalidation_history_ <<= (frame_number - last_frame_number_);
  last_frame_number_ = frame_number;
}

bool PicturePileBase::PictureInfo::Invalidate(int frame_number) {
  AdvanceInvalidationHistory(frame_number);
  invalidation_history_.set(0);

  bool did_invalidate = !!picture_.get();
  picture_ = NULL;
  return did_invalidate;
}

bool PicturePileBase::PictureInfo::NeedsRecording(int frame_number,
                                                  int distance_to_visible) {
  AdvanceInvalidationHistory(frame_number);

  // We only need recording if we don't have a picture. Furthermore, we only
  // need a recording if we're within frequent invalidation distance threshold
  // or the invalidation is not frequent enough (below invalidation frequency
  // threshold).
  return !picture_.get() &&
         ((distance_to_visible <= kFrequentInvalidationDistanceThreshold) ||
          (GetInvalidationFrequency() < kInvalidationFrequencyThreshold));
}

void PicturePileBase::PictureInfo::SetPicture(scoped_refptr<Picture> picture) {
  picture_ = picture;
}

const Picture* PicturePileBase::PictureInfo::GetPicture() const {
  return picture_.get();
}

float PicturePileBase::PictureInfo::GetInvalidationFrequency() const {
  return invalidation_history_.count() /
         static_cast<float>(INVALIDATION_FRAMES_TRACKED);
}

}  // namespace cc
