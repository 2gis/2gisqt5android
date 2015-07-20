// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

SkXfermode::Mode const kBlendModes[] = {
    SkXfermode::kSrcOver_Mode,   SkXfermode::kScreen_Mode,
    SkXfermode::kOverlay_Mode,   SkXfermode::kDarken_Mode,
    SkXfermode::kLighten_Mode,   SkXfermode::kColorDodge_Mode,
    SkXfermode::kColorBurn_Mode, SkXfermode::kHardLight_Mode,
    SkXfermode::kSoftLight_Mode, SkXfermode::kDifference_Mode,
    SkXfermode::kExclusion_Mode, SkXfermode::kMultiply_Mode,
    SkXfermode::kHue_Mode,       SkXfermode::kSaturation_Mode,
    SkXfermode::kColor_Mode,     SkXfermode::kLuminosity_Mode};

SkColor kCSSTestColors[] = {
    0xffff0000,  // red
    0xff00ff00,  // lime
    0xff0000ff,  // blue
    0xff00ffff,  // aqua
    0xffff00ff,  // fuchsia
    0xffffff00,  // yellow
    0xff008000,  // green
    0xff800000,  // maroon
    0xff000080,  // navy
    0xff800080,  // purple
    0xff808000,  // olive
    0xff008080,  // teal
    0xfffa8072,  // salmon
    0xffc0c0c0,  // silver
    0xff000000,  // black
    0xff808080,  // gray
    0x80000000,  // black with transparency
    0xffffffff,  // white
    0x80ffffff,  // white with transparency
    0x00000000   // transparent
};

const int kBlendModesCount = arraysize(kBlendModes);
const int kCSSTestColorsCount = arraysize(kCSSTestColors);

using RenderPassOptions = uint32;
const uint32 kUseMasks = 1 << 0;
const uint32 kUseAntialiasing = 1 << 1;
const uint32 kUseColorMatrix = 1 << 2;
const uint32 kForceShaders = 1 << 3;

class LayerTreeHostBlendingPixelTest : public LayerTreePixelTest {
 public:
  LayerTreeHostBlendingPixelTest()
      : force_antialiasing_(false), force_blending_with_shaders_(false) {
    pixel_comparator_.reset(new FuzzyPixelOffByOneComparator(true));
  }

  virtual void InitializeSettings(LayerTreeSettings* settings) override {
    settings->force_antialiasing = force_antialiasing_;
    settings->force_blending_with_shaders = force_blending_with_shaders_;
  }

 protected:
  void RunBlendingWithRootPixelTestType(PixelTestType type) {
    const int kLaneWidth = 15;
    const int kLaneHeight = kBlendModesCount * kLaneWidth;
    const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSOrange);

    // Orange child layers will blend with the green background
    for (int i = 0; i < kBlendModesCount; ++i) {
      gfx::Rect child_rect(
          (i + 1) * kLaneWidth, kLaneWidth, kLaneWidth, kLaneHeight);
      scoped_refptr<SolidColorLayer> green_lane =
          CreateSolidColorLayer(child_rect, kCSSGreen);
      background->AddChild(green_lane);
      green_lane->SetBlendMode(kBlendModes[i]);
    }

