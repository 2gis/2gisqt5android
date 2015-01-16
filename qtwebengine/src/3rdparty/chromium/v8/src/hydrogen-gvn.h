// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_GVN_H_
#define V8_HYDROGEN_GVN_H_

#include "src/hydrogen.h"
#include "src/hydrogen-instructions.h"
#include "src/compiler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// This class extends GVNFlagSet with additional "special" dynamic side effects,
// which can be used to represent side effects that cannot be expressed using
// the GVNFlags of an HInstruction. These special side effects are tracked by a
// SideEffectsTracker (see below).
class SideEffects V8_FINAL {
 public:
  static const int kNumberOfSpecials = 64 - kNumberOfFlags;

  SideEffects() : bits_(0) {
    ASSERT(kNumberOfFlags + kNumberOfSpecials == sizeof(bits_) * CHAR_BIT);
  }
  explicit SideEffects(GVNFlagSet flags) : bits_(flags.ToIntegral()) {}
  bool IsEmpty() const { return bits_ == 0; }
  bool ContainsFlag(GVNFlag flag) const {
    return (bits_ & MaskFlag(flag)) != 0;
  }
  bool ContainsSpecial(int special) const {
    return (bits_ & MaskSpecial(special)) != 0;
  }
  bool ContainsAnyOf(SideEffects set) const { return (bits_ & set.bits_) != 0; }
  void Add(SideEffects set) { bits_ |= set.bits_; }
  void AddSpecial(int special) { bits_ |= MaskSpecial(special); }
  void RemoveFlag(GVNFlag flag) { bits_ &= ~MaskFlag(flag); }
  void RemoveAll() { bits_ = 0; }
  uint64_t ToIntegral() const { return bits_; }
  void PrintTo(StringStream* stream) const;

 private:
  uint64_t MaskFlag(GVNFlag flag) const {
    return static_cast<uint64_t>(1) << static_cast<unsigned>(flag);
  }
  uint64_t MaskSpecial(int special) const {
    ASSERT(special >= 0);
    ASSERT(special < kNumberOfSpecials);
    return static_cast<uint64_t>(1) << static_cast<unsigned>(
        special + kNumberOfFlags);
  }

  uint64_t bits_;
};


// Tracks global variable and inobject field loads/stores in a fine grained
// fashion, and represents them using the "special" dynamic side effects of the
// SideEffects class (see above). This way unrelated global variable/inobject
// field stores don't prevent hoisting and merging of global variable/inobject
// field loads.
class SideEffectsTracker V8_FINAL BASE_EMBEDDED {
 public:
  SideEffectsTracker() : num_global_vars_(0), num_inobject_fields_(0) {}
  SideEffects ComputeChanges(HInstruction* instr);
  SideEffects ComputeDependsOn(HInstruction* instr);
  void PrintSideEffectsTo(StringStream* stream, SideEffects side_effects) const;

 private:
  bool ComputeGlobalVar(Unique<Cell> cell, int* index);
  bool ComputeInobjectField(HObjectAccess access, int* index);

  static int GlobalVar(int index) {
    ASSERT(index >= 0);
    ASSERT(index < kNumberOfGlobalVars);
    return index;
  }
  static int InobjectField(int index) {
    ASSERT(index >= 0);
    ASSERT(index < kNumberOfInobjectFields);
    return index + kNumberOfGlobalVars;
  }

  // Track up to four global vars.
  static const int kNumberOfGlobalVars = 4;
  Unique<Cell> global_vars_[kNumberOfGlobalVars];
  int num_global_vars_;

  // Track up to n inobject fields.
  static const int kNumberOfInobjectFields =
      SideEffects::kNumberOfSpecials - kNumberOfGlobalVars;
  HObjectAccess inobject_fields_[kNumberOfInobjectFields];
  int num_inobject_fields_;
};


// Perform common subexpression elimination and loop-invariant code motion.
class HGlobalValueNumberingPhase V8_FINAL : public HPhase {
 public:
  explicit HGlobalValueNumberingPhase(HGraph* graph);

  void Run();

 private:
  SideEffects CollectSideEffectsOnPathsToDominatedBlock(
      HBasicBlock* dominator,
      HBasicBlock* dominated);
  void AnalyzeGraph();
  void ComputeBlockSideEffects();
  void LoopInvariantCodeMotion();
  void ProcessLoopBlock(HBasicBlock* block,
                        HBasicBlock* before_loop,
                        SideEffects loop_kills);
  bool AllowCodeMotion();
  bool ShouldMove(HInstruction* instr, HBasicBlock* loop_header);

  SideEffectsTracker side_effects_tracker_;
  bool removed_side_effects_;

  // A map of block IDs to their side effects.
  ZoneList<SideEffects> block_side_effects_;

  // A map of loop header block IDs to their loop's side effects.
  ZoneList<SideEffects> loop_side_effects_;

  // Used when collecting side effects on paths from dominator to
  // dominated.
  BitVector visited_on_paths_;

  DISALLOW_COPY_AND_ASSIGN(HGlobalValueNumberingPhase);
};

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_GVN_H_
