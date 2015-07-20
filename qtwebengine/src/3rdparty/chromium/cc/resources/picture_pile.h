// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_H_
#define CC_RESOURCES_PICTURE_PILE_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/picture_pile_base.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class PicturePileImpl;
class Region;
class RenderingStatsInstrumentation;

class CC_EXPORT PicturePile : public PicturePileBase {
 public:
  PicturePile();
  ~PicturePile() override;

  // Re-record parts of the picture that are invalid.
  // Invalidations are in layer space, and will be expanded to cover everything
  // that was either recorded/changed or that has no recording, leaving out only
  // pieces that we had a recording for and it was not changed.
  // Return true iff the pile was modified.
  bool UpdateAndExpandInvalidation(
      ContentLayerClient* painter,
      Region* invalidation,
      SkColor background_color,
      bool contents_opaque,
      bool contents_fill_bounds_completely,
      const gfx::Size& layer_size,
      const gfx::Rect& visible_layer_rect,
      int frame_number,
      Picture::RecordingMode recording_mode,
      RenderingStatsInstrumentation* stats_instrumentation);

  void SetEmptyBounds();

  void set_slow_down_raster_scale_factor(int factor) {
    slow_down_raster_scale_factor_for_debug_ = factor;
  }

  void set_show_debug_picture_borders(bool show) {
    show_debug_picture_borders_ = show;
  }

  bool is_suitable_for_gpu_rasterization() const {
    return is_suitable_for_gpu_rasterization_;
  }
  void SetUnsuitableForGpuRasterizationForTesting() {
    is_suitable_for_gpu_rasterization_ = false;
  }

  void SetPixelRecordDistanceForTesting(int d) { pixel_record_distance_ = d; }

 protected:
  // An internal CanRaster check that goes to the picture_map rather than
  // using the recorded_viewport hint.
  bool CanRasterSlowTileCheck(const gfx::Rect& layer_rect) const;

 private:
  friend class PicturePileImpl;

  void DetermineIfSolidColor();

  bool is_suitable_for_gpu_rasterization_;
  int pixel_record_distance_;

  DISALLOW_COPY_AND_ASSIGN(PicturePile);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_H_
