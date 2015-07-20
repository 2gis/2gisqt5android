// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_ashmem.h"
#include "base/memory/discardable_memory_ashmem_allocator.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_shmem.h"
#include "base/sys_info.h"

namespace base {
namespace {

const char kAshmemAllocatorName[] = "DiscardableMemoryAshmemAllocator";

// For Ashmem, have the DiscardableMemoryManager trigger userspace eviction
// when address space usage gets too high (e.g. 512 MBytes).
const size_t kAshmemMemoryLimit = 512 * 1024 * 1024;

size_t GetOptimalAshmemRegionSizeForAllocator() {
  // Note that this may do some I/O (without hitting the disk though) so it
  // should not be called on the critical path.
  return base::SysInfo::AmountOfPhysicalMemory() / 8;
}

// Holds the shared state used for allocations.
struct SharedState {
  SharedState()
      : manager(kAshmemMemoryLimit, kAshmemMemoryLimit, TimeDelta::Max()),
        allocator(kAshmemAllocatorName,
                  GetOptimalAshmemRegionSizeForAllocator()) {}

  internal::DiscardableMemoryManager manager;
  internal::DiscardableMemoryAshmemAllocator allocator;
};
LazyInstance<SharedState>::Leaky g_shared_state = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void DiscardableMemory::ReleaseFreeMemory() {
  internal::DiscardableMemoryShmem::ReleaseFreeMemory();
}

// static
bool DiscardableMemory::ReduceMemoryUsage() {
  return internal::DiscardableMemoryEmulated::ReduceMemoryUsage();
}

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_ASHMEM,
    DISCARDABLE_MEMORY_TYPE_EMULATED,
    DISCARDABLE_MEMORY_TYPE_SHMEM
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_ASHMEM: {
      SharedState* const shared_state = g_shared_state.Pointer();
      scoped_ptr<internal::DiscardableMemoryAshmem> memory(
          new internal::DiscardableMemoryAshmem(
              size, &shared_state->allocator, &shared_state->manager));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_EMULATED: {
      scoped_ptr<internal::DiscardableMemoryEmulated> memory(
          new internal::DiscardableMemoryEmulated(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_SHMEM: {
      scoped_ptr<internal::DiscardableMemoryShmem> memory(
          new internal::DiscardableMemoryShmem(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_NONE:
    case DISCARDABLE_MEMORY_TYPE_MACH:
      NOTREACHED();
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

// static
void DiscardableMemory::PurgeForTesting() {
  g_shared_state.Pointer()->manager.PurgeAll();
  internal::DiscardableMemoryEmulated::PurgeForTesting();
  internal::DiscardableMemoryShmem::PurgeForTesting();
}

}  // namespace base
