#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/render_node.h>
#include <whiteboard/ddf/stylesheet.h>

using namespace whiteboard::ddf;

TEST_CASE("RenderNode computes styles from stylesheet", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}, {"stroke", "black"}});
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    
    node.compute_styles(stylesheet);
    
    REQUIRE(node.computed_style["fill"] == "red");
    REQUIRE(node.computed_style["stroke"] == "black");
}

TEST_CASE("RenderNode inline styles override stylesheet", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}});
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    node.inline_style["fill"] = "blue";
    
    node.compute_styles(stylesheet);
    
    REQUIRE(node.computed_style["fill"] == "blue");
}

TEST_CASE("RenderNode inherits styles from parent", "[render_node_styles]") {
    StyleSheet stylesheet;
    
    RenderNode parent;
    parent.id = "parent";
    parent.type = "group";
    parent.computed_style["font-family"] = "Arial";
    parent.computed_style["font-size"] = "16";
    parent.computed_style["fill"] = "red";  // Not inheritable
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->type = "text";
    child->parent = &parent;
    
    child->compute_styles(stylesheet);
    
    // Should inherit font properties
    REQUIRE(child->computed_style["font-family"] == "Arial");
    REQUIRE(child->computed_style["font-size"] == "16");
    
    // Should not inherit fill (not inheritable)
    REQUIRE(child->computed_style["fill"] != "red");
}

TEST_CASE("RenderNode applies class selectors", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule(".highlight", {{"fill", "yellow"}});
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    node.classes.push_back("highlight");
    
    node.compute_styles(stylesheet);
    
    REQUIRE(node.computed_style["fill"] == "yellow");
}

TEST_CASE("RenderNode applies ID selectors", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("#special", {{"fill", "green"}});
    
    RenderNode node;
    node.id = "special";
    node.type = "rect";
    
    node.compute_styles(stylesheet);
    
    REQUIRE(node.computed_style["fill"] == "green");
}

TEST_CASE("RenderNode applies pseudo-state selectors", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}});
    stylesheet.add_rule("rect:hover", {{"fill", "blue"}});
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    node.pseudo_states.insert("hover");
    
    node.compute_styles(stylesheet);
    
    REQUIRE(node.computed_style["fill"] == "blue");
}

TEST_CASE("RenderNode respects CSS specificity", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}});           // specificity: 1
    stylesheet.add_rule(".highlight", {{"fill", "yellow"}});  // specificity: 10
    stylesheet.add_rule("#node1", {{"fill", "green"}});       // specificity: 100
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    node.classes.push_back("highlight");
    
    node.compute_styles(stylesheet);
    
    // ID selector should win
    REQUIRE(node.computed_style["fill"] == "green");
}

TEST_CASE("RenderNode recursively computes children styles", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}});
    stylesheet.add_rule("circle", {{"fill", "blue"}});
    
    RenderNode parent;
    parent.id = "parent";
    parent.type = "group";
    
    auto child1 = std::make_unique<RenderNode>();
    child1->id = "child1";
    child1->type = "rect";
    child1->parent = &parent;
    
    auto child2 = std::make_unique<RenderNode>();
    child2->id = "child2";
    child2->type = "circle";
    child2->parent = &parent;
    
    parent.children.push_back(std::move(child1));
    parent.children.push_back(std::move(child2));
    
    parent.compute_styles(stylesheet);
    
    REQUIRE(parent.children[0]->computed_style["fill"] == "red");
    REQUIRE(parent.children[1]->computed_style["fill"] == "blue");
}

TEST_CASE("RenderNode children inherit from parent", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("group", {{"font-family", "Arial"}, {"font-size", "16"}});
    
    RenderNode parent;
    parent.id = "parent";
    parent.type = "group";
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->type = "text";
    child->parent = &parent;
    
    parent.children.push_back(std::move(child));
    
    parent.compute_styles(stylesheet);
    
    // Child should inherit font properties from parent
    REQUIRE(parent.children[0]->computed_style["font-family"] == "Arial");
}

TEST_CASE("RenderNode applies default styles", "[render_node_styles]") {
    StyleSheet stylesheet;
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    
    node.compute_styles(stylesheet);
    
    // Should have default styles
    REQUIRE(node.computed_style.count("fill") > 0);
    REQUIRE(node.computed_style.count("stroke") > 0);
}

TEST_CASE("RenderNode text elements get text defaults", "[render_node_styles]") {
    StyleSheet stylesheet;
    
    RenderNode node;
    node.id = "text1";
    node.type = "text";
    
    node.compute_styles(stylesheet);
    
    // Text should have font defaults
    REQUIRE(node.computed_style.count("font-family") > 0);
    REQUIRE(node.computed_style.count("font-size") > 0);
    REQUIRE(node.computed_style["fill"] == "black");  // Text defaults to black
}

TEST_CASE("RenderNode combines multiple style sources", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("rect", {{"fill", "red"}, {"stroke", "black"}});
    stylesheet.add_rule(".card", {{"stroke-width", "2"}});
    
    RenderNode node;
    node.id = "node1";
    node.type = "rect";
    node.classes.push_back("card");
    node.inline_style["opacity"] = "0.8";
    
    node.compute_styles(stylesheet);
    
    // Should have styles from all sources
    REQUIRE(node.computed_style["fill"] == "red");           // from type selector
    REQUIRE(node.computed_style["stroke"] == "black");       // from type selector
    REQUIRE(node.computed_style["stroke-width"] == "2");     // from class selector
    REQUIRE(node.computed_style["opacity"] == "0.8");        // from inline style
}

TEST_CASE("RenderNode nested inheritance works correctly", "[render_node_styles]") {
    StyleSheet stylesheet;
    stylesheet.add_rule("#root", {{"font-family", "Arial"}});
    
    RenderNode root;
    root.id = "root";
    root.type = "group";
    
    auto level1 = std::make_unique<RenderNode>();
    level1->id = "level1";
    level1->type = "group";
    level1->parent = &root;
    
    auto level2 = std::make_unique<RenderNode>();
    level2->id = "level2";
    level2->type = "text";
    level2->parent = level1.get();
    
    level1->children.push_back(std::move(level2));
    root.children.push_back(std::move(level1));
    
    root.compute_styles(stylesheet);
    
    // Level 2 should inherit from level 1, which inherited from root
    REQUIRE(root.children[0]->children[0]->computed_style["font-family"] == "Arial");
}
