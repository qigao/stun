#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;

TEST_CASE("ShapeLayer can add and retrieve shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    layer.add_shape(shape);
    
    auto* retrieved = layer.get_shape("shape1");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "shape1");
    REQUIRE(retrieved->type == "rect");
}

TEST_CASE("ShapeLayer can remove shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    layer.add_shape(shape);
    REQUIRE(layer.get_shape("shape1") != nullptr);
    
    layer.remove_shape("shape1");
    REQUIRE(layer.get_shape("shape1") == nullptr);
}

TEST_CASE("ShapeLayer can update shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.geometry["width"] = 100.0f;
    
    layer.add_shape(shape);
    
    Shape updated = shape;
    updated.geometry["width"] = 200.0f;
    
    layer.update_shape("shape1", updated);
    
    auto* retrieved = layer.get_shape("shape1");
    REQUIRE(retrieved->geometry["width"] == 200.0f);
}

TEST_CASE("ShapeLayer returns nullptr for non-existent shape", "[shape_layer]") {
    ShapeLayer layer;
    
    REQUIRE(layer.get_shape("nonexistent") == nullptr);
}

TEST_CASE("ShapeLayer can get all shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    auto shapes = layer.get_all_shapes();
    REQUIRE(shapes.size() == 2);
}

TEST_CASE("ShapeLayer can group shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    std::string group_id = layer.group_shapes({"shape1", "shape2"}, "mygroup");
    
    // Verify group was created
    auto* group = layer.get_shape(group_id);
    REQUIRE(group != nullptr);
    REQUIRE(group->type == "group");
    REQUIRE(group->child_shape_ids.size() == 2);
    
    // Verify children have parent reference
    auto* child1 = layer.get_shape("shape1");
    auto* child2 = layer.get_shape("shape2");
    REQUIRE(!child1->parent_shape_id.empty());
    REQUIRE(child1->parent_shape_id == group_id);
    REQUIRE(!child2->parent_shape_id.empty());
    REQUIRE(child2->parent_shape_id == group_id);
}

TEST_CASE("ShapeLayer can ungroup shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    std::string group_id = layer.group_shapes({"shape1", "shape2"}, "mygroup");
    
    // Ungroup
    layer.ungroup_shapes(group_id);
    
    // Verify group was removed
    REQUIRE(layer.get_shape(group_id) == nullptr);
    
    // Verify children no longer have parent reference
    auto* child1 = layer.get_shape("shape1");
    auto* child2 = layer.get_shape("shape2");
    REQUIRE(child1->parent_shape_id.empty());
    REQUIRE(child2->parent_shape_id.empty());
}

TEST_CASE("ShapeLayer can get group children", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    std::string group_id = layer.group_shapes({"shape1", "shape2"}, "mygroup");
    
    auto children = layer.get_group_children(group_id);
    REQUIRE(children.size() == 2);
    REQUIRE(std::find(children.begin(), children.end(), "shape1") != children.end());
    REQUIRE(std::find(children.begin(), children.end(), "shape2") != children.end());
}

TEST_CASE("ShapeLayer can get root shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape root1;
    root1.id = "root1";
    root1.type = "rect";
    
    Shape root2;
    root2.id = "root2";
    root2.type = "circle";
    
    Shape child;
    child.id = "child";
    child.type = "rect";
    child.parent_shape_id = "root1";
    
    layer.add_shape(root1);
    layer.add_shape(root2);
    layer.add_shape(child);
    
    auto roots = layer.get_root_shapes();
    REQUIRE(roots.size() == 2);
    
    // Verify only root shapes are returned
    bool has_root1 = false;
    bool has_root2 = false;
    bool has_child = false;
    
    for (auto* shape : roots) {
        if (shape->id == "root1") has_root1 = true;
        if (shape->id == "root2") has_root2 = true;
        if (shape->id == "child") has_child = true;
    }
    
    REQUIRE(has_root1);
    REQUIRE(has_root2);
    REQUIRE_FALSE(has_child);
}

TEST_CASE("ShapeLayer can get children of a shape", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape parent;
    parent.id = "parent";
    parent.type = "group";
    parent.child_shape_ids = {"child1", "child2"};
    
    Shape child1;
    child1.id = "child1";
    child1.type = "rect";
    child1.parent_shape_id = "parent";
    
    Shape child2;
    child2.id = "child2";
    child2.type = "circle";
    child2.parent_shape_id = "parent";
    
    layer.add_shape(parent);
    layer.add_shape(child1);
    layer.add_shape(child2);
    
    auto children = layer.get_children("parent");
    REQUIRE(children.size() == 2);
    
    bool has_child1 = false;
    bool has_child2 = false;
    
    for (auto* child : children) {
        if (child->id == "child1") has_child1 = true;
        if (child->id == "child2") has_child2 = true;
    }
    
    REQUIRE(has_child1);
    REQUIRE(has_child2);
}

TEST_CASE("ShapeLayer returns empty vector for shape with no children", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    layer.add_shape(shape);
    
    auto children = layer.get_children("shape1");
    REQUIRE(children.empty());
}

TEST_CASE("ShapeLayer can bind data to shape", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    
    layer.add_shape(shape);
    
    DataBinding binding;
    binding.node_id = "node1";
    binding.property_mappings["text"] = "{{name}}";
    
    layer.bind_to_data("shape1", binding);
    
    auto* retrieved = layer.get_shape("shape1");
    REQUIRE(retrieved->has_data_binding == true);
    REQUIRE(retrieved->data_binding.node_id == "node1");
}

TEST_CASE("ShapeLayer clear removes all shapes", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    REQUIRE(layer.get_all_shapes().size() == 2);
    
    layer.clear();
    
    REQUIRE(layer.get_all_shapes().empty());
}

TEST_CASE("ShapeLayer generates unique IDs for groups", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    std::string group_id1 = layer.group_shapes({"shape1"}, "group1");
    std::string group_id2 = layer.group_shapes({"shape2"}, "group2");
    
    REQUIRE(group_id1 != group_id2);
}

TEST_CASE("ShapeLayer handles nested groups", "[shape_layer]") {
    ShapeLayer layer;
    
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "circle";
    
    layer.add_shape(shape1);
    layer.add_shape(shape2);
    
    // Create inner group
    std::string inner_group = layer.group_shapes({"shape1", "shape2"}, "inner");
    
    // Create outer group containing the inner group
    Shape shape3;
    shape3.id = "shape3";
    shape3.type = "rect";
    layer.add_shape(shape3);
    
    std::string outer_group = layer.group_shapes({inner_group, "shape3"}, "outer");
    
    // Verify structure
    auto* outer = layer.get_shape(outer_group);
    REQUIRE(outer->child_shape_ids.size() == 2);
    
    auto* inner = layer.get_shape(inner_group);
    REQUIRE(!inner->parent_shape_id.empty());
    REQUIRE(inner->parent_shape_id == outer_group);
}
