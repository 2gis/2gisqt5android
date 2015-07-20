// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENTS_SCALING_LAYER_H_
#define CC_LAYERS_CONTENTS_SCALING_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

// Base class for layers that need contents scale.
// The content bounds are determined by bounds and scale of the contents.
class CC_EXPORT ContentsScalingLayer : public Layer {
 public:
  void CalculateContentsScale(float ideal_contents_scale,
                              float* contents_scale_x,
                              float* contents_scale_y,
                              gfx::Size* content_bounds) override;

  bool Update(ResourceUpdateQueue* queue,
              const OcclusionTracker<Layer>* occlusion) override;

 protected:
  ContentsScalingLayer();
  ~ContentsScalingLayer() override;

  gfx::Size ComputeContentBoundsForScale(float scale_x, float scale_y) const;

 private:
  float last_update_contents_scale_x_;
  float last_update_contents_scale_y_;

  DISALLOW_COPY_AND_ASSIGN(ContentsScalingLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_CONTENTS_SCALING_LAYER_H__
