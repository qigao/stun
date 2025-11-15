#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <whiteboard/ddf/layout_algorithms.h>
#include <whiteboard/ddf/data_layer.h>
#include <cmath>
#include <set>
using namespace whiteboard::ddf;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Create a simple hierarchical tree structure for testing
 * 
 * Structure:
 *       root
 *      /  |  \
 *    n1  n2  n3
 *   / \      |
 *  n4 n5    n6
 */
DataLayer create_tree_data() {
    DataLayer data;
    
    // Create nodes
    DataNode root;
    root.id = "root";
    root.type = "person";
    root.properties["name"] = "CEO";
    data.add_node(root);
    
    DataNode n1;
    n1.id = "n1";
    n1.type = "person";
    n1.properties["name"] = "VP1";
    data.add_node(n1);
    
    DataNode n2;
    n2.id = "n2";
    n2.type = "person";
    n2.properties["name"] = "VP2";
    data.add_node(n2);
    
    DataNode n3;
    n3.id = "n3";
    n3.type = "person";
    n3.properties["name"] = "VP3";
    data.add_node(n3);
    
    DataNode n4;
    n4.id = "n4";
    n4.type = "person";
    n4.properties["name"] = "Manager1";
    data.add_node(n4);
    
    DataNode n5;
    n5.id = "n5";
    n5.type = "person";
    n5.properties["name"] = "Manager2";
    data.add_node(n5);
    
    DataNode n6;
    n6.id = "n6";
    n6.type = "person";
    n6.properties["name"] = "Manager3";
    data.add_node(n6);
    
    // Create relationships (parent-child)
    DataRelationship rel1;
    rel1.id = "rel1";
    rel1.type = "parent-child";
    rel1.from_node_id = "root";
    rel1.to_node_id = "n1";
    data.add_relationship(rel1);
    
    DataRelationship rel2;
    rel2.id = "rel2";
    rel2.type = "parent-child";
    rel2.from_node_id = "root";
    rel2.to_node_id = "n2";
    data.add_relationship(rel2);
    
    DataRelationship rel3;
    rel3.id = "rel3";
    rel3.type = "parent-child";
    rel3.from_node_id = "root";
    rel3.to_node_id = "n3";
    data.add_relationship(rel3);
    
    DataRelationship rel4;
    rel4.id = "rel4";
    rel4.type = "parent-child";
    rel4.from_node_id = "n1";
    rel4.to_node_id = "n4";
    data.add_relationship(rel4);
    
    DataRelationship rel5;
    rel5.id = "rel5";
    rel5.type = "parent-child";
    rel5.from_node_id = "n1";
    rel5.to_node_id = "n5";
    data.add_relationship(rel5);
    
    DataRelationship rel6;
    rel6.id = "rel6";
    rel6.type = "parent-child";
    rel6.from_node_id = "n3";
    rel6.to_node_id = "n6";
    data.add_relationship(rel6);
    
    return data;
}

/**
 * @brief Create a graph structure for testing force-directed layout
 * 
 * Structure: 4 nodes with various connections
 */
DataLayer create_graph_data() {
    DataLayer data;
    
    // Create nodes
    for (int i = 0; i < 4; ++i) {
        DataNode node;
        node.id = "node" + std::to_string(i);
        node.type = "entity";
        node.properties["label"] = "Node " + std::to_string(i);
        data.add_node(node);
    }
    
    // Create relationships (bidirectional connections)
    DataRelationship rel1;
    rel1.id = "rel1";
    rel1.type = "connection";
    rel1.from_node_id = "node0";
    rel1.to_node_id = "node1";
    data.add_relationship(rel1);
    
    DataRelationship rel2;
    rel2.id = "rel2";
    rel2.type = "connection";
    rel2.from_node_id = "node1";
    rel2.to_node_id = "node2";
    data.add_relationship(rel2);
    
    DataRelationship rel3;
    rel3.id = "rel3";
    rel3.type = "connection";
    rel3.from_node_id = "node2";
    rel3.to_node_id = "node3";
    data.add_relationship(rel3);
    
    DataRelationship rel4;
    rel4.id = "rel4";
    rel4.type = "connection";
    rel4.from_node_id = "node3";
    rel4.to_node_id = "node0";
    data.add_relationship(rel4);
    
    return data;
}

/**
 * @brief Create a simple list of nodes for grid layout
 */
