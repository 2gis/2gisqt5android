// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include "cc/animation/timing_function.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(ScrollOffsetAnimationCurveTest, Duration) {
  gfx::ScrollOffset target_value(100.f, 200.f);
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value,
          EaseInOutTimingFunction::Create().Pass()));

  curve->SetInitialValue(target_value);
  EXPECT_DOUBLE_EQ(0.0, curve->Duration());

  // x decreases, y stays the same.
  curve->SetInitialValue(gfx::ScrollOffset(136.f, 200.f));
  EXPECT_DOUBLE_EQ(0.1, curve->Duration());

  // x increases, y stays the same.
  curve->SetInitialValue(gfx::ScrollOffset(19.f, 200.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration());

  // x stays the same, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(100.f, 344.f));
  EXPECT_DOUBLE_EQ(0.2, curve->Duration());

  // x stays the same, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(100.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration());

  // x decreases, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(32500.f, 500.f));
  EXPECT_DOUBLE_EQ(3.0, curve->Duration());

  // x decreases, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(150.f, 119.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration());

  // x increases, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(0.f, 14600.f));
  EXPECT_DOUBLE_EQ(2.0, curve->Duration());

  // x increases, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(95.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration());
}

TEST(ScrollOffsetAnimationCurveTest, GetValue) {
  gfx::ScrollOffset initial_value(2.f, 40.f);
  gfx::ScrollOffset target_value(10.f, 20.f);
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value,
          EaseInOutTimingFunction::Create().Pass()));
  curve->SetInitialValue(initial_value);

  double duration = curve->Duration();
  EXPECT_GT(curve->Duration(), 0);
  EXPECT_LT(curve->Duration(), 0.1);

  EXPECT_EQ(AnimationCurve::ScrollOffset, curve->Type());
  EXPECT_EQ(duration, curve->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value, curve->GetValue(-1.0));
  EXPECT_VECTOR2DF_EQ(initial_value, curve->GetValue(0.0));
  EXPECT_VECTOR2DF_EQ(gfx::ScrollOffset(6.f, 30.f),
                      curve->GetValue(duration/2.0));
  EXPECT_VECTOR2DF_EQ(target_value, curve->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(target_value, curve->GetValue(duration+1.0));

  // Verify that GetValue takes the timing function into account.
  gfx::ScrollOffset value = curve->GetValue(duration/4.0);
  EXPECT_NEAR(3.0333f, value.x(), 0.00015f);
  EXPECT_NEAR(37.4168f, value.y(), 0.00015f);
}

// Verify that a clone behaves exactly like the original.
TEST(ScrollOffsetAnimationCurveTest, Clone) {
  gfx::ScrollOffset initial_value(2.f, 40.f);
  gfx::ScrollOffset target_value(10.f, 20.f);
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value,
          EaseInOutTimingFunction::Create().Pass()));
  curve->SetInitialValue(initial_value);
  double duration = curve->Duration();

  scoped_ptr<AnimationCurve> clone(curve->Clone().Pass());

  EXPECT_EQ(AnimationCurve::ScrollOffset, clone->Type());
  EXPECT_EQ(duration, clone->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(-1.0));
  EXPECT_VECTOR2DF_EQ(initial_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(0.0));
  EXPECT_VECTOR2DF_EQ(
      gfx::ScrollOffset(6.f, 30.f),
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration / 2.0));
  EXPECT_VECTOR2DF_EQ(
      target_value,
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(
      target_value,
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration + 1.0));

  // Verify that the timing function was cloned correctly.
  gfx::ScrollOffset value =
      clone->ToScrollOffsetAnimationCurve()->GetValue(duration / 4.0);
  EXPECT_NEAR(3.0333f, value.x(), 0.00015f);
  EXPECT_NEAR(37.4168f, value.y(), 0.00015f);
}

TEST(ScrollOffsetAnimationCurveTest, UpdateTarget) {
  gfx::ScrollOffset initial_value(0.f, 0.f);
  gfx::ScrollOffset target_value(0.f, 3600.f);
  scoped_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, EaseInOutTimingFunction::Create().Pass()));
  curve->SetInitialValue(initial_value);
  EXPECT_EQ(1.0, curve->Duration());
  EXPECT_EQ(1800.0, curve->GetValue(0.5).y());
  EXPECT_EQ(3600.0, curve->GetValue(1.0).y());

  curve->UpdateTarget(0.5, gfx::ScrollOffset(0.0, 9900.0));

  EXPECT_EQ(2.0, curve->Duration());
  EXPECT_EQ(1800.0, curve->GetValue(0.5).y());
  EXPECT_NEAR(5566.49, curve->GetValue(1.0).y(), 0.01);
  EXPECT_EQ(9900.0, curve->GetValue(2.0).y());

  curve->UpdateTarget(1.0, gfx::ScrollOffset(0.0, 7200.0));

  EXPECT_NEAR(1.674, curve->Duration(), 0.01);
  EXPECT_NEAR(5566.49, curve->GetValue(1.0).y(), 0.01);
  EXPECT_EQ(7200.0, curve->GetValue(1.674).y());
}

}  // namespace
}  // namespace cc
