#include <catch2/catch_test_macros.hpp>
#include <whiteboard/lexbor/lexbor_css_parser.h>
#include <iostream>

using namespace whiteboard::lexbor;

TEST_CASE("Debug word match step by step", "[debug]") {
    LexborSelectorMatcher matcher;
    
    // Test 1: Simple exact match first
    std::map<std::string, std::string> attrs1 = {{"class", "btn"}};
    bool result1 = matcher.matches("[class=\"btn\"]", "shape1", "rect", {}, attrs1, {});
    std::cout << "Test 1 [class=\"btn\"] with value 'btn': " << (result1 ? "PASS" : "FAIL") << std::endl;
    REQUIRE(result1);
    
    // Test 2: Word match with single word
    std::map<std::string, std::string> attrs2 = {{"class", "btn"}};
    bool result2 = matcher.matches("[class~=\"btn\"]", "shape1", "rect", {}, attrs2, {});
    std::cout << "Test 2 [class~=\"btn\"] with value 'btn': " << (result2 ? "PASS" : "FAIL") << std::endl;
    REQUIRE(result2);
    
    // Test 3: Word match with multiple words
    std::map<std::string, std::string> attrs3 = {{"class", "btn primary"}};
    bool result3 = matcher.matches("[class~=\"btn\"]", "shape1", "rect", {}, attrs3, {});
    std::cout << "Test 3 [class~=\"btn\"] with value 'btn primary': " << (result3 ? "PASS" : "FAIL") << std::endl;
    REQUIRE(result3);
    
    // Test 4: Word match middle word
    std::map<std::string, std::string> attrs4 = {{"class", "btn primary large"}};
    bool result4 = matcher.matches("[class~=\"primary\"]", "shape1", "rect", {}, attrs4, {});
    std::cout << "Test 4 [class~=\"primary\"] with value 'btn primary large': " << (result4 ? "PASS" : "FAIL") << std::endl;
    REQUIRE(result4);
}
