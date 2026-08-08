//  test_json_pretty_printer.cpp
//
//  Created by Rob Ross on 8/7/26.
//
//  Copyright (c) 2026.  All rights reserved.

#include "gtest/gtest.h"
#include "roblib/json_parser.h"

#include <stddef.h>

// SLIT: slice literal convenience macro
#define SLIT(S) slice_from_cstring((S))
#define NL putchar('\n');



TEST(JsonPrettyPrinter,  jsonp_format_scalar) {
    // JsonValue null_value = { .type = JSON_NULL, .u.n_long_long = 0L } ;
    // jsonp_format_scalar(null_value); NL

    JsonValue true_value = { .type = JSON_BOOLEAN, .u.boolean = true};
    jsonp_format_scalar(true_value); NL

    JsonValue false_value = { .type = JSON_BOOLEAN, .u.boolean = false};
    jsonp_format_scalar(false_value); NL

    JsonValue number_value = { .type = JSON_NUMBER, .u.n_double = 1.0e10};
    jsonp_format_scalar(number_value); NL

    JsonValue double_value = { .type = JSON_DOUBLE, .u.n_double = -1.0123e10};
    jsonp_format_scalar(double_value); NL

    // JsonValue long_double_value = { .type = JSON_LONG_DOUBLE, .u.n_long_double = -0.123456789012345678901234567890L};
    // jsonp_format_scalar(long_double_value); NL

    JsonValue long_value = { .type = JSON_LONG, .u.n_long = 258};
    jsonp_format_scalar(long_value); NL

    // JsonValue long_long_value = { .type = JSON_LONG_LONG, .u.n_long_long = 258L};
    // jsonp_format_scalar(long_long_value); NL

    //string here
    JsonValue string_value = { .type = JSON_STRING, .u.string = SLIT("This is a test string")};
    jsonp_format_scalar(string_value); NL

    string_value = { .type = JSON_STRING, .u.string = SLIT("This \x1F \x09is a \x5C \x22test\x22 strin\x01g")};
    jsonp_format_scalar(string_value); NL

    printf("sizeof(JsonValue): %zu, _Alignof(JsonValue): %zu\n", sizeof(JsonValue), alignof(JsonValue));

}
