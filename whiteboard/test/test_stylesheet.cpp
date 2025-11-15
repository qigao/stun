#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/stylesheet.h>

using namespace whiteboard::ddf;

TEST_CASE("StyleSheet can add and match type selectors", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}, {"stroke", "blue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {});
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "rect");
    REQUIRE(rules[0].properties.at("fill") == "red");
    REQUIRE(rules[0].properties.at("stroke") == "blue");
    REQUIRE(rules[0].specificity == 1);
}

TEST_CASE("StyleSheet can match class selectors", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule(".highlight", {{"fill", "yellow"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {"highlight"}, {});
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == ".highlight");
    REQUIRE(rules[0].specificity == 10);
}

TEST_CASE("StyleSheet can match ID selectors", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("#shape1", {{"fill", "green"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {});
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "#shape1");
    REQUIRE(rules[0].specificity == 100);
}

TEST_CASE("StyleSheet can match pseudo-class selectors", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect:hover", {{"fill", "lightblue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {"hover"});
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == "rect:hover");
}

TEST_CASE("StyleSheet does not match pseudo-class when state is not active", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect:hover", {{"fill", "lightblue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {});
    
    REQUIRE(rules.empty());
}

TEST_CASE("StyleSheet can match class with pseudo-class", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule(".card:selected", {{"stroke", "blue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {"card"}, {"selected"});
    
    REQUIRE(rules.size() == 1);
    REQUIRE(rules[0].selector == ".card:selected");
}

TEST_CASE("StyleSheet sorts rules by specificity", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}});           // specificity: 1
    stylesheet.add_rule(".highlight", {{"fill", "yellow"}});  // specificity: 10
    stylesheet.add_rule("#shape1", {{"fill", "green"}});      // specificity: 100
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {"highlight"}, {});
    
    REQUIRE(rules.size() == 3);
    REQUIRE(rules[0].specificity == 1);   // rect
    REQUIRE(rules[1].specificity == 10);  // .highlight
    REQUIRE(rules[2].specificity == 100); // #shape1
}

TEST_CASE("StyleSheet compute_style applies cascade correctly", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}, {"stroke", "black"}});
    stylesheet.add_rule(".highlight", {{"fill", "yellow"}});
    
    auto computed = stylesheet.compute_style("shape1", "rect", {"highlight"}, {}, {}, {});
    
    // .highlight should override rect's fill due to higher specificity
    REQUIRE(computed["fill"] == "yellow");
    REQUIRE(computed["stroke"] == "black");
}

TEST_CASE("StyleSheet inline styles have highest priority", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}});
    stylesheet.add_rule("#shape1", {{"fill", "green"}});
    
    std::map<std::string, std::string> inline_style = {{"fill", "blue"}};
    
    auto computed = stylesheet.compute_style("shape1", "rect", {}, {}, inline_style, {});
    
    // Inline style should override everything
    REQUIRE(computed["fill"] == "blue");
}

TEST_CASE("StyleSheet inherits properties from parent", "[stylesheet]") {
    StyleSheet stylesheet;
    
    std::map<std::string, std::string> parent_style = {
        {"font-family", "Arial"},
        {"font-size", "16"},
        {"fill", "red"}  // Not inheritable
    };
    
    auto computed = stylesheet.compute_style("child1", "text", {}, {}, {}, parent_style);
    
    // Should inherit font properties but not fill
    REQUIRE(computed["font-family"] == "Arial");
    REQUIRE(computed["font-size"] == "16");
    REQUIRE(computed["fill"] != "red");  // fill is not inherited
}

TEST_CASE("StyleSheet applies default styles", "[stylesheet]") {
    StyleSheet stylesheet;
    
    auto computed = stylesheet.compute_style("shape1", "rect", {}, {}, {}, {});
    
    // Should have default styles
    REQUIRE(computed.count("fill") > 0);
    REQUIRE(computed.count("stroke") > 0);
    REQUIRE(computed.count("stroke-width") > 0);
}

TEST_CASE("StyleSheet handles multiple classes", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule(".card", {{"fill", "white"}});
    stylesheet.add_rule(".selected", {{"stroke", "blue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {"card", "selected"}, {});
    
    REQUIRE(rules.size() == 2);
}

TEST_CASE("StyleSheet handles multiple pseudo-states", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect:hover", {{"fill", "lightblue"}});
    stylesheet.add_rule("rect:selected", {{"stroke", "blue"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {"hover", "selected"});
    
    REQUIRE(rules.size() == 2);
}

TEST_CASE("StyleSheet specificity calculation is correct", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}});
    stylesheet.add_rule(".class", {{"fill", "red"}});
    stylesheet.add_rule("#id", {{"fill", "red"}});
    stylesheet.add_rule("rect:hover", {{"fill", "red"}});
    stylesheet.add_rule(".class:hover", {{"fill", "red"}});
    
    const auto& rules = stylesheet.get_rules();
    
    REQUIRE(rules[0].specificity == 1);   // rect
    REQUIRE(rules[1].specificity == 10);  // .class
    REQUIRE(rules[2].specificity == 100); // #id
    REQUIRE(rules[3].specificity == 11);  // rect:hover (1 + 10)
    REQUIRE(rules[4].specificity == 20);  // .class:hover (10 + 10)
}

TEST_CASE("StyleSheet clear removes all rules", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("rect", {{"fill", "red"}});
    stylesheet.add_rule(".class", {{"fill", "blue"}});
    
    REQUIRE(stylesheet.get_rules().size() == 2);
    
    stylesheet.clear();
    
    REQUIRE(stylesheet.get_rules().empty());
}

TEST_CASE("StyleSheet handles empty selector", "[stylesheet]") {
    StyleSheet stylesheet;
    
    stylesheet.add_rule("", {{"fill", "red"}});
    
    auto rules = stylesheet.get_matching_rules("shape1", "rect", {}, {});
    
    REQUIRE(rules.empty());
}

TEST_CASE("StyleSheet text elements have appropriate defaults", "[stylesheet]") {
    StyleSheet stylesheet;
    
    auto computed = stylesheet.compute_style("text1", "text", {}, {}, {}, {});
    
    REQUIRE(computed.count("font-family") > 0);
    REQUIRE(computed.count("font-size") > 0);
    REQUIRE(computed["fill"] == "black");  // Text should default to black fill
}
