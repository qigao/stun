#include <catch2/catch_test_macros.hpp>
#include <whiteboard/lexbor/lexbor_css_parser.h>
#include <iostream>

using namespace whiteboard::lexbor;

TEST_CASE("Debug attribute word match", "[debug]") {
    LexborSelectorMatcher matcher;
    
    std::map<std::string, std::string> attrs = {
        {"class", "btn primary large"}
    };
    
    std::cout << "Testing [class~=\"primary\"]" << std::endl;
    std::cout << "Attribute 'class' value: '" << attrs["class"] << "'" << std::endl;
    
    bool result = matcher.matches("[class~=\"primary\"]", "shape1", "rect", {}, attrs, {});
    std::cout << "Match result: " << (result ? "true" : "false") << std::endl;
    
    REQUIRE(result);
}
