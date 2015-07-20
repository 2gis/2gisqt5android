// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/append_quads_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/quads/draw_quad.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostOnDemandRasterPixelTest : public LayerTreePixelTest {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->impl_side_painting = true;
  }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    // Not enough memory available. Enforce on-demand rasterization.
    impl->SetMemoryPolicy(
        ManagedMemoryPolicy(1, gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                            1000));
  }

  void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) override {
    // Find the PictureLayerImpl ask it to append quads to check their material.
    // The PictureLayerImpl is assumed to be the first child of the root layer
    // in the active tree.
    PictureLayerImpl* picture_layer = static_cast<PictureLayerImpl*>(
        host_impl->active_tree()->root_layer()->child_at(0));

    scoped_ptr<RenderPass> render_pass = RenderPass::Create();

    AppendQuadsData data;
    picture_layer->AppendQuads(render_pass.get(), Occlusion(), &data);

    for (const auto& quad : render_pass->quad_list)
      EXPECT_EQ(quad->material, DrawQuad::PICTURE_CONTENT);

    // Triggers pixel readback and ends the test.
    LayerTreePixelTest::SwapBuffersOnThread(host_impl, result);
  }

  void RunOnDemandRasterPixelTest();
};

class BlueYellowLayerClient : public ContentLayerClient {
 public:
  explicit BlueYellowLayerClient(gfx::Rect layer_rect)
      : layer_rect_(layer_rect) {}

  void DidChangeLayerCanUseLCDText() override {}

  bool FillsBoundsCompletely() const override { return false; }

  void PaintContents(
      SkCanvas* canvas,
      const gfx::Rect& clip,
      ContentLayerClient::GraphicsContextStatus gc_status) override {
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    canvas->drawRect(SkRect::MakeWH(layer_rect_.width(),
                                    layer_rect_.height() / 2),
                     paint);

    paint.setColor(SK_ColorYELLOW);
    canvas->drawRect(
        SkRect::MakeXYWH(0,
                         layer_rect_.height() / 2,
                         layer_rect_.width(),
                         layer_rect_.height() / 2),
        paint);
  }

 private:
  gfx::Rect layer_rect_;
};

void LayerTreeHostOnDemandRasterPixelTest::RunOnDemandRasterPixelTest() {
  // Use multiple colors in a single layer to prevent bypassing on-demand
  // rasterization if a single solid color is detected in picture analysis.
  gfx::Rect layer_rect(200, 200);
  BlueYellowLayerClient client(layer_rect);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);

  layer->SetIsDrawable(true);
  layer->SetBounds(layer_rect.size());
  layer->SetPosition(layer_rect.origin());

  RunPixelTest(PIXEL_TEST_GL,
               layer,
               base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")));
}

TEST_F(LayerTreeHostOnDemandRasterPixelTest, RasterPictureLayer) {
  RunOnDemandRasterPixelTest();
}

class LayerTreeHostOnDemandRasterPixelTestWithGpuRasterizationForced
    : public LayerTreeHostOnDemandRasterPixelTest {
  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostOnDemandRasterPixelTest::InitializeSettings(settings);
    settings->gpu_rasterization_forced = true;
  }
};

TEST_F(LayerTreeHostOnDemandRasterPixelTestWithGpuRasterizationForced,
       RasterPictureLayer) {
  RunOnDemandRasterPixelTest();
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
