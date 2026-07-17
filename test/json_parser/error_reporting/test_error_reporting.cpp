//  test_error_reporting.cpp
// Created by Rob Ross on 7/13/26.
//

#include <algorithm>
#include <gtest/gtest.h>
#include "../test_json_parser.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>


extern "C" {
#include "roblib/json_parser.h"
}

struct JsonTestParam {
    std::string test_name;
    std::string json_text;
    JsonParseErrType json_error_type;
    uint32_t    first_bad_char;
    uint32_t    parse_start;
    uint32_t    parse_end;
};

class JsonTestErrorReportingParam : public JsonParserTest, public testing::WithParamInterface<JsonTestParam> {
private:
    static std::string unescape(const std::string& s) {
        std::string res;
        res.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'x') {
                i += 2;
                std::string hexStr;
                while (i < s.size() && hexStr.size() < 2 && isxdigit(static_cast<unsigned char>(s[i]))) {
                    hexStr += s[i++];
                }
                if (!hexStr.empty()) {
                    try {
                        res += static_cast<char>(std::stoul(hexStr, nullptr, 16));
                    } catch (...) {
                        // If stoul fails for some reason, just append the original characters
                        res += "\\x" + hexStr;
                    }
                }
                i--; // Back up for the loop increment
            } else {
                res += s[i];
            }
        }
        return res;
    }

public:
    static std::vector<JsonTestParam> read_file(const std::string& path) {
        std::vector<JsonTestParam> params;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        std::string line;
        while (std::getline(file, line)) {
            // Trim trailing \r if present (for CRLF compatibility in binary mode)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                continue;
            }

            // Ignore comments
            if (line[0] == '#') {
                continue;
            }

            std::stringstream ss(line);
            std::string name, json_text, err_type_str, bad_char_str, start_str, end_str;

            if (std::getline(ss, name, '|') &&
                std::getline(ss, json_text, '|') &&
                std::getline(ss, err_type_str, '|') &&
                std::getline(ss, bad_char_str, '|') &&
                std::getline(ss, start_str, '|') &&
                std::getline(ss, end_str)) {

                try {
                    JsonTestParam param;
                    param.test_name = name;
                    param.json_text = unescape(json_text);
                    param.json_error_type = static_cast<JsonParseErrType>(std::stoi(err_type_str));
                    param.first_bad_char = static_cast<uint32_t>(std::stoul(bad_char_str));
                    param.parse_start = static_cast<uint32_t>(std::stoul(start_str));
                    param.parse_end = static_cast<uint32_t>(std::stoul(end_str));
                    params.push_back(param);
                } catch (...) {
                    // Return empty vector on error as requested
                    return {};
                }
            }
        }

        return params;
    }
};

// Custom name generator for the test runner output
std::string ParamNameGenerator(const testing::TestParamInfo<JsonTestParam>& info) {
    // Prefix with Pass/Fail to distinguish categories and help with the "starts with underscore" constraint
    std::string name = info.param.test_name;

    // Sanitize name: GTest only allows alphanumeric and underscores
    std::replace_if(name.begin(), name.end(), [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');

    // Append the index to guarantee uniqueness even if filenames sanitize to the same string (e.g., .e+ vs .e-)
    return name + "_" + std::to_string(info.index);
}

TEST_P(JsonTestErrorReportingParam, parse_json) {
    const JsonTestParam& param = GetParam();
    std::string json_text = param.json_text;

    JsonValue *jval = jsonp_parse(json_text.c_str(), &err, JsonParserEnvironment::arena);

    EXPECT_EQ(jval, nullptr)
        << "test: " << param.test_name
        << "\nExpected failure but succeeded.\nContent: " << json_text;
    EXPECT_EQ(err.err_type, param.json_error_type) << err.message  << ", json=" << err.json;
    EXPECT_EQ(err.first_bad_char, param.first_bad_char ) << err.json;
    EXPECT_EQ(err.parse_start, param.parse_start ) << err.json;
    EXPECT_EQ(err.parse_end, param.parse_end ) << err.json;

}

INSTANTIATE_TEST_SUITE_P(
    ShouldFail,
    JsonTestErrorReportingParam,
    testing::ValuesIn(JsonTestErrorReportingParam::read_file(std::string(ERROR_REPORTING_DATA_PATH) + "/error_reporting_tests.txt")),
    ParamNameGenerator
);
