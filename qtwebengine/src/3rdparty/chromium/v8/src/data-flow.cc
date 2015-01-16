// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/data-flow.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
void BitVector::Print() {
  bool first = true;
  PrintF("{");
  for (int i = 0; i < length(); i++) {
    if (Contains(i)) {
      if (!first) PrintF(",");
      first = false;
      PrintF("%d", i);
    }
  }
  PrintF("}");
}
#endif


void BitVector::Iterator::Advance() {
  current_++;
  uint32_t val = current_value_;
  while (val == 0) {
    current_index_++;
    if (Done()) return;
    val = target_->data_[current_index_];
    current_ = current_index_ << 5;
  }
  val = SkipZeroBytes(val);
  val = SkipZeroBits(val);
  current_value_ = val >> 1;
}

} }  // namespace v8::internal
