//
// Created by Rob Ross on 2/21/26.
//
// ... existing code ...
#pragma once

// This file is intended to be used as a *shim* for <assert.h>.
// Put its directory first on the include path and name it "assert.h".

// This file redefines the assert macro. It is intended to be used during execution of unit tests.
// This allows testing to ensure that an assert fails when the constraint under test is violated.

#if defined(PUNIT_TEST_ASSERTS)
  #include "unit.h"

  // Pull in the real system assert first.
  #include_next <assert.h>

  // Override assert *after* the real header.
  #undef assert
  #define assert(expr)                                                     \
  do {                                                                     \
      if (!(expr)) {                                                       \
        punit_errorf("assertion failed: %s", #expr);                     \
      }                                                                    \
  } while (0)
#else
  // Normal behavior when not testing
  #include_next <assert.h>
#endif
// ... existing code ...