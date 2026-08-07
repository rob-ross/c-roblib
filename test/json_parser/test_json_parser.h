// test_json_parser.h

#pragma once

// test data note: you can download Greeked JSON files over http from https://jsonplaceholder.typicode.com

#include <gtest/gtest.h>
#include <string>
#include <tuple>

struct json_parse_error_s;
typedef json_parse_error_s JsonParseError;

// Forward declaration because we only use AlokArena* (Incomplete Type is fine)
// This reduces coupling and improves compile times.
struct alok_arena_s;
typedef alok_arena_s AlokArena;

using str_param = std::tuple<std::string, std::string>;

class JsonParserEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;

    // The single source of truth for the arena
    inline static AlokArena *arena;
};

class JsonParserTest : public testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Now a pointer to allow forward declaration.
    // Managed in SetUp/TearDown in the .cpp file.
    JsonParseError *err = nullptr;

    // Pointers to the global environment arena
    AlokArena* arena = JsonParserEnvironment::arena;
};
