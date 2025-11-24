#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;

TEST_CASE("Shape structure has all required fields", "[shape]") {
    Shape shape;
    
    // Basic fields
    shape.id = "shape1";
    shape.type = "rect";
    
    // CSS-like styling
    shape.classes.push_back("card");
    shape.inline_style["fill"] = "red";
    
    // Geometry
    shape.geometry["x"] = 100.0f;
    shape.geometry["y"] = 100.0f;
    shape.geometry["width"] = 120.0f;
    shape.geometry["height"] = 60.0f;
    
    // Tree structure
    shape.parent_shape_id = "parent1";
    shape.child_shape_ids.push_back("child1");
    shape.child_shape_ids.push_back("child2");
    
    // Pseudo-states
    shape.pseudo_states.insert("hover");
    shape.pseudo_states.insert("selected");
    
    // Verify all fields are accessible
    REQUIRE(shape.id == "shape1");
    REQUIRE(shape.type == "rect");
    REQUIRE(shape.classes.size() == 1);
    REQUIRE(shape.inline_style["fill"] == "red");
    REQUIRE(shape.geometry["width"] == 120.0f);
    REQUIRE(!shape.parent_shape_id.empty());
    REQUIRE(shape.parent_shape_id == "parent1");
    REQUIRE(shape.child_shape_ids.size() == 2);
    REQUIRE(shape.pseudo_states.size() == 2);
}

TEST_CASE("Shape supports connection points", "[shape]") {
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    ConnectionPoint top = {"top", 0.5f, 0.0f};
    ConnectionPoint bottom = {"bottom", 0.5f, 1.0f};
    
    shape.connection_points.push_back(top);
    shape.connection_points.push_back(bottom);
    
    REQUIRE(shape.connection_points.size() == 2);
    REQUIRE(shape.connection_points[0].id == "top");
    REQUIRE(shape.connection_points[0].x_ratio == 0.5f);
    REQUIRE(shape.connection_points[0].y_ratio == 0.0f);
}

TEST_CASE("Shape supports data binding", "[shape]") {
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    binding.property_mappings["fill"] = "{{department | color_map}}";
    
    shape.data_binding = binding;
    shape.has_data_binding = true;
    
    REQUIRE(shape.has_data_binding == true);
    REQUIRE(shape.data_binding.node_id == "node1");
    REQUIRE(shape.data_binding.property_mappings.size() == 2);
}

TEST_CASE("Shape supports transform", "[shape]") {
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    // Affine transform: a, b, c, d, e, f
    shape.transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 50.0f};
    
    REQUIRE(shape.transform.size() == 6);
    REQUIRE(shape.transform[4] == 100.0f);  // translate x
    REQUIRE(shape.transform[5] == 50.0f);   // translate y
}

TEST_CASE("Shape supports text content", "[shape]") {
    Shape shape;
    shape.id = "text1";
    shape.type = "text";
    shape.text = "Hello World";
    
    REQUIRE(shape.text == "Hello World");
}

TEST_CASE("Shape tree structure - parent-child relationships", "[shape]") {
    Shape parent;
    parent.id = "parent";
    parent.type = "group";
    
    Shape child1;
    child1.id = "child1";
    child1.type = "rect";
    child1.parent_shape_id = "parent";
    
    Shape child2;
    child2.id = "child2";
    child2.type = "circle";
    child2.parent_shape_id = "parent";
    
    parent.child_shape_ids.push_back("child1");
    parent.child_shape_ids.push_back("child2");
    
    // Verify parent has children
    REQUIRE(parent.child_shape_ids.size() == 2);
    REQUIRE(parent.child_shape_ids[0] == "child1");
    REQUIRE(parent.child_shape_ids[1] == "child2");
    
    // Verify children have parent
    REQUIRE(!child1.parent_shape_id.empty());
    REQUIRE(child1.parent_shape_id == "parent");
    REQUIRE(!child2.parent_shape_id.empty());
    REQUIRE(child2.parent_shape_id == "parent");
}

TEST_CASE("Shape without parent is a root shape", "[shape]") {
    Shape root;
    root.id = "root";
    root.type = "rect";
    
    REQUIRE(root.parent_shape_id.empty());
}

TEST_CASE("Shape pseudo-states can be managed", "[shape]") {
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    // Add pseudo-states
    shape.pseudo_states.insert("hover");
    REQUIRE(shape.pseudo_states.count("hover") > 0);
    
    shape.pseudo_states.insert("selected");
    REQUIRE(shape.pseudo_states.size() == 2);
    
    // Remove pseudo-state
    shape.pseudo_states.erase("hover");
    REQUIRE(shape.pseudo_states.count("hover") == 0);
    REQUIRE(shape.pseudo_states.size() == 1);
}

TEST_CASE("Shape supports multiple CSS classes", "[shape]") {
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    shape.classes.push_back("card");
    shape.classes.push_back("highlight");
    shape.classes.push_back("selected");
    
    REQUIRE(shape.classes.size() == 3);
    REQUIRE(std::find(shape.classes.begin(), shape.classes.end(), "card") != shape.classes.end());
    REQUIRE(std::find(shape.classes.begin(), shape.classes.end(), "highlight") != shape.classes.end());
}
