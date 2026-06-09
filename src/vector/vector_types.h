//
//  vector_types.h
//
// Created by Rob Ross on 2/23/26.
//

#pragma once

#include "vec_template.h"

#define VEC_T int
#define VEC_NAME VectorInt
#include "vec_template.inc"
#undef VEC_T
#undef VEC_NAME

// #define VEC_T char
// #define VEC_NAME VectorChar
// #include "vec_template.inc"
// #undef VEC_T
// #undef VEC_NAME
//
// #define VEC_T long
// #define VEC_NAME VectorLong
// #include "vec_template.inc"
// #undef VEC_T
// #undef VEC_NAME