// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
TEST(GUIDTest, GUIDGeneratesAllZeroes) {
  uint64 bytes[] = { 0, 0 };
  std::string clientid = base::RandomDataToGUIDString(bytes);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}

TEST(GUIDTest, GUIDGeneratesCorrectly) {
  uint64 bytes[] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
  std::string clientid = base::RandomDataToGUIDString(bytes);
  EXPECT_EQ("01234567-89AB-CDEF-FEDC-BA9876543210", clientid);
}
#endif

TEST(GUIDTest, GUIDCorrectlyFormatted) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid = base::GenerateGUID();
    EXPECT_TRUE(base::IsValidGUID(guid));
  }
}

TEST(GUIDTest, GUIDBasicUniqueness) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid1 = base::GenerateGUID();
    std::string guid2 = base::GenerateGUID();
    EXPECT_EQ(36U, guid1.length());
    EXPECT_EQ(36U, guid2.length());
    EXPECT_NE(guid1, guid2);
  }
}
