//  test_json_parser.cpp
// Created by Rob Ross on 7/1/26.
//


#include <gtest/gtest.h>
#include "test_json_parser.h"

// Wrap your C header so the C++ compiler understands it
extern "C" {
#include "roblib/json_parser.h"
}


void JsonParserEnvironment::SetUp() {
    jsonp_init();
    arena = new Arena();
    arena_create_arena(arena, 1024 * 1024);
}


void JsonParserEnvironment::TearDown() {
    arena_destroy_arena(arena);
    delete arena;
    jsonp_destroy();
}

TEST_F(JsonParserTest, TestNullText) {
    JsonValue *jval = jsonp_parse(nullptr, &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestEmptyText) {
    JsonValue *jval = jsonp_parse("", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestLiterals) {
    JsonValue *jval;

    jval = jsonp_parse("null", &err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = jsonp_parse(" null ", &err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = jsonp_parse("nul", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = jsonp_parse("nulll", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = jsonp_parse("nullington", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";

    jval = jsonp_parse("true", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = jsonp_parse(" true ", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = jsonp_parse(" true false", &err, arena);  // should return null and an error
    EXPECT_EQ(jval, nullptr) << " 'true false' is invalid JSON";
    // EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    // EXPECT_TRUE(jval->u.boolean);

    jval = jsonp_parse("false", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);
    jval = jsonp_parse(" false ", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);

    jval = jsonp_parse(" false true", &err, arena);
    EXPECT_EQ(jval, nullptr) << " 'false true' is invalid JSON";

    // EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    // EXPECT_FALSE(jval->u.boolean);

    jval = jsonp_parse("falsee [\"list\"]", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestArrayTrailingCommas) {
    char const * test_fixture = "[\"\",]";
    JsonValue *jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    EXPECT_EQ(err.err_type, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED);

    bool flag_was_set = jsonp_is_flag_set(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
    jsonp_set_config_flag(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
    jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: " << test_fixture;
    EXPECT_EQ(err.err_type, 0) << "expected no error for: " << test_fixture;

    //restore flag
    if (!flag_was_set)  jsonp_clear_config_flag(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_ARRAYS);
}

TEST_F(JsonParserTest, TestObjectTrailingCommas) {
    char const * test_fixture = "{\"foo\" : 1, }";
    JsonValue *jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    EXPECT_EQ(err.err_type, JSON_ERR_TRAILING_COMMA_NOT_ALLOWED);


    bool flag_was_set = jsonp_is_flag_set(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);
    jsonp_set_config_flag(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);
    jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: " << test_fixture;
    EXPECT_EQ(err.err_type, JSON_ERR_NONE) << "expected no error for: " << test_fixture;

    //restore flag
    if (!flag_was_set)  jsonp_clear_config_flag(JSON_CONFIG_ALLOW_TRAILING_COMMAS_IN_OBJECTS);
}

TEST_F(JsonParserTest, n_multidigit_number_then_00_json) {
    char const * test_fixture = "123\x0";
    JsonValue *jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_NE(jval, nullptr) << "expected successful parse with jsonp_parse";

    jval = jsonp_parse_ex(test_fixture, &err, arena, 4);
    EXPECT_EQ(jval, nullptr) << "expected fail to parse";
    EXPECT_EQ(err.err_type, JSON_ERR_UNEXPECTED_EOF);
    EXPECT_EQ(err.parse_end, 3);
}

TEST_F(JsonParserTest, n_structure_whitespace_formfeed_json) {
    // form feed not "whitespace" per the JSON spec.
    char const * test_fixture = "[\x0c]";  // literal Form feed character
    JsonValue *jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected fail to parse with embedded formfeed";
    EXPECT_EQ(err.err_type, JSON_ERR_UNEXPECTED_TEXT);
    EXPECT_EQ(err.parse_end, 1);

    // todo (rob) this exposes an issue with this API call. In a multi-threaded environment, the next call to
    // jsonp_parse will pick up the new whitespace definition. Perhaps we provide a global definition only at
    // init time in jsonp_int and this cannot change. But then we allow per-context customization.
    // so we'd need a create_context type method the user can use to set flags and options before calling
    // jsonp_parse_with_context....
    jsonp_define_whitespace_chars(" \t\n\r\f");  // add formfeed

    jval = jsonp_parse(test_fixture, &err, arena);
    EXPECT_NE(jval, nullptr) << "expected successful parse for: '" << test_fixture \
        << "' after adding form-feed as white space character, but parsing failed. err_type = " << err.err_type;

    jsonp_define_whitespace_chars(JSON_WHITESPACE_CHARS_DEFAULT);  // restore original
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
    JsonValue *jval = jsonp_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_STREQ(jval->u.string, expected_output.c_str());
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
    JsonValue *jval = jsonp_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_STREQ(jval->u.string, expected_output.c_str());
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
            " backslash-uFEF00  \xEF\xBB\xB0" "0 one extra hex char, valid. " ),
        str_param( " \" backslash-01F600  😀  \\U01F600 \" ", " backslash-01F600  😀  😀 " )
    )
);


TEST_P(JsonParserUnicodeStrings, TestStrings) {
    auto [input_json, expected_output] = GetParam(); // Structured binding
    JsonValue *jval = jsonp_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_STREQ(jval->u.string, expected_output.c_str());
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
    JsonValue *jval = jsonp_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json << err.message;
    EXPECT_EQ(jval->type, JSON_INT);
    if (jval->type == JSON_INT) {
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
    JsonValue *jval = jsonp_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_FLOAT);
    if (jval->type == JSON_FLOAT) {
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