DataLayer create_grid_data(int count) {
    DataLayer data;
    
    for (int i = 0; i < count; ++i) {
        DataNode node;
        node.id = "item" + std::to_string(i);
        node.type = "item";
        node.properties["index"] = std::to_string(i);
        data.add_node(node);
    }
    
    return data;
}

// ============================================================================
// Tree Layout Tests
// ============================================================================

TEST_CASE("TreeLayout computes positions for hierarchical data", "[layout][tree]") {
    DataLayer data = create_tree_data();
    TreeLayout layout;
    
    LayoutResult result = layout.compute(data, "root");
    
    // Should have positions for all 7 nodes
    REQUIRE(result.positions.size() == 7);
    REQUIRE(result.positions.count("root") > 0);
    REQUIRE(result.positions.count("n1") > 0);
    REQUIRE(result.positions.count("n2") > 0);
    REQUIRE(result.positions.count("n3") > 0);
    REQUIRE(result.positions.count("n4") > 0);
    REQUIRE(result.positions.count("n5") > 0);
    REQUIRE(result.positions.count("n6") > 0);
}

TEST_CASE("TreeLayout positions root at top in TopDown direction", "[layout][tree]") {
    DataLayer data = create_tree_data();
    
    TreeLayoutParams params;
    params.direction = TreeDirection::TopDown;
    TreeLayout layout(params);
    
    LayoutResult result = layout.compute(data, "root");
    
    // Root should be at y=0 (top)
    REQUIRE(result.positions["root"].y == 0.0f);
    
    // Children should be below root
    REQUIRE(result.positions["n1"].y > result.positions["root"].y);
    REQUIRE(result.positions["n2"].y > result.positions["root"].y);
    REQUIRE(result.positions["n3"].y > result.positions["root"].y);
    
    // Grandchildren should be below children
    REQUIRE(result.positions["n4"].y > result.positions["n1"].y);
    REQUIRE(result.positions["n5"].y > result.positions["n1"].y);
    REQUIRE(result.positions["n6"].y > result.positions["n3"].y);
}

TEST_CASE("TreeLayout positions root at bottom in BottomUp direction", "[layout][tree]") {
    DataLayer data = create_tree_data();
    
    TreeLayoutParams params;
    params.direction = TreeDirection::BottomUp;
    TreeLayout layout(params);
    
    LayoutResult result = layout.compute(data, "root");
    
    // Root should be at y=0 (bottom after transformation)
    REQUIRE(result.positions["root"].y == 0.0f);
    
    // Children should be above root (negative y)
    REQUIRE(result.positions["n1"].y < result.positions["root"].y);
    REQUIRE(result.positions["n2"].y < result.positions["root"].y);
    REQUIRE(result.positions["n3"].y < result.positions["root"].y);
}

TEST_CASE("TreeLayout positions root at left in LeftRight direction", "[layout][tree]") {
    DataLayer data = create_tree_data();
    
    TreeLayoutParams params;
    params.direction = TreeDirection::LeftRight;
    TreeLayout layout(params);
    
    LayoutResult result = layout.compute(data, "root");
    
    // Root should be at x=0 (left)
    REQUIRE(result.positions["root"].x == 0.0f);
    
    // Children should be to the right of root
    REQUIRE(result.positions["n1"].x > result.positions["root"].x);
    REQUIRE(result.positions["n2"].x > result.positions["root"].x);
    REQUIRE(result.positions["n3"].x > result.positions["root"].x);
}

TEST_CASE("TreeLayout respects spacing parameters", "[layout][tree]") {
    DataLayer data = create_tree_data();
    
    TreeLayoutParams params;
    params.direction = TreeDirection::TopDown;
    params.vertical_spacing = 100.0f;
    TreeLayout layout(params);
    
    LayoutResult result = layout.compute(data, "root");
    
    // Check that vertical spacing is applied
    float root_y = result.positions["root"].y;
    float child_y = result.positions["n1"].y;
    
    // The difference should be related to vertical_spacing
    REQUIRE(child_y > root_y);
    REQUIRE(child_y - root_y >= params.vertical_spacing * 0.5f);
}

TEST_CASE("TreeLayout finds root automatically when not specified", "[layout][tree]") {
    DataLayer data = create_tree_data();
    TreeLayout layout;
    
    // Don't specify root - should find it automatically
    LayoutResult result = layout.compute(data, "");
    
    // Should still compute layout for all nodes
    REQUIRE(result.positions.size() == 7);
}

