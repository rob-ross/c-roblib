//
// Created by Rob Ross on 7/1/26.
//

#include <gtest/gtest.h>

// GTest seems to use actual, expected in comparisons

// Wrap your C header so the C++ compiler understands it
extern "C" {
#include "roblib/string_utils.h"
}

// A simple sanity check test
TEST(GoogleTestSanity, HelloWorld) {
    EXPECT_EQ(1 + 1, 2);
}

// An actual test calling a function inside your libc_roblib.a
TEST(CLibraryTest, CoreFunctionCheck) {
    // Replace 'your_c_function()' with a real function from your library
    char const c = sutil_char_at("this is the Best!", 12);
    EXPECT_EQ(c, 'B');
}
