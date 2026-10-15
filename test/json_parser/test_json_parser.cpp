//  test_json_parser.cpp
// Created by Rob Ross on 7/1/26.
//


#include <gtest/gtest.h>
#include "test_json_parser.h"

#include "roblib/json_parser.h"

// SLIT: slice literal convenience macro
#define SLIT(S) slice_from_cstring((S))
#define NL putchar('\n');

void JsonParserEnvironment::SetUp() {
    jsonp_init();
    // Using {} (List Initialization) to ensure the C struct is completely
    // zero-initialized before passing it to the C API.
    ArenaErrResult aer = alok_arena_create( 1024 * 1024);
    if (aer.err ) {
        fprintf(stderr, "Could not allocate AlokArena");
        // ugly cast but C++ doesn't play well with my Error framework
        Error *e = static_cast<Error *>(static_cast<void *>(&aer));
        err_print(*e);
    }
    arena =  aer.result;
}


void JsonParserEnvironment::TearDown() {
    alok_arena_destroy(arena);
    jsonp_destroy();
}

void JsonParserTest::SetUp() {
    // Fresh allocation for every test case. {} ensures all fields
    // (especially the enum and buffers) start at zero.
    err = new JsonParseError{};
    // todo (rob) for future optimization - we should reset the AlokArena here so it starts from the beginning
    alok_arena_reset(arena, false);
    // for each test. Thus the memory allocated for the AlokArena will attain a "high water mark",
    // but won't grow without bound.
}

void JsonParserTest::TearDown() {
    // Clean up the memory after the test finishes
    delete err;
    err = nullptr;
}

