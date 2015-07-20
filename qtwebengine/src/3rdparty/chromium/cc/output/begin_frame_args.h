// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_BEGIN_FRAME_ARGS_H_
#define CC_OUTPUT_BEGIN_FRAME_ARGS_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/cc_export.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {

struct CC_EXPORT BeginFrameArgs {
  enum BeginFrameArgsType {
    INVALID,
    NORMAL,
    SYNCHRONOUS,
    MISSED,
  };

  // Creates an invalid set of values.
  BeginFrameArgs();

  // You should be able to find all instances where a BeginFrame has been
  // created by searching for "BeginFrameArgs::Create".
  static BeginFrameArgs Create(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval);
  static BeginFrameArgs CreateTyped(base::TimeTicks frame_time,
                                    base::TimeTicks deadline,
                                    base::TimeDelta interval,
                                    BeginFrameArgsType type);
  static BeginFrameArgs CreateForSynchronousCompositor(
      base::TimeTicks now = base::TimeTicks());

  // This is the default delta that will be used to adjust the deadline when
  // proper draw-time estimations are not yet available.
  static base::TimeDelta DefaultEstimatedParentDrawTime();

  // This is the default interval to use to avoid sprinkling the code with
  // magic numbers.
  static base::TimeDelta DefaultInterval();

  // This is the default amount of time after the frame_time to retroactively
  // send a BeginFrame that had been skipped. This only has an effect if the
  // deadline has passed, since the deadline is also used to trigger BeginFrame
  // retroactively.
  static base::TimeDelta DefaultRetroactiveBeginFramePeriod();

  bool IsValid() const { return interval >= base::TimeDelta(); }

  scoped_refptr<base::debug::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::debug::TracedValue* dict) const;

  base::TimeTicks frame_time;
  base::TimeTicks deadline;
  base::TimeDelta interval;
  BeginFrameArgsType type;

 private:
  BeginFrameArgs(base::TimeTicks frame_time,
                 base::TimeTicks deadline,
                 base::TimeDelta interval,
                 BeginFrameArgsType type);
};

}  // namespace cc

#endif  // CC_OUTPUT_BEGIN_FRAME_ARGS_H_
