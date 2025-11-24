#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/render_node.h>

using namespace whiteboard::ddf;

TEST_CASE("RenderNode has all required fields", "[render_node]") {
    RenderNode node;
    
    // Identity
    node.id = "node1";
    node.type = "rect";
    node.classes.push_back("card");
    node.pseudo_states.insert("hover");
    
    // Styles
    node.inline_style["fill"] = "red";
    node.computed_style["stroke"] = "blue";
    
    // Geometry
    node.geometry["x"] = 100.0f;
    node.geometry["y"] = 50.0f;
    node.geometry["width"] = 200.0f;
    node.geometry["height"] = 100.0f;
    
    // Transform
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 10.0f, 20.0f};
    node.world_transform = {1.0f, 0.0f, 0.0f, 1.0f, 10.0f, 20.0f};
    
    // Bounds
    node.bounds_x = 100.0f;
    node.bounds_y = 50.0f;
    node.bounds_width = 200.0f;
    node.bounds_height = 100.0f;
    
    // Data binding
    node.data_node_id = "data1";
    
    // Text
    node.text = "Hello";
    
    // Verify all fields are accessible
    REQUIRE(node.id == "node1");
    REQUIRE(node.type == "rect");
    REQUIRE(node.classes.size() == 1);
    REQUIRE(node.pseudo_states.size() == 1);
    REQUIRE(node.inline_style["fill"] == "red");
    REQUIRE(node.computed_style["stroke"] == "blue");
    REQUIRE(node.geometry["width"] == 200.0f);
    REQUIRE(node.local_transform.size() == 6);
    REQUIRE(node.bounds_width == 200.0f);
    REQUIRE(node.data_node_id == "data1");
    REQUIRE(node.text == "Hello");
}

TEST_CASE("RenderNode supports tree structure", "[render_node]") {
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
    
    REQUIRE(parent.children.size() == 2);
    REQUIRE(parent.children[0]->id == "child1");
    REQUIRE(parent.children[1]->id == "child2");
    REQUIRE(parent.children[0]->parent == &parent);
    REQUIRE(parent.children[1]->parent == &parent);
}

TEST_CASE("RenderNode without parent is a root node", "[render_node]") {
    RenderNode root;
    root.id = "root";
    root.type = "rect";
    
    REQUIRE(root.parent == nullptr);
}

TEST_CASE("RenderNode supports nested tree structure", "[render_node]") {
    RenderNode root;
    root.id = "root";
    
    auto level1 = std::make_unique<RenderNode>();
    level1->id = "level1";
    level1->parent = &root;
    
    auto level2 = std::make_unique<RenderNode>();
    level2->id = "level2";
    level2->parent = level1.get();
    
    level1->children.push_back(std::move(level2));
    root.children.push_back(std::move(level1));
    
    REQUIRE(root.children.size() == 1);
    REQUIRE(root.children[0]->children.size() == 1);
    REQUIRE(root.children[0]->children[0]->id == "level2");
}

TEST_CASE("RenderNode supports multiple CSS classes", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    node.classes.push_back("card");
    node.classes.push_back("highlight");
    node.classes.push_back("selected");
    
    REQUIRE(node.classes.size() == 3);
}

TEST_CASE("RenderNode supports multiple pseudo-states", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    node.pseudo_states.insert("hover");
    node.pseudo_states.insert("selected");
    node.pseudo_states.insert("active");
    
    REQUIRE(node.pseudo_states.size() == 3);
    REQUIRE(node.pseudo_states.count("hover") > 0);
    REQUIRE(node.pseudo_states.count("selected") > 0);
}

TEST_CASE("RenderNode inline_style has higher priority semantics", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    // Inline style should override computed style (by convention)
    node.computed_style["fill"] = "blue";
    node.inline_style["fill"] = "red";
    
    // Both are stored, but inline should be used in rendering
    REQUIRE(node.computed_style["fill"] == "blue");
    REQUIRE(node.inline_style["fill"] == "red");
}

TEST_CASE("RenderNode supports affine transform", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    // Identity transform
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    
    REQUIRE(node.local_transform.size() == 6);
    REQUIRE(node.local_transform[0] == 1.0f);  // a (scale x)
    REQUIRE(node.local_transform[3] == 1.0f);  // d (scale y)
}

TEST_CASE("RenderNode supports translation transform", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    // Translation by (100, 50)
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 50.0f};
    
    REQUIRE(node.local_transform[4] == 100.0f);  // e (translate x)
    REQUIRE(node.local_transform[5] == 50.0f);   // f (translate y)
}

TEST_CASE("RenderNode bounds can be set", "[render_node]") {
    RenderNode node;
    node.id = "node1";
    
    node.bounds_x = 10.0f;
    node.bounds_y = 20.0f;
    node.bounds_width = 100.0f;
    node.bounds_height = 50.0f;
    
    REQUIRE(node.bounds_x == 10.0f);
    REQUIRE(node.bounds_y == 20.0f);
    REQUIRE(node.bounds_width == 100.0f);
    REQUIRE(node.bounds_height == 50.0f);
}

TEST_CASE("RenderNode can store geometry properties", "[render_node]") {
    RenderNode node;
    node.id = "rect1";
    node.type = "rect";
    
    node.geometry["x"] = 0.0f;
    node.geometry["y"] = 0.0f;
    node.geometry["width"] = 120.0f;
    node.geometry["height"] = 60.0f;
    node.geometry["rx"] = 5.0f;  // border radius
    
    REQUIRE(node.geometry.size() == 5);
    REQUIRE(node.geometry["width"] == 120.0f);
    REQUIRE(node.geometry["rx"] == 5.0f);
}

TEST_CASE("RenderNode can store circle geometry", "[render_node]") {
    RenderNode node;
    node.id = "circle1";
    node.type = "circle";
    
    node.geometry["cx"] = 50.0f;
    node.geometry["cy"] = 50.0f;
    node.geometry["r"] = 25.0f;
    
    REQUIRE(node.geometry["r"] == 25.0f);
}
