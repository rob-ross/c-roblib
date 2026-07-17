#ifndef TEST_JSON_PARSER_H
#define TEST_JSON_PARSER_H

#include <gtest/gtest.h>
#include <string>
#include <tuple>

// Wrap C library headers
extern "C" {
#include "roblib/arena.h" // Assuming this is where Arena is defined
#include "roblib/json_parser.h"
}

using str_param = std::tuple<std::string, std::string>;

class JsonParserEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;

    // The single source of truth for the arena
    inline static Arena *arena;
};

class JsonParserTest : public testing::Test {
protected:
    JsonParseError err{};
    Arena* arena = JsonParserEnvironment::arena;
};

#endif // TEST_JSON_PARSER_H
