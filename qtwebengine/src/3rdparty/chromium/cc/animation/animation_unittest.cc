// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using base::TimeDelta;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks::FromInternalValue(seconds *
                                            base::Time::kMicrosecondsPerSecond);
}

scoped_ptr<Animation> CreateAnimation(double iterations,
                                      double duration,
                                      double playback_rate) {
  scoped_ptr<Animation> to_return(
      Animation::Create(make_scoped_ptr(new FakeFloatAnimationCurve(duration)),
                        0,
                        1,
                        Animation::Opacity));
  to_return->set_iterations(iterations);
  to_return->set_playback_rate(playback_rate);
  return to_return.Pass();
}

scoped_ptr<Animation> CreateAnimation(double iterations, double duration) {
  return CreateAnimation(iterations, duration, 1);
}

scoped_ptr<Animation> CreateAnimation(double iterations) {
  return CreateAnimation(iterations, 1, 1);
}

TEST(AnimationTest, TrimTimeZeroIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, TrimTimeOneIteration) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, TrimTimeOneHalfIteration) {
  scoped_ptr<Animation> anim(CreateAnimation(1.5));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.9, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.9)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, TrimTimeInfiniteIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(-1));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
}

TEST(AnimationTest, TrimTimeReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::Reverse);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
}

TEST(AnimationTest, TrimTimeAlternateInfiniteIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
}

TEST(AnimationTest, TrimTimeAlternateOneIteration) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
}

TEST(AnimationTest, TrimTimeAlternateTwoIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(2));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
}

TEST(AnimationTest, TrimTimeAlternateTwoHalfIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(2.5));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.50)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75)));
}

TEST(AnimationTest, TrimTimeAlternateReverseInfiniteIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
}

TEST(AnimationTest, TrimTimeAlternateReverseOneIteration) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
}

TEST(AnimationTest, TrimTimeAlternateReverseTwoIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(2));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
}

TEST(AnimationTest, TrimTimeStartTime) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0)));
}

TEST(AnimationTest, TrimTimeStartTimeReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_start_time(TicksFromSecondsF(4));
  anim->set_direction(Animation::Reverse);
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0)));
}

TEST(AnimationTest, TrimTimeTimeOffset) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(4000));
  anim->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, TrimTimeTimeOffsetReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(4000));
  anim->set_start_time(TicksFromSecondsF(4));
  anim->set_direction(Animation::Reverse);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, TrimTimeNegativeTimeOffset) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-4000));

  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)));
}

TEST(AnimationTest, TrimTimeNegativeTimeOffsetReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-4000));
  anim->set_direction(Animation::Reverse);

  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)));
}

TEST(AnimationTest, TrimTimePauseResume) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  anim->SetRunState(Animation::Paused, TicksFromSecondsF(0.5));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(1024.0));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.5)));
}

TEST(AnimationTest, TrimTimePauseResumeReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Reverse);
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  anim->SetRunState(Animation::Paused, TicksFromSecondsF(0.25));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(1024.0));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.75)));
}

TEST(AnimationTest, TrimTimeSuspendResume) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  anim->Suspend(TicksFromSecondsF(0.5));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  anim->Resume(TicksFromSecondsF(1024));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.5)));
}

TEST(AnimationTest, TrimTimeSuspendResumeReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Reverse);
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  anim->Suspend(TicksFromSecondsF(0.75));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  anim->Resume(TicksFromSecondsF(1024));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.25)));
}

TEST(AnimationTest, TrimTimeZeroDuration) {
  scoped_ptr<Animation> anim(CreateAnimation(0, 0));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, TrimTimeStarting) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 5.0));
  anim->SetRunState(Animation::Starting, TicksFromSecondsF(0.0));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  anim->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  anim->set_start_time(TicksFromSecondsF(1.0));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(3.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, TrimTimeNeedsSynchronizedStartTime) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 5.0));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  anim->set_needs_synchronized_start_time(true);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  anim->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  anim->set_start_time(TicksFromSecondsF(1.0));
  anim->set_needs_synchronized_start_time(false);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(3.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, IsFinishedAtZeroIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(0));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, IsFinishedAtOneIteration) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, IsFinishedAtInfiniteIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(-1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.5)));
}

