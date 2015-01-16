// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHECKS_H_
#define V8_CHECKS_H_

#include <string.h>

#include "include/v8stdint.h"
#include "src/base/build_config.h"

extern "C" void V8_Fatal(const char* file, int line, const char* format, ...);


// The FATAL, UNREACHABLE and UNIMPLEMENTED macros are useful during
// development, but they should not be relied on in the final product.
#ifdef DEBUG
#define FATAL(msg)                              \
  V8_Fatal(__FILE__, __LINE__, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal(__FILE__, __LINE__, "unimplemented code")
#define UNREACHABLE()                           \
  V8_Fatal(__FILE__, __LINE__, "unreachable code")
#else
#define FATAL(msg)                              \
  V8_Fatal("", 0, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal("", 0, "unimplemented code")
#define UNREACHABLE() ((void) 0)
#endif

// Simulator specific helpers.
// We can't use USE_SIMULATOR here because it isn't defined yet.
#if V8_TARGET_ARCH_ARM64 && !V8_HOST_ARCH_ARM64
  // TODO(all): If possible automatically prepend an indicator like
  // UNIMPLEMENTED or LOCATION.
  #define ASM_UNIMPLEMENTED(message)                                         \
  __ Debug(message, __LINE__, NO_PARAM)
  #define ASM_UNIMPLEMENTED_BREAK(message)                                   \
  __ Debug(message, __LINE__,                                                \
           FLAG_ignore_asm_unimplemented_break ? NO_PARAM : BREAK)
  #define ASM_LOCATION(message)                                              \
  __ Debug("LOCATION: " message, __LINE__, NO_PARAM)
#else
  #define ASM_UNIMPLEMENTED(message)
  #define ASM_UNIMPLEMENTED_BREAK(message)
  #define ASM_LOCATION(message)
#endif


// The CHECK macro checks that the given condition is true; if not, it
// prints a message to stderr and aborts.
#define CHECK(condition) do {                                       \
    if (!(condition)) {                                             \
      V8_Fatal(__FILE__, __LINE__, "CHECK(%s) failed", #condition); \
    }                                                               \
  } while (0)


// Helper function used by the CHECK_EQ function when given int
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file, int line,
                              const char* expected_source, int expected,
                              const char* value_source, int value) {
  if (expected != value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %i\n#   Found: %i",
             expected_source, value_source, expected, value);
  }
}


// Helper function used by the CHECK_EQ function when given int64_t
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file, int line,
                              const char* expected_source,
                              int64_t expected,
                              const char* value_source,
                              int64_t value) {
  if (expected != value) {
    // Print int64_t values in hex, as two int32s,
    // to avoid platform-dependencies.
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#"
             "   Expected: 0x%08x%08x\n#   Found: 0x%08x%08x",
             expected_source, value_source,
             static_cast<uint32_t>(expected >> 32),
             static_cast<uint32_t>(expected),
             static_cast<uint32_t>(value >> 32),
             static_cast<uint32_t>(value));
  }
}


// Helper function used by the CHECK_NE function when given int
// arguments.  Should not be called directly.
inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* unexpected_source,
                                 int unexpected,
                                 const char* value_source,
                                 int value) {
  if (unexpected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %i",
             unexpected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given string
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              const char* expected,
                              const char* value_source,
                              const char* value) {
  if ((expected == NULL && value != NULL) ||
      (expected != NULL && value == NULL) ||
      (expected != NULL && value != NULL && strcmp(expected, value) != 0)) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %s\n#   Found: %s",
             expected_source, value_source, expected, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* expected_source,
                                 const char* expected,
                                 const char* value_source,
                                 const char* value) {
  if (expected == value ||
      (expected != NULL && value != NULL && strcmp(expected, value) == 0)) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %s",
             expected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given pointer
// arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              const void* expected,
                              const char* value_source,
                              const void* value) {
  if (expected != value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %p\n#   Found: %p",
             expected_source, value_source,
             expected, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* expected_source,
                                 const void* expected,
                                 const char* value_source,
                                 const void* value) {
  if (expected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %p",
             expected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given floating
// point arguments.  Should not be called directly.
inline void CheckEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              double expected,
                              const char* value_source,
                              double value) {
  // Force values to 64 bit memory to truncate 80 bit precision on IA32.
  volatile double* exp = new double[1];
  *exp = expected;
  volatile double* val = new double[1];
  *val = value;
  if (*exp != *val) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %f\n#   Found: %f",
             expected_source, value_source, *exp, *val);
  }
  delete[] exp;
  delete[] val;
}


inline void CheckNonEqualsHelper(const char* file,
                              int line,
                              const char* expected_source,
                              int64_t expected,
                              const char* value_source,
                              int64_t value) {
  if (expected == value) {
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %f\n#   Found: %f",
             expected_source, value_source, expected, value);
  }
}


