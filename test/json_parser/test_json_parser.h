// test_json_parser.h

#pragma once

#include <gtest/gtest.h>
#include <string>
#include <tuple>

struct json_parse_error_s;
typedef json_parse_error_s JsonParseError;

// Forward declaration because we only use Arena* (Incomplete Type is fine)
// This reduces coupling and improves compile times.
struct arena_s;
typedef arena_s Arena;

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
    void SetUp() override;
    void TearDown() override;

    // Now a pointer to allow forward declaration.
    // Managed in SetUp/TearDown in the .cpp file.
    JsonParseError *err = nullptr;

    // Pointers to the global environment arena
    Arena* arena = JsonParserEnvironment::arena;
};
