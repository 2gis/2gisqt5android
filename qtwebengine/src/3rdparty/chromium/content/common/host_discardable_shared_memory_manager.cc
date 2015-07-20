// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/host_discardable_shared_memory_manager.h"

#include <algorithm>

#include "base/callback.h"
#include "base/debug/crash_logging.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"

namespace content {
namespace {

base::LazyInstance<HostDiscardableSharedMemoryManager>
    g_discardable_shared_memory_manager = LAZY_INSTANCE_INITIALIZER;

const size_t kDefaultMemoryLimit = 512 * 1024 * 1024;

const int kEnforceMemoryPolicyDelayMs = 1000;

}  // namespace

HostDiscardableSharedMemoryManager::MemorySegment::MemorySegment(
    linked_ptr<base::DiscardableSharedMemory> memory,
    base::ProcessHandle process_handle)
    : memory(memory), process_handle(process_handle) {
}

HostDiscardableSharedMemoryManager::MemorySegment::~MemorySegment() {
}

HostDiscardableSharedMemoryManager::HostDiscardableSharedMemoryManager()
    : memory_limit_(kDefaultMemoryLimit),
      bytes_allocated_(0),
      memory_pressure_listener_(new base::MemoryPressureListener(
          base::Bind(&HostDiscardableSharedMemoryManager::OnMemoryPressure,
                     base::Unretained(this)))),
      enforce_memory_policy_pending_(false),
      weak_ptr_factory_(this) {
}

HostDiscardableSharedMemoryManager::~HostDiscardableSharedMemoryManager() {
}

HostDiscardableSharedMemoryManager*
HostDiscardableSharedMemoryManager::current() {
  return g_discardable_shared_memory_manager.Pointer();
}

scoped_ptr<base::DiscardableSharedMemory>
HostDiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    size_t size) {
  // TODO(reveman): Need to implement this for discardable memory support in
  // the browser process.
  NOTIMPLEMENTED();
  return scoped_ptr<base::DiscardableSharedMemory>();
}

