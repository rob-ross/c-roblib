//  test_JSONTestSuite.cpp
//
// Created by Rob Ross on 7/11/26.
//
// Run JSONTestSuite against the JSON parser


#include "test_json_parser.h"

#include <dirent.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <functional>
#include <algorithm>
#include <vector>
#include <cctype>

extern "C" {
#include "roblib/json_parser.h"
}

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

void collect_files(const char* path, bool should_pass, std::vector<JsonTestParams>& tests) {
    DIR *dir = opendir(path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    struct stat file_stat;
    char full_path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        // Skip the current and parent directory pointers
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path for the stat call
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (len >= (int)sizeof(full_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            continue;
        }

        if (lstat(full_path, &file_stat) == -1) {
            continue;
        }

        const bool is_dir  = S_ISDIR(file_stat.st_mode);
        const bool is_file = S_ISREG(file_stat.st_mode);

        if (is_dir) {
            // for this test we don't need to recurse into subdirectories
        } else if ( is_file) {
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext || ext == entry->d_name) continue;
            ext++; // Move past the dot

            if (strcmp(ext, "json") != 0) {
                continue;
            }

            tests.push_back({entry->d_name, full_path, should_pass});
        }
    }
    closedir(dir);
}

std::vector<JsonTestParams> GetTestSuiteFiles(std::string test_file_path, bool should_pass) {
    std::vector<JsonTestParams> tests;
    collect_files(std::string(JSONTestSuite_DATA_PATH + test_file_path).c_str(), should_pass, tests);
    return tests;
}

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

TEST_P(JsonTestSuiteParam, parse_json) {
    const JsonTestParams& params = GetParam();
    std::string json_text = read_file(params.full_path);

    // Pass the explicit size to the parser so it doesn't stop at embedded nulls
    // JsonValue *jval = json_parse_ex(json_text.c_str(), json_text.size(), &err, arena);
    JsonValue *jval = jsonp_parse(json_text.c_str(), &err, arena);
    if (params.should_pass) {
        EXPECT_NE(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected success but failed.\nContent: " << json_text;
        EXPECT_EQ(err.err_type, JSON_ERR_NONE);
    } else {
        EXPECT_EQ(jval, nullptr)
            << "File: " << params.filename
            << "\nExpected failure but succeeded.\nContent: " << json_text;
        EXPECT_NE(err.err_type, JSON_ERR_NONE);
        jsonp_print_parse_error(&err);
    }

}

// TEST_P(JsonTestSuiteParam, FileTests) {
//     const JsonTestParams& params = GetParam();
//     std::string json_text = read_file(params.full_path);
//
//     JsonValue *jval = json_parse(json_text.c_str(), &err, arena);
//
//     if (params.should_pass) {
//         EXPECT_NE(jval, nullptr)
//             << "File: " << params.filename
//             << "\nExpected success but failed.\nContent: " << json_text;
//         EXPECT_EQ(err.err_type, JSON_ERR_NONE);
//     } else {
//         EXPECT_EQ(jval, nullptr)
//             << "File: " << params.filename
//             << "\nExpected failure but succeeded.\nContent: " << json_text;
//         EXPECT_NE(err.err_type, JSON_ERR_NONE);
//     }
// }

INSTANTIATE_TEST_SUITE_P(
    ShouldPass,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/pass"), true)),
    ParamNameGenerator
);

INSTANTIATE_TEST_SUITE_P(
    ShouldFail,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/fail"), false)),
    ParamNameGenerator
);

// INSTANTIATE_TEST_SUITE_P(
//     Indeterminate_as_Pass,
//     JsonTestSuiteParam,
//     testing::ValuesIn(GetTestSuiteFiles(std::string("/indeterminate"), true)),
//     ParamNameGenerator
// );

INSTANTIATE_TEST_SUITE_P(
    Indeterminate_as_Fail,
    JsonTestSuiteParam,
    testing::ValuesIn(GetTestSuiteFiles(std::string("/indeterminate"), false)),
    ParamNameGenerator
);
