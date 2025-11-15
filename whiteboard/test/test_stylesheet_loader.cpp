#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/stylesheet_loader.h>

using namespace whiteboard::ddf;

TEST_CASE("StyleSheetLoader can parse basic CSS rules", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        rect {
            fill: #ff0000;
            stroke: #000000;
            stroke-width: 2px;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "rect");
    REQUIRE(rules[0].properties.size() == 3);
    REQUIRE(rules[0].properties[0].name == "fill");
    REQUIRE(rules[0].properties[0].value == "#ff0000");
    REQUIRE(rules[0].properties[1].name == "stroke");
    REQUIRE(rules[0].properties[1].value == "#000000");
    REQUIRE(rules[0].properties[2].name == "stroke-width");
    REQUIRE(rules[0].properties[2].value == "2px");
}

TEST_CASE("StyleSheetLoader can parse class selectors", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        .highlight {
            fill: yellow;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == ".highlight");
    REQUIRE(rules[0].properties.size() == 1);
    REQUIRE(rules[0].properties[0].name == "fill");
    REQUIRE(rules[0].properties[0].value == "yellow");
}

TEST_CASE("StyleSheetLoader can parse ID selectors", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        #node1 {
            fill: blue;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "#node1");
}

TEST_CASE("StyleSheetLoader can parse pseudo-class selectors", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        rect:hover {
            fill: lightblue;
        }
        
        rect:selected {
            stroke: blue;
            stroke-width: 3px;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 2);
    REQUIRE(rules[0].selector == "rect:hover");
    REQUIRE(rules[1].selector == "rect:selected");
}

TEST_CASE("StyleSheetLoader can parse multiple selectors", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        rect, circle {
            fill: red;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 2);
    REQUIRE(rules[0].selector == "rect");
    REQUIRE(rules[1].selector == "circle");
    REQUIRE(rules[0].properties[0].value == "red");
    REQUIRE(rules[1].properties[0].value == "red");
}

TEST_CASE("StyleSheetLoader can parse descendant combinators", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        group rect {
            fill: green;
        }
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "group rect");
}

TEST_CASE("StyleSheetLoader handles CSS comments", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        /* This is a comment */
        rect {
            fill: red;
        }
        /* Another comment */
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "rect");
}

TEST_CASE("StyleSheetLoader handles empty CSS", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = "";
    auto rules = loader.parse(css);
    
    REQUIRE(rules.empty());
}

TEST_CASE("StyleSheetLoader handles whitespace", "[stylesheet_loader]") {
    StyleSheetLoader loader;
    
    std::string css = R"(
        
        
        rect   {
            fill   :   red   ;
            stroke :blue;
        }
        
        
    )";
    
    auto rules = loader.parse(css);
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "rect");
    REQUIRE(rules[0].properties[0].name == "fill");
    REQUIRE(rules[0].properties[0].value == "red");
    REQUIRE(rules[0].properties[1].name == "stroke");
    REQUIRE(rules[0].properties[1].value == "blue");
}