TEST_CASE("TreeLayout handles empty data", "[layout][tree]") {
    DataLayer data;
    TreeLayout layout;
    
    LayoutResult result = layout.compute(data, "");
    
    REQUIRE(result.positions.empty());
}

TEST_CASE("TreeLayout handles single node", "[layout][tree]") {
    DataLayer data;
    
    DataNode node;
    node.id = "single";
    node.type = "node";
    data.add_node(node);
    
    TreeLayout layout;
    LayoutResult result = layout.compute(data, "single");
    
    REQUIRE(result.positions.size() == 1);
    REQUIRE(result.positions.count("single") > 0);
}

TEST_CASE("TreeLayout computes correct bounds", "[layout][tree]") {
    DataLayer data = create_tree_data();
    TreeLayout layout;
    
    LayoutResult result = layout.compute(data, "root");
    
    // Bounds should be set
    REQUIRE(result.bounds_min.x <= result.bounds_max.x);
    REQUIRE(result.bounds_min.y <= result.bounds_max.y);
    
    // All positions should be within bounds
    for (const auto& [node_id, pos] : result.positions) {
        REQUIRE(pos.x >= result.bounds_min.x);
        REQUIRE(pos.x <= result.bounds_max.x);
        REQUIRE(pos.y >= result.bounds_min.y);
        REQUIRE(pos.y <= result.bounds_max.y);
    }
}

// ============================================================================
// Force-Directed Layout Tests
// ============================================================================

TEST_CASE("ForceDirectedLayout computes positions for graph data", "[layout][force_directed]") {
    DataLayer data = create_graph_data();
    ForceDirectedLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    // Should have positions for all 4 nodes
    REQUIRE(result.positions.size() == 4);
    REQUIRE(result.positions.count("node0") > 0);
    REQUIRE(result.positions.count("node1") > 0);
    REQUIRE(result.positions.count("node2") > 0);
    REQUIRE(result.positions.count("node3") > 0);
}

TEST_CASE("ForceDirectedLayout spreads nodes apart", "[layout][force_directed]") {
    DataLayer data = create_graph_data();
    ForceDirectedLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    // Check that nodes are not all at the same position
    Vec2 pos0 = result.positions["node0"];
    Vec2 pos1 = result.positions["node1"];
    Vec2 pos2 = result.positions["node2"];
    Vec2 pos3 = result.positions["node3"];
    
    // Calculate distances between nodes
    auto distance = [](const Vec2& a, const Vec2& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    
    // Nodes should be separated by some minimum distance
    REQUIRE(distance(pos0, pos1) > 10.0f);
    REQUIRE(distance(pos1, pos2) > 10.0f);
    REQUIRE(distance(pos2, pos3) > 10.0f);
}

TEST_CASE("ForceDirectedLayout respects repulsion strength", "[layout][force_directed]") {
    DataLayer data = create_graph_data();
    
    ForceDirectedLayoutParams params;
    params.repulsion_strength = 5000.0f;  // High repulsion
    params.max_iterations = 100;
    ForceDirectedLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // With high repulsion, nodes should be well separated
    Vec2 pos0 = result.positions["node0"];
    Vec2 pos1 = result.positions["node1"];
    
    float dx = pos0.x - pos1.x;
    float dy = pos0.y - pos1.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // Should be reasonably separated
    REQUIRE(distance > 20.0f);
}

TEST_CASE("ForceDirectedLayout converges within max iterations", "[layout][force_directed]") {
    DataLayer data = create_graph_data();
    
    ForceDirectedLayoutParams params;
    params.max_iterations = 50;
    ForceDirectedLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // Should complete without hanging
    REQUIRE(result.positions.size() == 4);
}

TEST_CASE("ForceDirectedLayout handles empty data", "[layout][force_directed]") {
    DataLayer data;
    ForceDirectedLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.empty());
}

TEST_CASE("ForceDirectedLayout handles single node", "[layout][force_directed]") {
    DataLayer data;
    
    DataNode node;
    node.id = "single";
    node.type = "node";
    data.add_node(node);
    
    ForceDirectedLayout layout;
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.size() == 1);
    REQUIRE(result.positions.count("single") > 0);
}

TEST_CASE("ForceDirectedLayout handles disconnected nodes", "[layout][force_directed]") {
    DataLayer data;
    
    // Create nodes with no relationships
    for (int i = 0; i < 3; ++i) {
        DataNode node;
        node.id = "node" + std::to_string(i);
        node.type = "node";
        data.add_node(node);
    }
    
    ForceDirectedLayout layout;
    LayoutResult result = layout.compute(data);
    
    // Should still compute positions (repulsion only)
    REQUIRE(result.positions.size() == 3);
}

