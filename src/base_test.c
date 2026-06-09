//
// Created by Rob Ross on 2/21/26.
//


#include <stdio.h>
#include "base.h"

void test_pv(void) {
    bool bool_v                                             = 42;
    PVL(bool_v);
    bool* bool_ptr                                          = &bool_v;
    PVL(bool_ptr);
    bool const* bool_const_ptr                              = &bool_v;
    PVL(bool_const_ptr);
    char char_v                                             = 42;
    PVL(char_v);
    char* char_ptr                                          = &char_v;
    PVL(char_ptr);
    char const* char_const_ptr                              = &char_v;
    PVL(char_const_ptr);
    unsigned char unsigned_char_v                           = 42;
    PVL(unsigned_char_v);
    unsigned char* unsigned_char_ptr                        = &unsigned_char_v;
    PVL(unsigned_char_ptr);
    unsigned char const* unsigned_char_const_ptr            = &unsigned_char_v;
    PVL(unsigned_char_const_ptr);
    signed char signed_char_v                               = 42;
    PVL(signed_char_v);
    signed char* signed_char_ptr                            = &signed_char_v;
    PVL(signed_char_ptr);
    signed char const* signed_char_const_ptr                = &signed_char_v;
    PVL(signed_char_const_ptr);
    short short_v                                           = 42;
    PVL(short_v);
    short* short_ptr                                        = &short_v;
    PVL(short_ptr);
    short const* short_const_ptr                            = &short_v;
    PVL(short_const_ptr);
    unsigned short unsigned_short_v                         = 42;
    PVL(unsigned_short_v);
    unsigned short* unsigned_short_ptr                      = &unsigned_short_v;
    PVL(unsigned_short_ptr);
    unsigned short const* unsigned_short_const_ptr          = &unsigned_short_v;
    PVL(unsigned_short_const_ptr);
    int int_v                                               = 42;
    PVL(int_v);
    int* int_ptr                                            = &int_v;
    PVL(int_ptr);
    int const* int_const_ptr                                = &int_v;
    PVL(int_const_ptr);
    unsigned int unsigned_int_v                             = 42;
    PVL(unsigned_int_v);
    unsigned int* unsigned_int_ptr                          = &unsigned_int_v;
    PVL(unsigned_int_ptr);
    unsigned int const* unsigned_int_const_ptr              = &unsigned_int_v;
    PVL(unsigned_int_const_ptr);
    long long_v                                             = 42;
    PVL(long_v);
    long* long_ptr                                          = &long_v;
    PVL(long_ptr);
    long const* long_const_ptr                              = &long_v;
    PVL(long_const_ptr);
    unsigned long unsigned_long_v                           = 42;
    PVL(unsigned_long_v);
    unsigned long* unsigned_long_ptr                        = &unsigned_long_v;
    PVL(unsigned_long_ptr);
    unsigned long const* unsigned_long_const_ptr            = &unsigned_long_v;
    PVL(unsigned_long_const_ptr);
    long long long_long_v                                   = 42;
    PVL(long_long_v);
    long long* long_long_ptr                                = &long_long_v;
    PVL(long_long_ptr);
    long long const* long_long_const_ptr                    = &long_long_v;
    PVL(long_long_const_ptr);
    unsigned long long unsigned_long_long_v                 = 42;
    PVL(unsigned_long_long_v);
    unsigned long long* unsigned_long_long_ptr              = &unsigned_long_long_v;
    PVL(unsigned_long_long_ptr);
    unsigned long long const* unsigned_long_long_const_ptr  = &unsigned_long_long_v;
    PVL(unsigned_long_long_const_ptr);
    float float_v                                           = 42;
    PVL(float_v);
    float* float_ptr                                        = &float_v;
    PVL(float_ptr);
    float const* float_const_ptr                            = &float_v;
    PVL(float_const_ptr);
    double double_v                                         = 42;
    PVL(double_v);
    double* double_ptr                                      = &double_v;
    PVL(double_ptr);
    double const* double_const_ptr                          = &double_v;
    PVL(double_const_ptr);
    long double long_double_v                               = 42;
    PVL(long_double_v);
    long double* long_double_ptr                            = &long_double_v;
    PVL(long_double_ptr);
    long double const* long_double_const_ptr                = &long_double_v;
    PVL(long_double_const_ptr);
    void* void_ptr                                          = &"foo";
    PVL(void_ptr);
    void const* void_const_ptr                              = &"foo";
    PVL(void_const_ptr);
}

void test_arrays(void) {
    int array[10];
    MEM_ZERO_ARRAY(array);
    PVL(array);
    PVL(sizeof(array));
}

void test_structs(void) {
    struct s {
        int x;
        int y;
    };
    struct s foo_s = { };
    PVL(sizeof(foo_s));
    PVL(&foo_s);
    PVL(foo_s.x);
}
//make:
// clang -std=c23 -o base_test.out base_test.c
int main(void) {
    // test_pv();

    char *str = "Now is the time for all good men to order a pizza from Pizza Hut!";
    PS(str);

    test_arrays();
    test_structs();

    return 0;
}