TEST_F(JsonParserTest, TestNullText) {
    JsonValue *jval = jsonp_parse_string(nullptr, err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestEmptyText) {
    JsonValue *jval = jsonp_parse_string("", err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestLiterals) {
    JsonValue *jval;

    jval = jsonp_parse_string("null", err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = jsonp_parse_string(" null ", err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = jsonp_parse_string("nul", err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = jsonp_parse_string("nulll", err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = jsonp_parse_string("nullington", err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";

    jval = jsonp_parse_string("true", err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = jsonp_parse_string(" true ", err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = jsonp_parse_string(" true false", err, arena);  // should return null and an error
    EXPECT_EQ(jval, nullptr) << " 'true false' is invalid JSON";
    // EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    // EXPECT_TRUE(jval->u.boolean);

    jval = jsonp_parse_string("false", err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);
    jval = jsonp_parse_string(" false ", err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);

    jval = jsonp_parse_string(" false true", err, arena);
    EXPECT_EQ(jval, nullptr) << " 'false true' is invalid JSON";

    // EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    // EXPECT_FALSE(jval->u.boolean);

    jval = jsonp_parse_string("falsee [\"list\"]", err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestArrayTrailingCommas) {
    char const * test_fixture = "[\"\",]";
    JsonValue *jval = jsonp_parse_string(test_fixture, err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    EXPECT_EQ(err->err_type, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED);

    JsonContext *context = jsonp_copy_global_context();

    bool flag_was_set = jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
    jsonp_set_context_config_flag(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
    jval = jsonp_parse_string_using_context(test_fixture, err, arena, context);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: " << test_fixture;
    EXPECT_EQ(err->err_type, 0) << "expected no error for: " << test_fixture;

    //restore flag
    if (!flag_was_set)  jsonp_clear_context_config_flag(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
    free(context);
}

TEST_F(JsonParserTest, TestObjectTrailingCommas) {
    char const * test_fixture = "{\"foo\" : 1, }";
    JsonValue *jval = jsonp_parse_string(test_fixture, err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    EXPECT_EQ(err->err_type, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED);

    JsonContext *context = jsonp_copy_global_context();

    bool flag_was_set = jsonp_is_context_config_flag_set(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);
    jsonp_set_context_config_flag(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);
    jval = jsonp_parse_string_using_context(test_fixture, err, arena, context);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: " << test_fixture;
    EXPECT_EQ(err->err_type, JSON_ERR_NONE) << "expected no error for: " << test_fixture;
    if (err->err_type) jsonp_print_parse_error(err);

    //restore flag
    if (!flag_was_set)  jsonp_clear_context_config_flag(context, JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);

    free(context);
}

TEST_F(JsonParserTest, n_multidigit_number_then_00_json) {
    char const * test_fixture = "123\x0";
    JsonValue *jval = jsonp_parse_string(test_fixture, err, arena);
    EXPECT_NE(jval, nullptr) << "expected successful parse with jsonp_parse";

    jval = jsonp_parse_string_ex(test_fixture, err, arena, 4);
    EXPECT_EQ(jval, nullptr) << "expected fail to parse";
    EXPECT_EQ(err->err_type, JSON_ERR_EXPECTED_EOF);
    // jsonp_print_parse_error(err);
    EXPECT_EQ(err->parse_end, 3);
}

TEST_F(JsonParserTest, n_structure_whitespace_formfeed_json) {
    // form feed not "whitespace" per the JSON spec.
    char const * test_fixture = "[\x0c]";  // literal Form feed character
    JsonValue *jval = jsonp_parse_string(test_fixture, err, arena);
    EXPECT_EQ(jval, nullptr) << "expected fail to parse with embedded formfeed";
    EXPECT_EQ(err->err_type, JSON_ERR_MISSING_ARRAY_ELEMENT);
    EXPECT_EQ(err->parse_end, 1);

    JsonContext *context = jsonp_copy_global_context();

    jsonp_set_context_whitespace_chars(context, " \t\n\r\f");  // add form-feed

    jval = jsonp_parse_string_using_context(test_fixture, err, arena, context);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: '" << test_fixture \
        << "' after adding form-feed as white space character, but parsing failed. err_type = " << err->err_type;
    if (!jval) {
        jsonp_print_parse_error(err);
    }

    jsonp_set_context_whitespace_chars(context, JSON_WHITESPACE_CHARS_DEFAULT);  // restore original

    free(context);
}


// -----------------------------------------------------------------
//      PARAMETERIZED TESTS
// -----------------------------------------------------------------

template <typename T>
class JsonParserParamFixture : public JsonParserTest, public ::testing::WithParamInterface<T> {};

using JsonParserStrings = JsonParserParamFixture<std::pair<std::string, std::string>>;
using JsonParserStringEscapes = JsonParserParamFixture<std::pair<std::string, std::string>>;
using JsonParserUnicodeStrings = JsonParserParamFixture<std::pair<std::string, std::string>>;


using JsonParserInts  = JsonParserParamFixture<std::pair<std::string, long>>;
using JsonParserFloats  = JsonParserParamFixture<std::pair<std::string, double>>;



TEST_P(JsonParserStrings, TestStrings) {
    auto [input_json, expected_output] = GetParam(); // Structured binding
    JsonValue *jval = jsonp_parse_string(input_json.c_str(), err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_TRUE(slice_equal(jval->u.string, SLIT(expected_output.c_str())));
    }
}

INSTANTIATE_TEST_SUITE_P(
    StringTests,
    JsonParserStrings,
    testing::Values(
        str_param("\"\"", ""),
        str_param("\"string\"", "string"),
        str_param(" \"This is a longer json string followed by a comma,\" ",
                    "This is a longer json string followed by a comma,")
    )
);

TEST_P(JsonParserStringEscapes, TestStringEscapes) {
    auto [input_json, expected_output] = GetParam(); // Structured binding
    JsonValue *jval = jsonp_parse_string(input_json.c_str(), err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json << " " << err->message;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_TRUE(slice_equal(jval->u.string, SLIT(expected_output.c_str())));
        // printf("actual: "); slice_print(jval->u.string);
        // printf(", expected: "); slice_print(SLIT(expected_output.c_str())); NL
    }
}

// To run a parameterized test against a unique list of parameter values, you must create a unique
// class to pass to each TEST_P/ INSTANTIATE_TEST_SUITE_P macro pair. If you share the class with
// different pairs, both testing patters will be run with the same parameter values.

INSTANTIATE_TEST_SUITE_P(
    StringEscapesTests,
    JsonParserStringEscapes,
    testing::Values(
        str_param(" \"Esc-backslash: s\\\\e  \" ", "Esc-backslash: s\\e  " ),
        str_param(" \"Esc-quote:     s\\\"e  \" ", "Esc-quote:     s\"e  " ),
        str_param(" \"Esc-Slash:     s\\/e  \" ", "Esc-Slash:     s/e  " ),
        str_param(" \"Esc-b:          s\\be  \" ", "Esc-b:          s\be  " ),
        str_param(" \"Esc-f:          s\\fe  \" ", "Esc-f:          s\fe  " ),
        str_param(" \"Esc-n:          s\\ne  \" ", "Esc-n:          s\ne  " ),
        str_param(" \"Esc-r:          s\\re  \" ", "Esc-r:          s\re  " ),
        str_param(" \"Esc-t:          s\\te  \" ", "Esc-t:          s\te  " ),

        str_param("\"Let's test ALL the single char escapes: \\\\ \\\" \\/ \\b \\f \\n \\r \\t\"",
            "Let's test ALL the single char escapes: \\ \" / \b \f \n \r \t"),
        // str_param(" \" backslash           \\ no character \" ", "" ),
        str_param( " \" escaped backslash \\\\ valid \" ", " escaped backslash \\ valid " ),
        // str_param( " \" backslash-z \\z not valid escape character \" ", "" ),
        str_param( " \" backslash-n \\n valid escape character \" ", " backslash-n \n valid escape character " ),
        // str_param( " \" backslash-u  \\u  invalid  \" ", "" ),
        // str_param( " \" backslash-uk \\uk invalid  \" ", "" ),
        // str_param( " \" backslash-ua  \\ua  need 4 hex digits\" ", "" ),
        str_param( " \" backslash-uabcd  \\uabcd valid \" ", " backslash-uabcd  \xEA\xAF\x8D valid " ),
        str_param( " \" backslash-uFEF0  \\uFEF0 valid \" ", " backslash-uFEF0  \xEF\xBB\xB0 valid " ),
        str_param( " \" backslash-uFEF00  \\uFEF00 one extra hex char, valid. \" ",
            " backslash-uFEF00  \xEF\xBB\xB0" "0 one extra hex char, valid. " )
        // str_param( " \" backslash-01F600  😀  \\U01F600 \" ", " backslash-01F600  😀  😀 " )
    )
);


TEST_P(JsonParserUnicodeStrings, TestStrings) {
    auto [input_json, expected_output] = GetParam(); // Structured binding
    JsonValue *jval = jsonp_parse_string(input_json.c_str(), err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_TRUE(slice_equal(jval->u.string, SLIT(expected_output.c_str())));
    }
}

INSTANTIATE_TEST_SUITE_P(
    StringUnicodeTests,
    JsonParserUnicodeStrings,
    testing::Values(
        str_param("\" unicode chars:  é, 😀\"", " unicode chars:  é, 😀")
    )
);

// --- Parameterized Test for ints ---

TEST_P(JsonParserInts, TestDoubles) {
    auto [input_json, expected_value] = GetParam();
    JsonValue *jval = jsonp_parse_string(input_json.c_str(), err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json << err->message;
    EXPECT_EQ(jval->type, JSON_LONG);
    if (jval->type == JSON_LONG) {
        EXPECT_EQ(jval->u.n_long, expected_value);
    }
}

INSTANTIATE_TEST_SUITE_P(
    NumberTests,
    JsonParserInts,
    testing::Values(
        std::make_pair("0", 0),
        std::make_pair("-0", 0),
        std::make_pair("1", 1),
        std::make_pair("-2", -2),
        std::make_pair("4", 4),
        std::make_pair("-4", -4),
        std::make_pair("44", 44),
        std::make_pair("4444", 4444),
        std::make_pair("-55555", -55555)
    )
);

// --- Parameterized Test for floats ---

TEST_P(JsonParserFloats, TestDoubles) {
    auto [input_json, expected_value] = GetParam();
    JsonValue *jval = jsonp_parse_string(input_json.c_str(), err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_DOUBLE);
    if (jval->type == JSON_DOUBLE) {
        EXPECT_DOUBLE_EQ(jval->u.n_double, expected_value);
    }
}

INSTANTIATE_TEST_SUITE_P(
    NumberTests,
    JsonParserFloats,
    testing::Values(
        // floats
        std::make_pair("1.2345", 1.2345),
        std::make_pair("3.333", 3.333),
        std::make_pair("-122.3959", -122.3959),
        std::make_pair("9.99e10", 9.99e10),
        std::make_pair("-8.88E-8", -8.88E-8),
        //  std::make_pair(("-abc", err), // should fail
        std::make_pair("0.0001", 0.0001),
        std::make_pair("-42.0", -42.0)
    )
);