TEST_CASE("ForceDirectedLayout computes correct bounds", "[layout][force_directed]") {
    DataLayer data = create_graph_data();
    ForceDirectedLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    // Bounds should be set
    REQUIRE(result.bounds_min.x <= result.bounds_max.x);
    REQUIRE(result.bounds_min.y <= result.bounds_max.y);
    
    // All positions should be within bounds
    for (const auto& [node_id, pos] : result.positions) {
        REQUIRE(pos.x >= result.bounds_min.x);
        REQUIRE(pos.x <= result.bounds_max.x);
        REQUIRE(pos.y >= result.bounds_min.y);
        REQUIRE(pos.y <= result.bounds_max.y);
    }
}

TEST_CASE("ForceDirectedLayout respects ideal edge length", "[layout][force_directed]") {
    DataLayer data;
    
    // Create two connected nodes
    DataNode n1;
    n1.id = "n1";
    n1.type = "node";
    data.add_node(n1);
    
    DataNode n2;
    n2.id = "n2";
    n2.type = "node";
    data.add_node(n2);
    
    DataRelationship rel;
    rel.id = "rel1";
    rel.type = "connection";
    rel.from_node_id = "n1";
    rel.to_node_id = "n2";
    data.add_relationship(rel);
    
    ForceDirectedLayoutParams params;
    params.ideal_edge_length = 150.0f;
    params.max_iterations = 500;
    ForceDirectedLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    Vec2 pos1 = result.positions["n1"];
    Vec2 pos2 = result.positions["n2"];
    
    float dx = pos1.x - pos2.x;
    float dy = pos1.y - pos2.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // Distance should be roughly near ideal edge length (within tolerance)
    REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(params.ideal_edge_length, 50.0f));
}

// ============================================================================
// Grid Layout Tests
// ============================================================================

TEST_CASE("GridLayout arranges nodes in grid pattern", "[layout][grid]") {
    DataLayer data = create_grid_data(9);
    GridLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    // Should have positions for all 9 nodes
    REQUIRE(result.positions.size() == 9);
}

TEST_CASE("GridLayout respects column count", "[layout][grid]") {
    DataLayer data = create_grid_data(6);
    
    GridLayoutParams params;
    params.columns = 3;  // 3 columns, 2 rows
    GridLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // Check that nodes are arranged in 3 columns
    // First 3 items should have same y coordinate
    REQUIRE(result.positions["item0"].y == result.positions["item1"].y);
    REQUIRE(result.positions["item1"].y == result.positions["item2"].y);
    
    // Next 3 items should have same y coordinate (different from first row)
    REQUIRE(result.positions["item3"].y == result.positions["item4"].y);
    REQUIRE(result.positions["item4"].y == result.positions["item5"].y);
    REQUIRE(result.positions["item3"].y != result.positions["item0"].y);
}

TEST_CASE("GridLayout auto-calculates columns for square grid", "[layout][grid]") {
    DataLayer data = create_grid_data(9);
    
    GridLayoutParams params;
    params.columns = 0;  // Auto-calculate
    GridLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // For 9 items, should create 3x3 grid
    // Check that we have 3 distinct y values
    std::set<float> y_values;
    for (const auto& [id, pos] : result.positions) {
        y_values.insert(pos.y);
    }
    
    REQUIRE(y_values.size() == 3);  // 3 rows
}

TEST_CASE("GridLayout respects cell dimensions", "[layout][grid]") {
    DataLayer data = create_grid_data(4);
    
    GridLayoutParams params;
    params.columns = 2;
    params.cell_width = 200.0f;
    params.cell_height = 150.0f;
    params.horizontal_spacing = 20.0f;
    params.vertical_spacing = 30.0f;
    GridLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // Check spacing between columns
    float x_diff = result.positions["item1"].x - result.positions["item0"].x;
    REQUIRE(x_diff == params.cell_width + params.horizontal_spacing);
    
    // Check spacing between rows
    float y_diff = result.positions["item2"].y - result.positions["item0"].y;
    REQUIRE(y_diff == params.cell_height + params.vertical_spacing);
}