void HostDiscardableSharedMemoryManager::
    AllocateLockedDiscardableSharedMemoryForChild(
        base::ProcessHandle process_handle,
        size_t size,
        base::SharedMemoryHandle* shared_memory_handle) {
  base::AutoLock lock(lock_);

  // Memory usage must be reduced to prevent the addition of |size| from
  // taking usage above the limit. Usage should be reduced to 0 in cases
  // where |size| is greater than the limit.
  size_t limit = 0;
  // Note: the actual mapped size can be larger than requested and cause
  // |bytes_allocated_| to temporarily be larger than |memory_limit_|. The
  // error is minimized by incrementing |bytes_allocated_| with the actual
  // mapped size rather than |size| below.
  if (size < memory_limit_)
    limit = memory_limit_ - size;

  if (bytes_allocated_ > limit)
    ReduceMemoryUsageUntilWithinLimit(limit);

  linked_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  if (!memory->CreateAndMap(size)) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  if (!memory->ShareToProcess(process_handle, shared_memory_handle)) {
    LOG(ERROR) << "Cannot share discardable memory segment";
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  base::CheckedNumeric<size_t> checked_bytes_allocated = bytes_allocated_;
  checked_bytes_allocated += memory->mapped_size();
  if (!checked_bytes_allocated.IsValid()) {
    *shared_memory_handle = base::SharedMemory::NULLHandle();
    return;
  }

  bytes_allocated_ = checked_bytes_allocated.ValueOrDie();
  BytesAllocatedChanged(bytes_allocated_);

  segments_.push_back(MemorySegment(memory, process_handle));
  std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);

  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void HostDiscardableSharedMemoryManager::ProcessRemoved(
    base::ProcessHandle process_handle) {
  base::AutoLock lock(lock_);

  size_t bytes_allocated_before_purging = bytes_allocated_;
  for (auto& segment : segments_) {
    // Skip segments that belong to a different process.
    if (segment.process_handle != process_handle)
      continue;

    size_t size = segment.memory->mapped_size();
    DCHECK_GE(bytes_allocated_, size);

    // This will unmap the memory segment and drop our reference. The result
    // is that the memory will be released to the OS if the child process is
    // no longer referencing it.
    // Note: We intentionally leave the segment in the vector to avoid
    // reconstructing the heap. The element will be removed from the heap
    // when its last usage time is older than all other segments.
    segment.memory->Close();
    bytes_allocated_ -= size;
  }

  if (bytes_allocated_ != bytes_allocated_before_purging)
    BytesAllocatedChanged(bytes_allocated_);
}

void HostDiscardableSharedMemoryManager::SetMemoryLimit(size_t limit) {
  base::AutoLock lock(lock_);

  memory_limit_ = limit;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

void HostDiscardableSharedMemoryManager::EnforceMemoryPolicy() {
  base::AutoLock lock(lock_);

  enforce_memory_policy_pending_ = false;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

void HostDiscardableSharedMemoryManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(lock_);

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Purge everything possible when pressure is critical.
      ReduceMemoryUsageUntilWithinLimit(0);
      break;
  }
}

void
HostDiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinMemoryLimit() {
  if (bytes_allocated_ <= memory_limit_)
    return;

  ReduceMemoryUsageUntilWithinLimit(memory_limit_);
  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void HostDiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinLimit(
    size_t limit) {
  TRACE_EVENT1("renderer_host",
               "HostDiscardableSharedMemoryManager::"
               "ReduceMemoryUsageUntilWithinLimit",
               "bytes_allocated",
               bytes_allocated_);

  // Usage time of currently locked segments are updated to this time and
  // we stop eviction attempts as soon as we come across a segment that we've
  // previously tried to evict but was locked.
  base::Time current_time = Now();

  lock_.AssertAcquired();
  size_t bytes_allocated_before_purging = bytes_allocated_;
  while (!segments_.empty()) {
    if (bytes_allocated_ <= limit)
      break;

    // Stop eviction attempts when the LRU segment is currently in use.
    if (segments_.front().memory->last_known_usage() >= current_time)
      break;

    std::pop_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
    MemorySegment segment = segments_.back();
    segments_.pop_back();

    // Attempt to purge and truncate LRU segment. When successful, as much
    // memory as possible will be released to the OS. How much memory is
    // released depends on the platform. The child process should perform
    // periodic cleanup to ensure that all memory is release within a
    // reasonable amount of time.
    if (segment.memory->PurgeAndTruncate(current_time)) {
      size_t size = segment.memory->mapped_size();
      DCHECK_GE(bytes_allocated_, size);
      bytes_allocated_ -= size;
      continue;
    }

    // Add memory segment (with updated usage timestamp) back on heap after
    // failed attempt to purge it.
    segments_.push_back(segment);
    std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
  }

  if (bytes_allocated_ != bytes_allocated_before_purging)
    BytesAllocatedChanged(bytes_allocated_);
}

void HostDiscardableSharedMemoryManager::BytesAllocatedChanged(
    size_t new_bytes_allocated) const {
  TRACE_COUNTER_ID1(
      "base", "TotalDiscardableMemoryUsage", this, new_bytes_allocated);

  static const char kTotalDiscardableMemoryUsageKey[] = "total-dm-usage";
  base::debug::SetCrashKeyValue(kTotalDiscardableMemoryUsageKey,
                                base::Uint64ToString(new_bytes_allocated));
}

base::Time HostDiscardableSharedMemoryManager::Now() const {
  return base::Time::Now();
}

void HostDiscardableSharedMemoryManager::ScheduleEnforceMemoryPolicy() {
  if (enforce_memory_policy_pending_)
    return;

  enforce_memory_policy_pending_ = true;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HostDiscardableSharedMemoryManager::EnforceMemoryPolicy,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kEnforceMemoryPolicyDelayMs));
}

}  // namespace content