    RunPixelTest(type,
                 background,
                 base::FilePath(FILE_PATH_LITERAL("blending_with_root.png")));
  }

  void RunBlendingWithTransparentPixelTestType(PixelTestType type) {
    const int kLaneWidth = 15;
    const int kLaneHeight = kBlendModesCount * kLaneWidth;
    const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

    scoped_refptr<SolidColorLayer> root =
        CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSBrown);

    scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
        gfx::Rect(0, kLaneWidth * 2, kRootSize, kLaneWidth), kCSSOrange);

    root->AddChild(background);
    background->SetIsRootForIsolatedGroup(true);

    // Orange child layers will blend with the green background
    for (int i = 0; i < kBlendModesCount; ++i) {
      gfx::Rect child_rect(
          (i + 1) * kLaneWidth, -kLaneWidth, kLaneWidth, kLaneHeight);
      scoped_refptr<SolidColorLayer> green_lane =
          CreateSolidColorLayer(child_rect, kCSSGreen);
      background->AddChild(green_lane);
      green_lane->SetBlendMode(kBlendModes[i]);
    }

    RunPixelTest(type,
                 root,
                 base::FilePath(FILE_PATH_LITERAL("blending_transparent.png")));
  }

  scoped_refptr<Layer> CreateColorfulBackdropLayer(int width, int height) {
    // Draw the backdrop with horizontal lanes.
    const int kLaneWidth = width;
    const int kLaneHeight = height / kCSSTestColorsCount;
    SkBitmap backing_store;
    backing_store.allocN32Pixels(width, height);
    SkCanvas canvas(backing_store);
    canvas.clear(SK_ColorTRANSPARENT);
    for (int i = 0; i < kCSSTestColorsCount; ++i) {
      SkPaint paint;
      paint.setColor(kCSSTestColors[i]);
      canvas.drawRect(
          SkRect::MakeXYWH(0, i * kLaneHeight, kLaneWidth, kLaneHeight), paint);
    }
    scoped_refptr<ImageLayer> layer = ImageLayer::Create();
    layer->SetIsDrawable(true);
    layer->SetBounds(gfx::Size(width, height));
    layer->SetBitmap(backing_store);
    return layer;
  }

  void SetupMaskLayer(scoped_refptr<Layer> layer) {
    const int kMaskOffset = 5;
    gfx::Size bounds = layer->bounds();
    scoped_refptr<ImageLayer> mask = ImageLayer::Create();
    mask->SetIsDrawable(true);
    mask->SetIsMask(true);
    mask->SetBounds(bounds);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(bounds.width(), bounds.height());
    SkCanvas canvas(bitmap);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.drawRect(SkRect::MakeXYWH(kMaskOffset,
                                     kMaskOffset,
                                     bounds.width() - kMaskOffset * 2,
                                     bounds.height() - kMaskOffset * 2),
                    paint);
    mask->SetBitmap(bitmap);
    layer->SetMaskLayer(mask.get());
  }

  void SetupColorMatrix(scoped_refptr<Layer> layer) {
    FilterOperations filter_operations;
    filter_operations.Append(FilterOperation::CreateSepiaFilter(1.f));
    layer->SetFilters(filter_operations);
  }

  void CreateBlendingColorLayers(int width,
                                 int height,
                                 scoped_refptr<Layer> background,
                                 RenderPassOptions flags) {
    const int kLanesCount = kBlendModesCount + 4;
    int lane_width = width / kLanesCount;
    const SkColor kMiscOpaqueColor = 0xffc86464;
    const SkColor kMiscTransparentColor = 0x80c86464;
    const SkXfermode::Mode kCoeffBlendMode = SkXfermode::kScreen_Mode;
    const SkXfermode::Mode kShaderBlendMode = SkXfermode::kColorBurn_Mode;
    // add vertical lanes with each of the blend modes
    for (int i = 0; i < kLanesCount; ++i) {
      gfx::Rect child_rect(i * lane_width, 0, lane_width, height);
      SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
      float opacity = 1.f;
      SkColor color = kMiscOpaqueColor;

      if (i < kBlendModesCount) {
        blend_mode = kBlendModes[i];
      } else if (i == kBlendModesCount) {
        blend_mode = kCoeffBlendMode;
        opacity = 0.5f;
      } else if (i == kBlendModesCount + 1) {
        blend_mode = kCoeffBlendMode;
        color = kMiscTransparentColor;
      } else if (i == kBlendModesCount + 2) {
        blend_mode = kShaderBlendMode;
        opacity = 0.5f;
      } else if (i == kBlendModesCount + 3) {
        blend_mode = kShaderBlendMode;
        color = kMiscTransparentColor;
      }

      scoped_refptr<SolidColorLayer> lane =
          CreateSolidColorLayer(child_rect, color);
      lane->SetBlendMode(blend_mode);
      lane->SetOpacity(opacity);
      lane->SetForceRenderSurface(true);
      if (flags & kUseMasks)
        SetupMaskLayer(lane);
      if (flags & kUseColorMatrix) {
        SetupColorMatrix(lane);
      }
      background->AddChild(lane);
    }
  }

  void RunBlendingWithRenderPass(PixelTestType type,
                                 const base::FilePath::CharType* expected_path,
                                 RenderPassOptions flags) {
    const int kRootSize = 400;

    scoped_refptr<SolidColorLayer> root =
        CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), SK_ColorWHITE);
    scoped_refptr<Layer> background =
        CreateColorfulBackdropLayer(kRootSize, kRootSize);

    background->SetIsRootForIsolatedGroup(true);
    root->AddChild(background);

    CreateBlendingColorLayers(kRootSize, kRootSize, background.get(), flags);

    this->impl_side_painting_ = false;
    this->force_antialiasing_ = (flags & kUseAntialiasing);
    this->force_blending_with_shaders_ = (flags & kForceShaders);

    if ((flags & kUseAntialiasing) && (type == PIXEL_TEST_GL)) {
      // Anti aliasing causes differences up to 7 pixels at the edges.
      // Several pixels have 9 units difference on the alpha channel.
      int large_error_allowed = (flags & kUseMasks) ? 7 : 9;
      // Blending results might differ with one pixel.
      int small_error_allowed = 1;
      // Most of the errors are one pixel errors.
      float percentage_pixels_small_error = (flags & kUseMasks) ? 7.7f : 12.1f;
      // Because of anti-aliasing, around 3% of pixels (at the edges) have
      // bigger errors (from small_error_allowed + 1 to large_error_allowed).
      float percentage_pixels_error = (flags & kUseMasks) ? 12.4f : 15.f;
      // The average error is still close to 1.
      float average_error_allowed_in_bad_pixels =
          (flags & kUseMasks) ? 1.3f : 1.f;

      // The sepia filter generates more small errors, but the number of large
      // errors remains around 3%.
      if (flags & kUseColorMatrix) {
        percentage_pixels_small_error = (flags & kUseMasks) ? 14.0f : 26.f;
        percentage_pixels_error = (flags & kUseMasks) ? 18.5f : 29.f;
        average_error_allowed_in_bad_pixels = (flags & kUseMasks) ? 0.9f : 0.7f;
      }

      pixel_comparator_.reset(
          new FuzzyPixelComparator(false,  // discard_alpha
                                   percentage_pixels_error,
                                   percentage_pixels_small_error,
                                   average_error_allowed_in_bad_pixels,
                                   large_error_allowed,
                                   small_error_allowed));
    } else if ((flags & kUseColorMatrix) && (type == PIXEL_TEST_GL)) {
      float percentage_pixels_error = 100.f;
      float percentage_pixels_small_error = 0.f;
      float average_error_allowed_in_bad_pixels = 1.f;
      int large_error_allowed = 2;
      int small_error_allowed = 0;
      pixel_comparator_.reset(
          new FuzzyPixelComparator(false,  // discard_alpha
                                   percentage_pixels_error,
                                   percentage_pixels_small_error,
                                   average_error_allowed_in_bad_pixels,
                                   large_error_allowed,
                                   small_error_allowed));
    }

    RunPixelTest(type, root, base::FilePath(expected_path));
  }

  bool force_antialiasing_;
  bool force_blending_with_shaders_;
};

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRoot_GL) {
  RunBlendingWithRootPixelTestType(PIXEL_TEST_GL);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRoot_Software) {
  RunBlendingWithRootPixelTestType(PIXEL_TEST_SOFTWARE);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithBackgroundFilter) {
  const int kLaneWidth = 15;
  const int kLaneHeight = kBlendModesCount * kLaneWidth;
  const int kRootSize = (kBlendModesCount + 2) * kLaneWidth;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(kRootSize, kRootSize), kCSSOrange);

  // Orange child layers have a background filter set and they will blend with
  // the green background
  for (int i = 0; i < kBlendModesCount; ++i) {
    gfx::Rect child_rect(
        (i + 1) * kLaneWidth, kLaneWidth, kLaneWidth, kLaneHeight);
    scoped_refptr<SolidColorLayer> green_lane =
        CreateSolidColorLayer(child_rect, kCSSGreen);
    background->AddChild(green_lane);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(.75));
    green_lane->SetBackgroundFilters(filters);
    green_lane->SetBlendMode(kBlendModes[i]);
  }

  RunPixelTest(PIXEL_TEST_GL,
               background,
               base::FilePath(FILE_PATH_LITERAL("blending_and_filter.png")));
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithTransparent_GL) {
  RunBlendingWithTransparentPixelTestType(PIXEL_TEST_GL);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithTransparent_Software) {
  RunBlendingWithTransparentPixelTestType(PIXEL_TEST_SOFTWARE);
}

