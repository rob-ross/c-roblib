//  test_json_parser.cpp
// Created by Rob Ross on 7/1/26.
//


#include <gtest/gtest.h>

// Wrap your C header so the C++ compiler understands it
extern "C" {
#include "roblib/json_parser.h"
}

class JsonParserTest : public testing::TestWithParam<std::string_view>{
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

TEST_F(JsonParserTest, TestStrings) {

}
