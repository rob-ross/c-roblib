//
// Created by Rob Ross on 7/1/26.
//

#include <gtest/gtest.h>
#include "json_parser/test_json_parser.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    
    // Now we can use the class because it's declared in the header
    testing::AddGlobalTestEnvironment(new JsonParserEnvironment);
    
    return RUN_ALL_TESTS();
}
