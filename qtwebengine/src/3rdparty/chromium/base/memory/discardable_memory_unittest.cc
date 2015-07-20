// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include <limits>
#endif

namespace base {
namespace {

class DiscardableMemoryTest
    : public testing::TestWithParam<DiscardableMemoryType> {
 public:
  DiscardableMemoryTest() {}
  virtual ~DiscardableMemoryTest() {
  }

 protected:
  scoped_ptr<DiscardableMemory> CreateLockedMemory(size_t size) {
    return DiscardableMemory::CreateLockedMemoryWithType(
        GetParam(), size).Pass();
  }
};

const size_t kSize = 1024;

TEST_P(DiscardableMemoryTest, IsNamed) {
  std::string type_name(DiscardableMemory::GetTypeName(GetParam()));
  EXPECT_NE("unknown", type_name);
  EXPECT_EQ(GetParam(), DiscardableMemory::GetNamedType(type_name));
}

bool IsNativeType(DiscardableMemoryType type) {
  return type == DISCARDABLE_MEMORY_TYPE_ASHMEM ||
         type == DISCARDABLE_MEMORY_TYPE_MACH;
}

TEST_P(DiscardableMemoryTest, SupportedNatively) {
  std::vector<DiscardableMemoryType> supported_types;
  DiscardableMemory::GetSupportedTypes(&supported_types);
#if defined(DISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY)
  EXPECT_NE(0, std::count_if(supported_types.begin(),
                             supported_types.end(),
                             IsNativeType));
#else
  // If we ever have a platform that decides at runtime if it can support
  // discardable memory natively, then we'll have to add a 'never supported
  // natively' define for this case. At present, if it's not always supported
  // natively, it's never supported.
  EXPECT_EQ(0, std::count_if(supported_types.begin(),
                             supported_types.end(),
                             IsNativeType));
#endif
}

// Test Lock() and Unlock() functionalities.
TEST_P(DiscardableMemoryTest, LockAndUnLock) {
  const scoped_ptr<DiscardableMemory> memory(CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  void* addr = memory->Memory();
  ASSERT_NE(nullptr, addr);

  memory->Unlock();

  EXPECT_NE(DISCARDABLE_MEMORY_LOCK_STATUS_FAILED, memory->Lock());
  addr = memory->Memory();
  ASSERT_NE(nullptr, addr);

  memory->Unlock();
}

// Test delete a discardable memory while it is locked.
TEST_P(DiscardableMemoryTest, DeleteWhileLocked) {
  const scoped_ptr<DiscardableMemory> memory(CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
}

// Test forced purging.
TEST_P(DiscardableMemoryTest, Purge) {
  const scoped_ptr<DiscardableMemory> memory(CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  memory->Unlock();

  DiscardableMemory::PurgeForTesting();
  EXPECT_EQ(DISCARDABLE_MEMORY_LOCK_STATUS_PURGED, memory->Lock());
}

#if !defined(NDEBUG) && !defined(OS_ANDROID)
// Death tests are not supported with Android APKs.
TEST_P(DiscardableMemoryTest, UnlockedMemoryAccessCrashesInDebugMode) {
  const scoped_ptr<DiscardableMemory> memory(CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  memory->Unlock();
  ASSERT_DEATH_IF_SUPPORTED(
      { *static_cast<int*>(memory->Memory()) = 0xdeadbeef; }, ".*");
}
#endif

// Test behavior when creating enough instances that could use up a 32-bit
// address space.
TEST_P(DiscardableMemoryTest, AddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;  // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  scoped_ptr<DiscardableMemory> instances[kNumberOfInstances];
  for (auto& memory : instances) {
    memory = CreateLockedMemory(kLargeSize);
    ASSERT_TRUE(memory);
    void* addr = memory->Memory();
    ASSERT_NE(nullptr, addr);
    memory->Unlock();
  }
}

std::vector<DiscardableMemoryType> GetSupportedDiscardableMemoryTypes() {
  std::vector<DiscardableMemoryType> supported_types;
  DiscardableMemory::GetSupportedTypes(&supported_types);
  return supported_types;
}

INSTANTIATE_TEST_CASE_P(
    DiscardableMemoryTests,
    DiscardableMemoryTest,
    ::testing::ValuesIn(GetSupportedDiscardableMemoryTypes()));

}  // namespace
}  // namespace base
