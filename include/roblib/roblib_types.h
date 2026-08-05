//  roblib_types.h
//
// Created by Rob Ross on 6/30/26.
//


#pragma once

#ifndef C_ROBLIB_ROBLIB_TYPES_H
#define C_ROBLIB_ROBLIB_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef int8_t    s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef unsigned char byte;
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

typedef long double f80;
typedef long double f128;

// -----------------------------------------------------------------
//      Limits
// -----------------------------------------------------------------

// 8 bit
constexpr s8 S8_MIN = (s8)0x80;
constexpr s8 S8_MAX = (s8)0x7F;

constexpr u8 U8_MIN = (u8)0x00;
constexpr u8 U8_MAX = (u8)0xFF;

// 16 bit
constexpr s16 S16_MIN = (s16)0x8000;
constexpr s16 S16_MAX = (s16)0x7FFF;

constexpr u16 U16_MIN = (u16)0x0000;
constexpr u16 U16_MAX = (u16)0xFFFF;

// 32 bit
constexpr s32 S32_MIN = (s32)0x80000000;
constexpr s32 S32_MAX = (s32)0x7FFFFFFF;

constexpr u32 U32_MIN = (u32)0x00000000;
constexpr u32 U32_MAX = (u32)0xFFFFFFFF;

// 64 bit
constexpr s64 S64_MIN = (s64)0x8000000000000000llu;
constexpr s64 S64_MAX = (s64)0x7FFFFFFFFFFFFFFFllu;

constexpr u64 U64_MIN = (u64)0x0000000000000000llu;
constexpr u64 U64_MAX = (u64)0xFFFFFFFFFFFFFFFFllu;

// 32 bit float
constexpr f32 F32_MIN_POS_SUBNORMAL = 1.401'298'464'3e-45f;
constexpr f32 F32_MAX_SUBNORMAL     = 1.175'494'210'7e-38f;
constexpr f32 F32_MIN_POS_NORMAL    = 1.175'494'350'8e-38f;
constexpr f32 F32_MAX_NORMAL        = 3.402'823'466'4e38f;

// 64 bit float
constexpr f64 F64_MIN_POS_SUBNORMAL = 4.940'656'458'412'465'4e-324;
constexpr f64 F64_MAX_SUBNORMAL     = 2.225'073'858'507'200'9e-308;
constexpr f64 F64_MIN_POS_NORMAL    = 2.225'073'858'507'201'4e-308;
constexpr f64 F64_MAX_NORMAL        = 1.797'693'134'862'315'7e308;

// 80 bit float
constexpr f80 F80_MIN_POS_SUBNORMAL = 3.645'199'531'882'474'602'528'41e-4951L;
constexpr f80 F80_MAX_SUBNORMAL     = 3.362'103'143'112'093'505'898'16e-4932L;
constexpr f80 F80_MIN_POS_NORMAL    = 3.362'103'143'112'093'506'262'68e-4932L;
constexpr f80 F80_MAX_NORMAL        = 1.189'731'495'357'231'765'021'26e4932L;

#if (1)
// 128-bit float - for future use
constexpr f128 F128_MIN_POS_SUBNORMAL = 6.475'175'119'438'025'110'924'438'958'227'646'552'5e-4951L;
// constexpr f128 F128_MIN_POS_SUBNORMAL_ = 6.475'175'119'438'025'110'924'438'958'227'646'552'5e-4966L;
constexpr f128 F128_MAX_SUBNORMAL     = 3.362'103'143'112'093'506'262'677'817'321'751'955'1e-4932L;
constexpr f128 F128_MIN_POS_NORMAL    = 3.362'103'143'112'093'506'262'677'817'321'752'602'6e-4932L;
constexpr f128 F128_MAX_NORMAL        = 1.189'731'495'357'231'765'021'26e4932L;
// constexpr f128 F128_MAX_NORMAL_        = 1.189'731'495'357'231'765'085'759'326'628'007'016'2e4932L;
#endif


#ifdef __cplusplus
}
#endif

#endif //C_ROBLIB_ROBLIB_TYPES_H
