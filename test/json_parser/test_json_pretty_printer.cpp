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



TEST(JsonPrettyPrinter,  jsonp_print) {
    JsonFormatFlags flags1 = { .indent = 2, .single_line = false,  };
    JsonFormatFlags flags2 = { .indent = 2, .single_line = true };

    JsonValue null_value = { .type = JSON_NULL, .u.n_long = 0L } ;
    jsonp_print(&null_value, flags1 ); NL

    JsonValue true_value = { .type = JSON_BOOLEAN, .u.boolean = true};
    jsonp_print(&true_value, flags1 ); NL

    JsonValue false_value = { .type = JSON_BOOLEAN, .u.boolean = false};
    jsonp_print(&false_value, flags1 ); NL

    JsonValue number_value = { .type = JSON_NUMBER, .u.n_double = 1.0e10};
    jsonp_print(&number_value, flags1 ); NL

    JsonValue double_value = { .type = JSON_DOUBLE, .u.n_double = -1.0123e10};
    jsonp_print(&double_value, flags1 ); NL

    // JsonValue long_double_value = { .type = JSON_LONG_DOUBLE, .u.n_long_double = -0.123456789012345678901234567890L};
    // jsonp_print(long_double_value); NL

    JsonValue long_value = { .type = JSON_LONG, .u.n_long = 258};
    jsonp_print(&long_value, flags1 ); NL

    // JsonValue long_long_value = { .type = JSON_LONG_LONG, .u.n_long_long = 258L};
    // jsonp_print(long_long_value); NL

    //string here
    JsonValue string_value = { .type = JSON_STRING, .u.string = SLIT("This is a test string")};
    jsonp_print(&string_value, flags1 ); NL

    string_value = { .type = JSON_STRING, .u.string = SLIT("This \x1F \x09is a \x5C \x22test\x22 strin\x01g")};
    jsonp_print(&string_value, flags1 ); NL

    printf("sizeof(JsonValue): %zu, _Alignof(JsonValue): %zu\n", sizeof(JsonValue), alignof(JsonValue));

}