TEST(AnimationTest, IsFinishedNegativeTimeOffset) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-500));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));

  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.5)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, IsFinishedPositiveTimeOffset) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(500));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));

  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, IsFinishedAtNotRunning) {
  scoped_ptr<Animation> anim(CreateAnimation(0));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::Paused, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::WaitingForTargetAvailability,
                    TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::Finished, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::Aborted, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
}

TEST(AnimationTest, IsFinished) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::Paused, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::WaitingForTargetAvailability,
                    TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::Finished, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
  anim->SetRunState(Animation::Aborted, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
}

TEST(AnimationTest, IsFinishedNeedsSynchronizedStartTime) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::Paused, TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::WaitingForTargetAvailability,
                    TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::Finished, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
  anim->SetRunState(Animation::Aborted, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
}

TEST(AnimationTest, RunStateChangesIgnoredWhileSuspended) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->Suspend(TicksFromSecondsF(0));
  EXPECT_EQ(Animation::Paused, anim->run_state());
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(Animation::Paused, anim->run_state());
  anim->Resume(TicksFromSecondsF(0));
  anim->SetRunState(Animation::Running, TicksFromSecondsF(0.0));
  EXPECT_EQ(Animation::Running, anim->run_state());
}

TEST(AnimationTest, TrimTimePlaybackNormal) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 1, 1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, TrimTimePlaybackSlow) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 1, 0.5));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)));
}

TEST(AnimationTest, TrimTimePlaybackFast) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 4, 2));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, TrimTimePlaybackNormalReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 2, -1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, TrimTimePlaybackSlowReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 2, -0.5));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)));
  EXPECT_EQ(1.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3)));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
}

TEST(AnimationTest, TrimTimePlaybackFastReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 2, -2));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
}

TEST(AnimationTest, TrimTimePlaybackFastInfiniteIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(-1, 4, 4));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.0)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.5)));
}

TEST(AnimationTest, TrimTimePlaybackNormalDoubleReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 1, -1));
  anim->set_direction(Animation::Reverse);
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, TrimTimePlaybackFastDoubleReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 4, -2));
  anim->set_direction(Animation::Reverse);
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFast) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 2, 2));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFastReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 2, 2));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFastDoubleReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 2, -2));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
}

TEST(AnimationTest,
     TrimTimeAlternateReverseThreeIterationsPlaybackFastAlternateReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(3, 2, -2));
  anim->set_direction(Animation::AlternateReverse);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.25)));
}

TEST(AnimationTest,
     TrimTimeAlternateReverseTwoIterationsPlaybackNormalAlternate) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 2, -1));
  anim->set_direction(Animation::Alternate);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)));
}

TEST(AnimationTest, TrimTimeIterationStart) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 1, 1));
  anim->set_iteration_start(0.5);
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, TrimTimeIterationStartAlternate) {
  scoped_ptr<Animation> anim(CreateAnimation(2, 1, 1));
  anim->set_direction(Animation::Alternate);
  anim->set_iteration_start(0.3);
  EXPECT_EQ(0.3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(0.3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.8, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.7)));
  EXPECT_EQ(0.7, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.2)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.7)));
}

TEST(AnimationTest, TrimTimeIterationStartAlternateThreeIterations) {
  scoped_ptr<Animation> anim(CreateAnimation(3, 1, 1));
  anim->set_direction(Animation::Alternate);
  anim->set_iteration_start(1);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5)));
}

TEST(AnimationTest,
     TrimTimeIterationStartAlternateThreeIterationsPlaybackReverse) {
  scoped_ptr<Animation> anim(CreateAnimation(3, 1, -1));
  anim->set_direction(Animation::Alternate);
  anim->set_iteration_start(1);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5)));
}

TEST(AnimationTest, InEffectFillMode) {
  scoped_ptr<Animation> anim(CreateAnimation(1));
  anim->set_fill_mode(Animation::FillModeNone);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeForwards);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeBackwards);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeBoth);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, InEffectFillModePlayback) {
  scoped_ptr<Animation> anim(CreateAnimation(1, 1, -1));
  anim->set_fill_mode(Animation::FillModeNone);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeForwards);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeBackwards);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillModeBoth);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));
}

}  // namespace
}  // namespace cc
