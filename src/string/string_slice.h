//  string_slice.h
//
//  Created by Rob Ross on 8/1/26.
//
//  Copyright (c) 2026.  All rights reserved.

//

#ifndef C_ROBLIB_STRING_SLICE_H
#define C_ROBLIB_STRING_SLICE_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct string_slice_s {
    char const * data;
    size_t length;
} StringSlice;



#ifdef __cplusplus
}
#endif
#endif //C_ROBLIB_STRING_SLICE_H