inline void CheckNonEqualsHelper(const char* file,
                                 int line,
                                 const char* expected_source,
                                 double expected,
                                 const char* value_source,
                                 double value) {
  // Force values to 64 bit memory to truncate 80 bit precision on IA32.
  volatile double* exp = new double[1];
  *exp = expected;
  volatile double* val = new double[1];
  *val = value;
  if (*exp == *val) {
    V8_Fatal(file, line,
             "CHECK_NE(%s, %s) failed\n#   Value: %f",
             expected_source, value_source, *val);
  }
  delete[] exp;
  delete[] val;
}


#define CHECK_EQ(expected, value) CheckEqualsHelper(__FILE__, __LINE__, \
  #expected, expected, #value, value)


#define CHECK_NE(unexpected, value) CheckNonEqualsHelper(__FILE__, __LINE__, \
  #unexpected, unexpected, #value, value)


#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))


#ifdef DEBUG
#ifndef OPTIMIZED_DEBUG
#define ENABLE_SLOW_ASSERTS    1
#endif
#endif

namespace v8 {
namespace internal {
#ifdef ENABLE_SLOW_ASSERTS
#define SLOW_ASSERT(condition) \
  CHECK(!v8::internal::FLAG_enable_slow_asserts || (condition))
extern bool FLAG_enable_slow_asserts;
#else
#define SLOW_ASSERT(condition) ((void) 0)
const bool FLAG_enable_slow_asserts = false;
#endif

// Exposed for making debugging easier (to see where your function is being
// called, just add a call to DumpBacktrace).
void DumpBacktrace();

} }  // namespace v8::internal


// The ASSERT macro is equivalent to CHECK except that it only
// generates code in debug builds.
#ifdef DEBUG
#define ASSERT_RESULT(expr)    CHECK(expr)
#define ASSERT(condition)      CHECK(condition)
#define ASSERT_EQ(v1, v2)      CHECK_EQ(v1, v2)
#define ASSERT_NE(v1, v2)      CHECK_NE(v1, v2)
#define ASSERT_GE(v1, v2)      CHECK_GE(v1, v2)
#define ASSERT_LT(v1, v2)      CHECK_LT(v1, v2)
#define ASSERT_LE(v1, v2)      CHECK_LE(v1, v2)
#else
#define ASSERT_RESULT(expr)    (expr)
#define ASSERT(condition)      ((void) 0)
#define ASSERT_EQ(v1, v2)      ((void) 0)
#define ASSERT_NE(v1, v2)      ((void) 0)
#define ASSERT_GE(v1, v2)      ((void) 0)
#define ASSERT_LT(v1, v2)      ((void) 0)
#define ASSERT_LE(v1, v2)      ((void) 0)
#endif

#define ASSERT_NOT_NULL(p)  ASSERT_NE(NULL, p)

// "Extra checks" are lightweight checks that are enabled in some release
// builds.
#ifdef ENABLE_EXTRA_CHECKS
#define EXTRA_CHECK(condition) CHECK(condition)
#else
#define EXTRA_CHECK(condition) ((void) 0)
#endif

#endif  // V8_CHECKS_H_