TEST_CASE("GridLayout positions first item at origin", "[layout][grid]") {
    DataLayer data = create_grid_data(5);
    GridLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    // First item should be at (0, 0)
    REQUIRE(result.positions["item0"].x == 0.0f);
    REQUIRE(result.positions["item0"].y == 0.0f);
}

TEST_CASE("GridLayout handles empty data", "[layout][grid]") {
    DataLayer data;
    GridLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.empty());
}

TEST_CASE("GridLayout handles single node", "[layout][grid]") {
    DataLayer data = create_grid_data(1);
    GridLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.size() == 1);
    REQUIRE(result.positions["item0"].x == 0.0f);
    REQUIRE(result.positions["item0"].y == 0.0f);
}

TEST_CASE("GridLayout computes correct bounds", "[layout][grid]") {
    DataLayer data = create_grid_data(12);
    
    GridLayoutParams params;
    params.columns = 4;
    params.cell_width = 100.0f;
    params.cell_height = 80.0f;
    params.horizontal_spacing = 10.0f;
    params.vertical_spacing = 10.0f;
    GridLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    // Bounds should start at origin
    REQUIRE(result.bounds_min.x == 0.0f);
    REQUIRE(result.bounds_min.y == 0.0f);
    
    // Max bounds should account for 4 columns and 3 rows
    float expected_max_x = (4 - 1) * (params.cell_width + params.horizontal_spacing) + params.cell_width;
    float expected_max_y = (3 - 1) * (params.cell_height + params.vertical_spacing) + params.cell_height;
    
    REQUIRE(result.bounds_max.x == expected_max_x);
    REQUIRE(result.bounds_max.y == expected_max_y);
}

TEST_CASE("GridLayout handles non-square grids", "[layout][grid]") {
    DataLayer data = create_grid_data(10);
    
    GridLayoutParams params;
    params.columns = 3;  // 3 columns, 4 rows (last row has 1 item)
    GridLayout layout(params);
    
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.size() == 10);
    
    // Check that last item is in correct position
    REQUIRE(result.positions["item9"].x == 0.0f);  // First column of last row
}

TEST_CASE("GridLayout with large node count", "[layout][grid]") {
    DataLayer data = create_grid_data(100);
    GridLayout layout;
    
    LayoutResult result = layout.compute(data);
    
    REQUIRE(result.positions.size() == 100);
    
    // Should auto-calculate to 10x10 grid
    std::set<float> x_values, y_values;
    for (const auto& [id, pos] : result.positions) {
        x_values.insert(pos.x);
        y_values.insert(pos.y);
    }
    
    REQUIRE(x_values.size() == 10);
    REQUIRE(y_values.size() == 10);
}

// ============================================================================
// Parameter Modification Tests
// ============================================================================

TEST_CASE("TreeLayout parameters can be modified", "[layout][tree][parameters]") {
    TreeLayout layout;
    
    TreeLayoutParams params;
    params.direction = TreeDirection::LeftRight;
    params.horizontal_spacing = 100.0f;
    params.vertical_spacing = 150.0f;
    
    layout.set_params(params);
    
    const auto& retrieved_params = layout.get_params();
    REQUIRE(retrieved_params.direction == TreeDirection::LeftRight);
    REQUIRE(retrieved_params.horizontal_spacing == 100.0f);
    REQUIRE(retrieved_params.vertical_spacing == 150.0f);
}

TEST_CASE("ForceDirectedLayout parameters can be modified", "[layout][force_directed][parameters]") {
    ForceDirectedLayout layout;
    
    ForceDirectedLayoutParams params;
    params.repulsion_strength = 2000.0f;
    params.attraction_strength = 0.2f;
    params.max_iterations = 1000;
    
    layout.set_params(params);
    
    const auto& retrieved_params = layout.get_params();
    REQUIRE(retrieved_params.repulsion_strength == 2000.0f);
    REQUIRE(retrieved_params.attraction_strength == 0.2f);
    REQUIRE(retrieved_params.max_iterations == 1000);
}

TEST_CASE("GridLayout parameters can be modified", "[layout][grid][parameters]") {
    GridLayout layout;
    
    GridLayoutParams params;
    params.columns = 5;
    params.cell_width = 120.0f;
    params.cell_height = 90.0f;
    
    layout.set_params(params);
    
    const auto& retrieved_params = layout.get_params();
    REQUIRE(retrieved_params.columns == 5);
    REQUIRE(retrieved_params.cell_width == 120.0f);
    REQUIRE(retrieved_params.cell_height == 90.0f);
}
