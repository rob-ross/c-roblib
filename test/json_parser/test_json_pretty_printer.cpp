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
    // todo (rob) currently this "test" just prints a JSON value to the console.
    // we need to capture the JsonValue and expected output as a test parameter and compare the console output (actual)
    // to the expected console output. Since we know all print/repr methods are implemented by `jsonp_print_impl`,
    // we really only need to store a string output result (expected) and compare to the output from `jsonp_sprint`,
    // and that will test all such repr methods.

    JsonFormatFlags flags1 = { .indent = 2, .single_line = false,  };
    JsonFormatFlags flags2 = { .indent = 2, .single_line = true };

    JsonValue null_value = { .u = { .n_long = 0L}, .type = JSON_NULL,  } ;
    jsonp_print(&null_value, flags1 ); NL

    JsonValue true_value = { .u = {.boolean = true}, .type = JSON_BOOLEAN };
    jsonp_print(&true_value, flags1 ); NL

    JsonValue false_value = { .u = {.boolean = false}, .type = JSON_BOOLEAN };
    jsonp_print(&false_value, flags1 ); NL

    JsonValue number_value = { .u = {.n_double = 1.0e10}, .type = JSON_NUMBER };
    jsonp_print(&number_value, flags1 ); NL

    JsonValue double_value = { .u = {.n_double = -1.0123e10}, .type = JSON_DOUBLE };
    jsonp_print(&double_value, flags1 ); NL

    // JsonValue long_double_value = { .type = JSON_LONG_DOUBLE, .u.n_long_double = -0.123456789012345678901234567890L};
    // jsonp_print(long_double_value); NL

    JsonValue long_value = {  .u = {.n_long = 258}, .type = JSON_LONG };
    jsonp_print(&long_value, flags1 ); NL

    // JsonValue long_long_value = { .type = JSON_LONG_LONG, .u.n_long_long = 258L};
    // jsonp_print(long_long_value); NL

    //string here
    JsonValue string_value = { .u = {.string = SLIT("This is a test string")}, .type = JSON_STRING };
    jsonp_print(&string_value, flags1 ); NL

    string_value = {
        .u = {.string = SLIT("This \x1F \x09is a \x5C \x22test\x22 strin\x01g")}, .type = JSON_STRING,
    };
    jsonp_print(&string_value, flags1 ); NL

    printf("sizeof(JsonValue): %zu, _Alignof(JsonValue): %zu\n", sizeof(JsonValue), alignof(JsonValue));

}