// Tests for render passes
TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPass_GL) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_GL, FILE_PATH_LITERAL("blending_render_pass.png"), 0);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPass_Software) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_SOFTWARE, FILE_PATH_LITERAL("blending_render_pass.png"), 0);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kUseAntialiasing);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassAA_Software) {
  RunBlendingWithRenderPass(PIXEL_TEST_SOFTWARE,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kUseAntialiasing);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassWithMask_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMask_Software) {
  RunBlendingWithRenderPass(PIXEL_TEST_SOFTWARE,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassWithMaskAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks | kUseAntialiasing);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskAA_Software) {
  RunBlendingWithRenderPass(PIXEL_TEST_SOFTWARE,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks | kUseAntialiasing);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassColorMatrix_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_cm.png"),
                            kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassColorMatrix_Software) {
  RunBlendingWithRenderPass(PIXEL_TEST_SOFTWARE,
                            FILE_PATH_LITERAL("blending_render_pass_cm.png"),
                            kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassColorMatrixAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_cm.png"),
                            kUseAntialiasing | kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassColorMatrixAA_Software) {
  RunBlendingWithRenderPass(PIXEL_TEST_SOFTWARE,
                            FILE_PATH_LITERAL("blending_render_pass_cm.png"),
                            kUseAntialiasing | kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrix_GL) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_GL,
      FILE_PATH_LITERAL("blending_render_pass_mask_cm.png"),
      kUseMasks | kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrix_Software) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_SOFTWARE,
      FILE_PATH_LITERAL("blending_render_pass_mask_cm.png"),
      kUseMasks | kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrixAA_GL) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_GL,
      FILE_PATH_LITERAL("blending_render_pass_mask_cm.png"),
      kUseMasks | kUseAntialiasing | kUseColorMatrix);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrixAA_Software) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_SOFTWARE,
      FILE_PATH_LITERAL("blending_render_pass_mask_cm.png"),
      kUseMasks | kUseAntialiasing | kUseColorMatrix);
}

// Tests for render passes forcing shaders for all the blend modes.
TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassShaders_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassShadersAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kUseAntialiasing | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMask_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks | kUseAntialiasing | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersColorMatrix_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kUseColorMatrix | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersColorMatrixAA_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass.png"),
                            kUseAntialiasing | kUseColorMatrix | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskColorMatrix_GL) {
  RunBlendingWithRenderPass(PIXEL_TEST_GL,
                            FILE_PATH_LITERAL("blending_render_pass_mask.png"),
                            kUseMasks | kUseColorMatrix | kForceShaders);
}

TEST_F(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskColorMatrixAA_GL) {
  RunBlendingWithRenderPass(
      PIXEL_TEST_GL, FILE_PATH_LITERAL("blending_render_pass_mask.png"),
      kUseMasks | kUseAntialiasing | kUseColorMatrix | kForceShaders);
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
