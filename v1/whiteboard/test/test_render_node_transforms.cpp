#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/render_node.h>
#include <cmath>

using namespace whiteboard::ddf;

// Helper to compare floats with tolerance
bool approx_equal(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

TEST_CASE("RenderNode identity transform", "[render_node_transforms]") {
    RenderNode node;
    node.id = "node1";
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    
    node.compute_transforms();
    
    REQUIRE(node.world_transform.size() == 6);
    REQUIRE(node.world_transform[0] == 1.0f);  // a
    REQUIRE(node.world_transform[1] == 0.0f);  // b
    REQUIRE(node.world_transform[2] == 0.0f);  // c
    REQUIRE(node.world_transform[3] == 1.0f);  // d
    REQUIRE(node.world_transform[4] == 0.0f);  // e
    REQUIRE(node.world_transform[5] == 0.0f);  // f
}

TEST_CASE("RenderNode translation transform", "[render_node_transforms]") {
    RenderNode node;
    node.id = "node1";
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 50.0f};
    
    node.compute_transforms();
    
    REQUIRE(node.world_transform[4] == 100.0f);  // translate x
    REQUIRE(node.world_transform[5] == 50.0f);   // translate y
}

TEST_CASE("RenderNode scale transform", "[render_node_transforms]") {
    RenderNode node;
    node.id = "node1";
    node.local_transform = {2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};
    
    node.compute_transforms();
    
    REQUIRE(node.world_transform[0] == 2.0f);  // scale x
    REQUIRE(node.world_transform[3] == 2.0f);  // scale y
}

TEST_CASE("RenderNode child inherits parent transform", "[render_node_transforms]") {
    RenderNode parent;
    parent.id = "parent";
    parent.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 50.0f};
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 20.0f, 10.0f};
    child->parent = &parent;
    
    parent.children.push_back(std::move(child));
    
    parent.compute_transforms();
    
    // Parent world transform should be its local transform
    REQUIRE(parent.world_transform[4] == 100.0f);
    REQUIRE(parent.world_transform[5] == 50.0f);
    
    // Child world transform should be parent + child
    REQUIRE(parent.children[0]->world_transform[4] == 120.0f);  // 100 + 20
    REQUIRE(parent.children[0]->world_transform[5] == 60.0f);   // 50 + 10
}

TEST_CASE("RenderNode nested transforms accumulate", "[render_node_transforms]") {
    RenderNode root;
    root.id = "root";
    root.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f};
    
    auto level1 = std::make_unique<RenderNode>();
    level1->id = "level1";
    level1->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 20.0f, 20.0f};
    level1->parent = &root;
    
    auto level2 = std::make_unique<RenderNode>();
    level2->id = "level2";
    level2->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 30.0f, 30.0f};
    level2->parent = level1.get();
    
    level1->children.push_back(std::move(level2));
    root.children.push_back(std::move(level1));
    
    root.compute_transforms();
    
    // Root: 10, 10
    REQUIRE(root.world_transform[4] == 10.0f);
    REQUIRE(root.world_transform[5] == 10.0f);
    
    // Level1: 10 + 20 = 30, 10 + 20 = 30
    REQUIRE(root.children[0]->world_transform[4] == 30.0f);
    REQUIRE(root.children[0]->world_transform[5] == 30.0f);
    
    // Level2: 30 + 30 = 60, 30 + 30 = 60
    REQUIRE(root.children[0]->children[0]->world_transform[4] == 60.0f);
    REQUIRE(root.children[0]->children[0]->world_transform[5] == 60.0f);
}

TEST_CASE("RenderNode scale accumulates correctly", "[render_node_transforms]") {
    RenderNode parent;
    parent.id = "parent";
    parent.local_transform = {2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};  // scale 2x
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->local_transform = {3.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f};  // scale 3x
    child->parent = &parent;
    
    parent.children.push_back(std::move(child));
    
    parent.compute_transforms();
    
    // Child should have combined scale of 2 * 3 = 6
    REQUIRE(parent.children[0]->world_transform[0] == 6.0f);
    REQUIRE(parent.children[0]->world_transform[3] == 6.0f);
}

TEST_CASE("RenderNode translation with parent scale", "[render_node_transforms]") {
    RenderNode parent;
    parent.id = "parent";
    parent.local_transform = {2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};  // scale 2x
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f};  // translate 10, 10
    child->parent = &parent;
    
    parent.children.push_back(std::move(child));
    
    parent.compute_transforms();
    
    // Child translation should be scaled by parent: 10 * 2 = 20
    REQUIRE(parent.children[0]->world_transform[4] == 20.0f);
    REQUIRE(parent.children[0]->world_transform[5] == 20.0f);
}

TEST_CASE("RenderNode handles missing local transform", "[render_node_transforms]") {
    RenderNode node;
    node.id = "node1";
    // No local_transform set
    
    node.compute_transforms();
    
    // Should default to identity
    REQUIRE(node.world_transform.size() == 6);
    REQUIRE(node.world_transform[0] == 1.0f);
    REQUIRE(node.world_transform[3] == 1.0f);
}

TEST_CASE("RenderNode without parent uses local transform", "[render_node_transforms]") {
    RenderNode node;
    node.id = "node1";
    node.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 50.0f, 25.0f};
    
    node.compute_transforms();
    
    // World transform should equal local transform
    REQUIRE(node.world_transform == node.local_transform);
}

TEST_CASE("RenderNode multiple children get correct transforms", "[render_node_transforms]") {
    RenderNode parent;
    parent.id = "parent";
    parent.local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 100.0f, 100.0f};
    
    auto child1 = std::make_unique<RenderNode>();
    child1->id = "child1";
    child1->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 10.0f, 10.0f};
    child1->parent = &parent;
    
    auto child2 = std::make_unique<RenderNode>();
    child2->id = "child2";
    child2->local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 20.0f, 20.0f};
    child2->parent = &parent;
    
    parent.children.push_back(std::move(child1));
    parent.children.push_back(std::move(child2));
    
    parent.compute_transforms();
    
    // Child1: 100 + 10 = 110
    REQUIRE(parent.children[0]->world_transform[4] == 110.0f);
    REQUIRE(parent.children[0]->world_transform[5] == 110.0f);
    
    // Child2: 100 + 20 = 120
    REQUIRE(parent.children[1]->world_transform[4] == 120.0f);
    REQUIRE(parent.children[1]->world_transform[5] == 120.0f);
}

TEST_CASE("RenderNode combined scale and translation", "[render_node_transforms]") {
    RenderNode parent;
    parent.id = "parent";
    parent.local_transform = {2.0f, 0.0f, 0.0f, 2.0f, 50.0f, 50.0f};  // scale 2x, translate 50,50
    
    auto child = std::make_unique<RenderNode>();
    child->id = "child";
    child->local_transform = {1.5f, 0.0f, 0.0f, 1.5f, 10.0f, 10.0f};  // scale 1.5x, translate 10,10
    child->parent = &parent;
    
    parent.children.push_back(std::move(child));
    
    parent.compute_transforms();
    
    // Combined scale: 2 * 1.5 = 3
    REQUIRE(approx_equal(parent.children[0]->world_transform[0], 3.0f));
    REQUIRE(approx_equal(parent.children[0]->world_transform[3], 3.0f));
    
    // Combined translation: parent_translate + parent_scale * child_translate
    // x: 50 + 2 * 10 = 70
    // y: 50 + 2 * 10 = 70
    REQUIRE(approx_equal(parent.children[0]->world_transform[4], 70.0f));
    REQUIRE(approx_equal(parent.children[0]->world_transform[5], 70.0f));
}
