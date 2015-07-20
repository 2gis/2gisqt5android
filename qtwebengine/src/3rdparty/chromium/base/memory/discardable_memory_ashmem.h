// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_H_

#include "base/memory/discardable_memory.h"

#include "base/macros.h"
#include "base/memory/discardable_memory_manager.h"

namespace base {
namespace internal {

class DiscardableAshmemChunk;
class DiscardableMemoryAshmemAllocator;
class DiscardableMemoryManager;

class DiscardableMemoryAshmem
    : public DiscardableMemory,
      public internal::DiscardableMemoryManagerAllocation {
 public:
  explicit DiscardableMemoryAshmem(size_t bytes,
                                   DiscardableMemoryAshmemAllocator* allocator,
                                   DiscardableMemoryManager* manager);

  virtual ~DiscardableMemoryAshmem();

  bool Initialize();

  // Overridden from DiscardableMemory:
  virtual DiscardableMemoryLockStatus Lock() override;
  virtual void Unlock() override;
  virtual void* Memory() const override;

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  virtual bool AllocateAndAcquireLock() override;
  virtual void ReleaseLock() override;
  virtual void Purge() override;
  virtual bool IsMemoryResident() const override;

 private:
  const size_t bytes_;
  DiscardableMemoryAshmemAllocator* const allocator_;
  DiscardableMemoryManager* const manager_;
  bool is_locked_;
  scoped_ptr<DiscardableAshmemChunk> ashmem_chunk_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryAshmem);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_H_
