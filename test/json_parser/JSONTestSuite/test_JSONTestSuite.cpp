//  test_JSONTestSuite.cpp
//
// Created by Rob Ross on 7/11/26.
//
// Runs JSONTestSuite JSON files against the JSON parser.
// see: https://github.com/rob-ross/JSONTestSuite/tree/master


#include "../test_json_parser.h"


#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <functional>
#include <algorithm>
#include <vector>
#include <filesystem>

/**
 * @brief Holds metadata for a single JSON test file.
 * Used as the parameter type for GTest's parameterized tests.
 */
struct JsonTestParams {
    std::string filename;
    std::string full_path;
    bool should_pass;
};

// Custom name generator for the test runner output
std::string ParamNameGenerator(const testing::TestParamInfo<JsonTestParams>& info) {
    // Prefix with Pass/Fail to distinguish categories and help with the "starts with underscore" constraint
    std::string name = (info.param.should_pass ? "Pass_" : "Fail_") + info.param.filename;

    // Sanitize name: GTest only allows alphanumeric and underscores
    std::replace_if(name.begin(), name.end(), [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');

    // Append the index to guarantee uniqueness even if filenames sanitize to the same string (e.g., .e+ vs .e-)
    return name + "_" + std::to_string(info.index);
}

/**
 * @brief Crawls a directory for .json files and populates the test vector.
 * Uses std::filesystem (C++17) for robust path handling.
 */
void collect_files(const char* path, bool should_pass, std::vector<JsonTestParams>& tests) {
    try {
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) return;

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file()) continue;

            if (entry.path().extension() == ".json") {
                tests.push_back({
                    entry.path().filename().string(),
                    entry.path().string(),
                    should_pass
                });
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        fprintf(stderr, "Filesystem error: %s\n", e.what());
    }
}


/**
 * @brief Entry point for the test suite instantiation.
 * JSONTestSuite_DATA_PATH must be defined via the build system (e.g., CMake).
 */
std::vector<JsonTestParams> GetTestSuiteFiles(const std::string &test_file_path, bool should_pass) {
    std::vector<JsonTestParams> tests;
    collect_files(std::string(JSONTestSuite_DATA_PATH + test_file_path).c_str(), should_pass, tests);
    return tests;
}

/**
 * @brief Fixture for JSON Test Suite.
 * Inherits from JsonParserTest to gain access to the shared arena and error structs.
 * Inherits from WithParamInterface to enable data-driven testing.
 */
class JsonTestSuiteParam : public JsonParserTest, public testing::WithParamInterface<JsonTestParams> {
protected:
    std::string read_file(const std::string& path) {
        // Open in binary mode and move to the end to get the size
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return "";
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string buffer(static_cast<size_t>(size), '\0');
        file.read(&buffer[0], size);

        return buffer;
    }
};

TEST_P(JsonTestSuiteParam, jsonp_parse_string) {
    const JsonTestParams& params = GetParam();
    std::string json_text = read_file(params.full_path);

    // C-string API Limitation: jsonp_parse_string treats \0 as the end of the input.
    // The C-string API cannot distinguish between content-NUL and terminator-NUL.
    // If a JSON file contains an embedded NUL byte, like `n_multidigit_number_then_00.json`,
    // which contains "123\0", then the parser stops early.
    // If the content before the NUL is valid JSON, it produces a false positive success.

    // Since jsonp_parse_string treats \0 as the end of input, it successfully parses "123"
    // resulting in a false positive. We skip this specific file for the string API.
    // Other files with NULs (like '{"a":\0}') still fail correctly and are kept for coverage.
    // Note: jsonp_parse_file handles this file correctly as it is not bound by C-string rules.
    if (params.filename == "n_multidigit_number_then_00.json") {
        GTEST_SKIP() << "Skipping jsonp_parse_string for known C-string false-positive: " << params.filename;
    }

    // Pass the explicit size to the parser so it doesn't stop at embedded nulls
    // JsonValue *jval = json_parse_ex(json_text.c_str(), json_text.size(), &err, arena);

    JsonValue *jval = jsonp_parse_string(json_text.c_str(), &err, arena);
    if (params.should_pass) {
        EXPECT_NE(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected success but failed.\nContent: " << json_text;
        EXPECT_EQ(err.err_type, JSON_ERR_NONE);
        if (err.err_type != JSON_ERR_NONE) jsonp_print_parse_error(&err);

    } else {
        EXPECT_EQ(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected failure but succeeded.\nContent: " << json_text;
        EXPECT_NE(err.err_type, JSON_ERR_NONE);
        // todo temp remove print after testing that the tests work.
        // Since we expect it to fail, don't print the error
        jsonp_print_parse_error(&err);
    }
}

TEST_P(JsonTestSuiteParam, jsonp_parse_string_ex) {
    const JsonTestParams& params = GetParam();
    std::string json_text = read_file(params.full_path);

    // Pass the explicit size to the parser so it doesn't stop at embedded nulls
    JsonValue *jval = jsonp_parse_string_ex(json_text.c_str(),  &err, arena, json_text.size() );

    if (params.should_pass) {
        EXPECT_NE(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected success but failed.\nContent: " << json_text;
        EXPECT_EQ(err.err_type, JSON_ERR_NONE);
        if (err.err_type != JSON_ERR_NONE) jsonp_print_parse_error(&err);

    } else {
        EXPECT_EQ(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected failure but succeeded.\nContent: " << json_text;
        EXPECT_NE(err.err_type, JSON_ERR_NONE);
        // todo temp remove print after testing that the tests work.
        // Since we expect it to fail, don't print the error
        jsonp_print_parse_error(&err);
    }
}

TEST_P(JsonTestSuiteParam, jsonp_parse_file) {
    const JsonTestParams& params = GetParam();
    // std::string json_filename = read_file(params.full_path);

    JsonValue *jval = jsonp_parse_file(params.full_path.c_str(), &err, arena);

    if (params.should_pass) {
        EXPECT_NE(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected success but failed.\nPath: " << params.full_path;
        EXPECT_EQ(err.err_type, JSON_ERR_NONE);
        if (err.err_type != JSON_ERR_NONE) jsonp_print_parse_error(&err);

    } else {
        EXPECT_EQ(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected failure but succeeded.\nPath: " << params.full_path;
        EXPECT_NE(err.err_type, JSON_ERR_NONE);
        if (err.err_type != JSON_ERR_NONE) jsonp_print_parse_error(&err);

    }
}

INSTANTIATE_TEST_SUITE_P(
    WantPass,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/pass"), true)),
    ParamNameGenerator
);

INSTANTIATE_TEST_SUITE_P(
    WantFail,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/fail"), false)),
    ParamNameGenerator
);

// indeterminate tests that should pass have been moved into the /pass directory
INSTANTIATE_TEST_SUITE_P(
    IndeterminateWantPass,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/indeterminate/want_pass"), true)),
    ParamNameGenerator
);

// indeterminate tests that should pass have been moved into the /pass directory
INSTANTIATE_TEST_SUITE_P(
    IndeterminateWantFail,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/indeterminate/want_fail"), false)),
    ParamNameGenerator
);
