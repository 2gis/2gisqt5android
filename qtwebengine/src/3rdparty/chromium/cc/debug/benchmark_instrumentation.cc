// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"

namespace cc {
namespace benchmark_instrumentation {

// Please do not change the trace events in this file without updating
// tools/perf/measurements/rendering_stats.py accordingly.
// The benchmarks search for events and their arguments by name.

void IssueMainThreadRenderingStatsEvent(
    const RenderingStats::MainThreadRenderingStats& stats) {
  TRACE_EVENT_INSTANT1("benchmark",
                       "BenchmarkInstrumentation::MainThreadRenderingStats",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", stats.AsTraceableData());
}

void IssueImplThreadRenderingStatsEvent(
    const RenderingStats::ImplThreadRenderingStats& stats) {
  TRACE_EVENT_INSTANT1("benchmark",
                       "BenchmarkInstrumentation::ImplThreadRenderingStats",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", stats.AsTraceableData());
}

void IssueDisplayRenderingStatsEvent() {
  scoped_refptr<base::debug::TracedValue> record_data =
      new base::debug::TracedValue();
  record_data->SetInteger("frame_count", 1);
  TRACE_EVENT_INSTANT1(
      "benchmark",
      "BenchmarkInstrumentation::DisplayRenderingStats",
      TRACE_EVENT_SCOPE_THREAD,
      "data",
      scoped_refptr<base::debug::ConvertableToTraceFormat>(record_data));
}

}  // namespace benchmark_instrumentation
}  // namespace cc
