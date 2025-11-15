#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/data_layer.h>

using namespace whiteboard::ddf;

TEST_CASE("ShapeLayer can bind shape to data node", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "John Doe";
    node.properties["title"] = "CEO";
    data_layer.add_node(node);
    
    // Create shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    shape_layer.add_shape(shape);
    
    // Bind shape to data
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    
    shape_layer.bind_to_data("shape1", binding);
    
    auto* retrieved = shape_layer.get_shape("shape1");
    REQUIRE(retrieved->has_data_binding == true);
    REQUIRE(retrieved->data_binding.node_id == "node1");
}

TEST_CASE("ShapeLayer updates shape text from data", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "John Doe";
    data_layer.add_node(node);
    
    // Create shape with data binding
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "John Doe");
}

TEST_CASE("ShapeLayer updates shape style from data", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "item";
    node.properties["color"] = "#ff0000";
    data_layer.add_node(node);
    
    // Create shape with data binding
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["fill"] = "{{color}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->inline_style["fill"] == "#ff0000");
}

TEST_CASE("ShapeLayer updates multiple properties from data", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "Jane Smith";
    node.properties["color"] = "#0000ff";
    data_layer.add_node(node);
    
    // Create shape with multiple bindings
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    binding.property_mappings["fill"] = "{{color}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "Jane Smith");
    REQUIRE(updated->inline_style["fill"] == "#0000ff");
}

TEST_CASE("ShapeLayer updates multiple shapes bound to same data", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "Bob Johnson";
    data_layer.add_node(node);
    
    // Create two shapes bound to same data
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "text";
    DataBinding binding1;
    binding1.node_id = "node1";
    binding1.property_mappings["text"] = "{{name}}";
    shape1.data_binding = binding1;
    shape_layer.add_shape(shape1);
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "text";
    DataBinding binding2;
    binding2.node_id = "node1";
    binding2.property_mappings["text"] = "{{name}}";
    shape2.data_binding = binding2;
    shape_layer.add_shape(shape2);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated1 = shape_layer.get_shape("shape1");
    auto* updated2 = shape_layer.get_shape("shape2");
    
    REQUIRE(updated1->text == "Bob Johnson");
    REQUIRE(updated2->text == "Bob Johnson");
}

TEST_CASE("ShapeLayer handles missing data node gracefully", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create shape bound to non-existent data
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    shape.text = "Original";
    
    DataBinding binding;
    binding.node_id = "nonexistent";
    binding.property_mappings["text"] = "{{name}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data (should not crash)
    shape_layer.update_from_data("nonexistent");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "Original");  // Should remain unchanged
}

TEST_CASE("ShapeLayer handles missing data property gracefully", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node without the expected property
    DataNode node;
    node.id = "node1";
    node.type = "person";
    // No "name" property
    data_layer.add_node(node);
    
    // Create shape bound to missing property
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    shape.text = "Original";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "");  // Should be empty string
}

TEST_CASE("ShapeLayer handles shapes without data binding", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create shape without data binding
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    shape.text = "Static Text";
    
    shape_layer.add_shape(shape);
    
    // Update from data (should not affect this shape)
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "Static Text");
}

TEST_CASE("ShapeLayer works without data layer set", "[data_binding]") {
    ShapeLayer shape_layer;
    // No data layer set
    
    // Create shape with data binding
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data (should not crash)
    shape_layer.update_from_data("node1");
    
    // Should complete without error
    REQUIRE(shape_layer.get_shape("shape1") != nullptr);
}

TEST_CASE("ShapeLayer evaluates simple template expressions", "[data_binding]") {
    ShapeLayer shape_layer;
    DataLayer data_layer;
    
    shape_layer.set_data_layer(&data_layer);
    
    // Create data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["firstName"] = "Alice";
    node.properties["lastName"] = "Wonder";
    data_layer.add_node(node);
    
    // Create shape with template
    Shape shape;
    shape.id = "shape1";
    shape.type = "text";
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{firstName}}";
    shape.data_binding = binding;
    
    shape_layer.add_shape(shape);
    
    // Update from data
    shape_layer.update_from_data("node1");
    
    auto* updated = shape_layer.get_shape("shape1");
    REQUIRE(updated->text == "Alice");
}
