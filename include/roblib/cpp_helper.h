//  cpp_helper.h
// Created by Rob Ross on 6/30/26.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* C99 [static N] ensures the array is non-null and has at least N elements.
   C++ does not support this syntax, so we wrap it. */
#ifdef __cplusplus
#define ROBLIB_STATIC_SIZE(n) n
#else
#define ROB_LIB_STATIC_SIZE(n) static n
#endif


#ifdef __cplusplus
}
#endif
