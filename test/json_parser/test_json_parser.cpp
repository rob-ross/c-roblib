//  test_json_parser.cpp
// Created by Rob Ross on 7/1/26.
//


#include <gtest/gtest.h>

// Wrap your C header so the C++ compiler understands it
extern "C" {
#include "roblib/json_parser.h"
}

using str_param = std::tuple<std::string, std::string> ;


class JsonParserTest : public testing::TestWithParam<str_param>{
protected:
    JsonError err{};

    static void SetUpTestSuite() {
        jsonp_init();
        arena = new Arena();
        ArenaErrResult aer = arena_create_arena( arena, 1024 * 1024);
        ASSERT_EQ(aer.err, 0) << "arena_create_arena failed with " << aer.reported_err << ", "<< aer.msg;
    }

    // Per-test-suite tear-down.
    // Called after the last test in this test suite.
    static void TearDownTestSuite() {
        arena_destroy_arena(arena);
        delete arena;
        jsonp_destroy();
    }
    // Some expensive resource shared by all tests.
    inline static Arena *arena = nullptr;
};

TEST_F(JsonParserTest, TestNullText) {
    JsonValue *jval = json_parse(nullptr, &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestEmptyText) {
    JsonValue *jval = json_parse("", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_F(JsonParserTest, TestLiterals) {
    JsonValue *jval;

    jval = json_parse("null", &err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = json_parse(" null ", &err, arena);
    EXPECT_EQ(jval->type, JSON_NULL) << "Expected JsonType = JSON_NULL";
    jval = json_parse("nul", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = json_parse("nulll", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
    jval = json_parse("nullington", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";

    jval = json_parse("true", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = json_parse(" true ", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);
    jval = json_parse(" true false", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_TRUE(jval->u.boolean);

    jval = json_parse("false", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);
    jval = json_parse(" false ", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);
    jval = json_parse(" false true", &err, arena);
    EXPECT_EQ(jval->type, JSON_BOOLEAN) << "Expected JsonType = JSON_BOOLEAN";
    EXPECT_FALSE(jval->u.boolean);
    jval = json_parse("falsee [\"list\"]", &err, arena);
    EXPECT_EQ(jval, nullptr) << "expected nullptr";
}

TEST_P(JsonParserTest, TestStrings) {
    auto [input_json, expected_output] = GetParam(); // Structured binding
    JsonValue *jval = json_parse(input_json.c_str(), &err, arena);

    ASSERT_NE(jval, nullptr) << "Failed to parse: " << input_json;
    EXPECT_EQ(jval->type, JSON_STRING);
    if (jval->type == JSON_STRING) {
        EXPECT_STREQ(jval->u.string, expected_output.c_str());
    }
}

INSTANTIATE_TEST_SUITE_P(
    StringTests,
    JsonParserTest,
    testing::Values(
        str_param("\"\"", ""),
        str_param("\"string\"", "string"),
        str_param(" \"This is a json string followed by a comma\", ",
                    "This is a json string followed by a comma")
    )
);
