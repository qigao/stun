#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/connector_layer.h>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;

// ============================================================================
// Connector CRUD Operations Tests
// ============================================================================

TEST_CASE("ConnectorLayer adds connectors", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";

  connector_layer.add_connector(connector);

  auto *retrieved = connector_layer.get_connector("conn1");
  REQUIRE(retrieved != nullptr);
  REQUIRE(retrieved->id == "conn1");
  REQUIRE(retrieved->from.shape_id == "shape1");
  REQUIRE(retrieved->to.shape_id == "shape2");
}

TEST_CASE("ConnectorLayer removes connectors", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";

  connector_layer.add_connector(connector);
  REQUIRE(connector_layer.get_connector("conn1") != nullptr);

  connector_layer.remove_connector("conn1");
  REQUIRE(connector_layer.get_connector("conn1") == nullptr);
}

TEST_CASE("ConnectorLayer updates connectors", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.style["stroke"] = "#000000";

  connector_layer.add_connector(connector);

  // Update connector
  Connector updated = connector;
  updated.style["stroke"] = "#ff0000";
  updated.to.shape_id = "shape3";

  connector_layer.update_connector("conn1", updated);

  auto *retrieved = connector_layer.get_connector("conn1");
  REQUIRE(retrieved != nullptr);
  REQUIRE(retrieved->style["stroke"] == "#ff0000");
  REQUIRE(retrieved->to.shape_id == "shape3");
}

TEST_CASE("ConnectorLayer retrieves all connectors", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.to.shape_id = "shape2";

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.to.shape_id = "shape3";

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);

  auto connectors = connector_layer.get_all_connectors();
  REQUIRE(connectors.size() == 2);
}

TEST_CASE("ConnectorLayer retrieves connectors for shape", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.to.shape_id = "shape2";

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.to.shape_id = "shape3";

  Connector conn3;
  conn3.id = "conn3";
  conn3.from.shape_id = "shape3";
  conn3.to.shape_id = "shape4";

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);
  connector_layer.add_connector(conn3);

  // Get connectors for shape2 (should be conn1 and conn2)
  auto connectors = connector_layer.get_connectors_for_shape("shape2");
  REQUIRE(connectors.size() == 2);

  // Get connectors for shape1 (should be conn1 only)
  auto connectors_shape1 = connector_layer.get_connectors_for_shape("shape1");
  REQUIRE(connectors_shape1.size() == 1);
  REQUIRE(connectors_shape1[0]->id == "conn1");
}

TEST_CASE("ConnectorLayer clears all connectors", "[connector_layer][crud]") {
  ConnectorLayer connector_layer;

  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.to.shape_id = "shape2";

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.to.shape_id = "shape3";

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);

  REQUIRE(connector_layer.get_all_connectors().size() == 2);

  connector_layer.clear();

  REQUIRE(connector_layer.get_all_connectors().size() == 0);
}

// ============================================================================
// Routing Algorithm Tests
// ============================================================================

TEST_CASE("ConnectorLayer routes straight line", "[connector_layer][routing]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes with connection points
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector with straight routing
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.size() == 4); // 2 points (x, y pairs)

  // Check start point (right side of shape1)
  REQUIRE(conn->path_points[0] == 100.0f); // x = 0 + 100 * 1.0
  REQUIRE(conn->path_points[1] == 25.0f);  // y = 0 + 50 * 0.5

  // Check end point (left side of shape2)
  REQUIRE(conn->path_points[2] == 200.0f); // x = 200 + 100 * 0.0
  REQUIRE(conn->path_points[3] == 25.0f);  // y = 0 + 50 * 0.5
}

TEST_CASE("ConnectorLayer routes orthogonal without obstacles", "[connector_layer][routing]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "bottom";
  cp1.x_ratio = 0.5f;
  cp1.y_ratio = 1.0f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 100.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "top";
  cp2.x_ratio = 0.5f;
  cp2.y_ratio = 0.0f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector with orthogonal routing
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "bottom";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "top";
  connector.routing.algorithm = RoutingAlgorithm::Orthogonal;
  connector.routing.avoid_shapes = false;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.size() >= 4); // At least 2 points

  // Check that path starts at shape1 bottom
  REQUIRE(conn->path_points[0] == 50.0f); // x = 0 + 100 * 0.5
  REQUIRE(conn->path_points[1] == 50.0f); // y = 0 + 50 * 1.0

  // Check that path ends at shape2 top
  size_t last_idx = conn->path_points.size() - 2;
  REQUIRE(conn->path_points[last_idx] == 250.0f);     // x = 200 + 100 * 0.5
  REQUIRE(conn->path_points[last_idx + 1] == 100.0f); // y = 100 + 50 * 0.0
}

TEST_CASE("ConnectorLayer routes bezier curve", "[connector_layer][routing]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 100.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector with bezier routing
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Bezier;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.size() == 8); // Bezier: start, cp1, cp2, end (4 points = 8 values)

  // Check start point
  REQUIRE(conn->path_points[0] == 100.0f); // x = 0 + 100 * 1.0
  REQUIRE(conn->path_points[1] == 25.0f);  // y = 0 + 50 * 0.5

  // Check end point
  REQUIRE(conn->path_points[6] == 200.0f); // x = 200 + 100 * 0.0
  REQUIRE(conn->path_points[7] == 125.0f); // y = 100 + 50 * 0.5

  // Control points should be offset from start and end
  REQUIRE(conn->path_points[2] != conn->path_points[0]); // cp1.x != start.x
  REQUIRE(conn->path_points[4] != conn->path_points[6]); // cp2.x != end.x
}

TEST_CASE("ConnectorLayer routes orthogonal with obstacle avoidance",
          "[connector_layer][routing]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 50.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 50.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  // Add obstacle in the middle
  Shape obstacle;
  obstacle.id = "obstacle";
  obstacle.type = "rect";
  obstacle.geometry["x"] = 100.0f;
  obstacle.geometry["y"] = 0.0f;
  obstacle.geometry["width"] = 50.0f;
  obstacle.geometry["height"] = 50.0f;

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);
  shape_layer.add_shape(obstacle);

  // Create connector with obstacle avoidance
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Orthogonal;
  connector.routing.avoid_shapes = true;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.size() >= 4); // Should have multiple segments to avoid obstacle

  // Path should start at shape1 right
  REQUIRE(conn->path_points[0] == 50.0f);
  REQUIRE(conn->path_points[1] == 25.0f);

  // Path should end at shape2 left
  size_t last_idx = conn->path_points.size() - 2;
  REQUIRE(conn->path_points[last_idx] == 200.0f);
  REQUIRE(conn->path_points[last_idx + 1] == 25.0f);
}

TEST_CASE("ConnectorLayer handles missing connection points", "[connector_layer][routing]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes without explicit connection points
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;
  // No connection points defined

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;
  // No connection points defined

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "nonexistent";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "nonexistent";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.size() == 4);

  // Should use center of shapes as fallback
  REQUIRE(conn->path_points[0] == 50.0f);  // center x of shape1
  REQUIRE(conn->path_points[1] == 25.0f);  // center y of shape1
  REQUIRE(conn->path_points[2] == 250.0f); // center x of shape2
  REQUIRE(conn->path_points[3] == 25.0f);  // center y of shape2
}

// ============================================================================
// Automatic Re-routing Tests
// ============================================================================

TEST_CASE("ConnectorLayer recomputes all paths", "[connector_layer][rerouting]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create multiple connectors
  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.from.connection_point_id = "right";
  conn1.to.shape_id = "shape2";
  conn1.to.connection_point_id = "left";
  conn1.routing.algorithm = RoutingAlgorithm::Straight;

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.from.connection_point_id = "left";
  conn2.to.shape_id = "shape1";
  conn2.to.connection_point_id = "right";
  conn2.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);

  // Initially paths are empty
  REQUIRE(connector_layer.get_connector("conn1")->path_points.empty());
  REQUIRE(connector_layer.get_connector("conn2")->path_points.empty());

  // Recompute all paths
  connector_layer.recompute_all_paths();

  // Now paths should be computed
  REQUIRE(!connector_layer.get_connector("conn1")->path_points.empty());
  REQUIRE(!connector_layer.get_connector("conn2")->path_points.empty());
}

TEST_CASE("ConnectorLayer recomputes paths for specific shape", "[connector_layer][rerouting]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  Shape shape3;
  shape3.id = "shape3";
  shape3.type = "rect";
  shape3.geometry["x"] = 400.0f;
  shape3.geometry["y"] = 0.0f;
  shape3.geometry["width"] = 100.0f;
  shape3.geometry["height"] = 50.0f;

  ConnectionPoint cp3;
  cp3.id = "left";
  cp3.x_ratio = 0.0f;
  cp3.y_ratio = 0.5f;
  shape3.connection_points.push_back(cp3);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);
  shape_layer.add_shape(shape3);

  // Create connectors
  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.from.connection_point_id = "right";
  conn1.to.shape_id = "shape2";
  conn1.to.connection_point_id = "left";
  conn1.routing.algorithm = RoutingAlgorithm::Straight;

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.from.connection_point_id = "left";
  conn2.to.shape_id = "shape3";
  conn2.to.connection_point_id = "left";
  conn2.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);

  // Compute initial paths
  connector_layer.compute_path("conn1");
  connector_layer.compute_path("conn2");

  // Store original path for conn2
  auto original_conn2_path = connector_layer.get_connector("conn2")->path_points;

  // Move shape2
  shape_layer.get_shape("shape2")->geometry["x"] = 250.0f;

  // Recompute paths for shape2
  connector_layer.recompute_paths_for_shape("shape2");

  // conn1 and conn2 should be updated (both connect to shape2)
  auto *conn1_ptr = connector_layer.get_connector("conn1");
  auto *conn2_ptr = connector_layer.get_connector("conn2");

  REQUIRE(!conn1_ptr->path_points.empty());
  REQUIRE(!conn2_ptr->path_points.empty());

  // conn2 path should have changed
  REQUIRE(conn2_ptr->path_points != original_conn2_path);
}

TEST_CASE("ConnectorLayer updates path when shape moves", "[connector_layer][rerouting]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  // Store original end point
  auto *conn = connector_layer.get_connector("conn1");
  float original_end_x = conn->path_points[2];
  float original_end_y = conn->path_points[3];

  // Move shape2
  shape_layer.get_shape("shape2")->geometry["x"] = 300.0f;
  shape_layer.get_shape("shape2")->geometry["y"] = 100.0f;

  // Recompute path
  connector_layer.recompute_paths_for_shape("shape2");

  // End point should have changed
  conn = connector_layer.get_connector("conn1");
  REQUIRE(conn->path_points[2] != original_end_x);
  REQUIRE(conn->path_points[3] != original_end_y);

  // New end point should be at new shape2 position
  REQUIRE(conn->path_points[2] == 300.0f); // x = 300 + 100 * 0.0
  REQUIRE(conn->path_points[3] == 125.0f); // y = 100 + 50 * 0.5
}

// ============================================================================
// Connector Styling and Rendering Tests
// ============================================================================

TEST_CASE("ConnectorLayer handles connector styles", "[connector_layer][styling]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";
  connector.style["stroke"] = "#ff0000";
  connector.style["stroke-width"] = "3";
  connector.arrow_start = ArrowType::Circle;
  connector.arrow_end = ArrowType::Arrow;

  connector_layer.add_connector(connector);

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->style["stroke"] == "#ff0000");
  REQUIRE(conn->style["stroke-width"] == "3");
  REQUIRE(conn->arrow_start == ArrowType::Circle);
  REQUIRE(conn->arrow_end == ArrowType::Arrow);
}

TEST_CASE("ConnectorLayer handles connector labels", "[connector_layer][styling]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";

  ConnectorLabel label;
  label.text = "connects to";
  label.position = 0.5f;
  label.offset_x = 10.0f;
  label.offset_y = -5.0f;
  connector.label = label;

  connector_layer.add_connector(connector);

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->label.has_value());
  REQUIRE(conn->label->text == "connects to");
  REQUIRE(conn->label->position == 0.5f);
  REQUIRE(conn->label->offset_x == 10.0f);
  REQUIRE(conn->label->offset_y == -5.0f);
}

TEST_CASE("ConnectorLayer renders to SVG", "[connector_layer][rendering]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Straight;
  connector.style["stroke"] = "#3498db";
  connector.style["stroke-width"] = "2";
  connector.arrow_end = ArrowType::Arrow;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  // Render to SVG
  std::string svg = connector_layer.render_to_svg("conn1");

  REQUIRE(!svg.empty());
  REQUIRE(svg.find("<path") != std::string::npos);
  REQUIRE(svg.find("stroke=\"#3498db\"") != std::string::npos);
  REQUIRE(svg.find("stroke-width=\"2\"") != std::string::npos);
  REQUIRE(svg.find("marker-end") != std::string::npos);
}

TEST_CASE("ConnectorLayer renders bezier to SVG", "[connector_layer][rendering]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 100.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create bezier connector
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Bezier;
  connector.style["stroke"] = "#e74c3c";
  connector.style["stroke-width"] = "3";

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  // Render to SVG
  std::string svg = connector_layer.render_to_svg("conn1");

  REQUIRE(!svg.empty());
  REQUIRE(svg.find("<path") != std::string::npos);
  REQUIRE(svg.find(" C ") != std::string::npos); // Bezier curve command
  REQUIRE(svg.find("stroke=\"#e74c3c\"") != std::string::npos);
}

TEST_CASE("ConnectorLayer renders label to SVG", "[connector_layer][rendering]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create shapes
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1;
  cp1.id = "right";
  cp1.x_ratio = 1.0f;
  cp1.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2;
  cp2.id = "left";
  cp2.x_ratio = 0.0f;
  cp2.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);

  // Create connector with label
  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.from.connection_point_id = "right";
  connector.to.shape_id = "shape2";
  connector.to.connection_point_id = "left";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  ConnectorLabel label;
  label.text = "connects";
  label.position = 0.5f;
  connector.label = label;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  // Render to SVG
  std::string svg = connector_layer.render_to_svg("conn1");

  REQUIRE(!svg.empty());
  REQUIRE(svg.find("<text") != std::string::npos);
  REQUIRE(svg.find("connects") != std::string::npos);
}

// ============================================================================
// Arrow Type Tests
// ============================================================================

TEST_CASE("ConnectorLayer handles different arrow types", "[connector_layer][arrows]") {
  ConnectorLayer connector_layer;

  // Test all arrow types
  std::vector<ArrowType> arrow_types = {ArrowType::None, ArrowType::Arrow, ArrowType::Diamond,
                                        ArrowType::Circle, ArrowType::Square};

  for (size_t i = 0; i < arrow_types.size(); ++i) {
    Connector connector;
    connector.id = "conn" + std::to_string(i);
    connector.from.shape_id = "shape1";
    connector.to.shape_id = "shape2";
    connector.arrow_start = arrow_types[i];
    connector.arrow_end = arrow_types[i];

    connector_layer.add_connector(connector);

    auto *conn = connector_layer.get_connector(connector.id);
    REQUIRE(conn != nullptr);
    REQUIRE(conn->arrow_start == arrow_types[i]);
    REQUIRE(conn->arrow_end == arrow_types[i]);
  }
}

// ============================================================================
// Routing Configuration Tests
// ============================================================================

TEST_CASE("ConnectorLayer handles routing configuration", "[connector_layer][routing_config]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";
  connector.routing.algorithm = RoutingAlgorithm::Orthogonal;
  connector.routing.avoid_shapes = true;
  connector.routing.padding = 15.0f;
  connector.routing.corner_radius = 8.0f;

  connector_layer.add_connector(connector);

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->routing.algorithm == RoutingAlgorithm::Orthogonal);
  REQUIRE(conn->routing.avoid_shapes == true);
  REQUIRE(conn->routing.padding == 15.0f);
  REQUIRE(conn->routing.corner_radius == 8.0f);
}

TEST_CASE("ConnectorLayer handles all routing algorithms", "[connector_layer][routing_config]") {
  ConnectorLayer connector_layer;

  std::vector<RoutingAlgorithm> algorithms = {
      RoutingAlgorithm::Straight, RoutingAlgorithm::Orthogonal, RoutingAlgorithm::Bezier,
      RoutingAlgorithm::CurvedOrthogonal};

  for (size_t i = 0; i < algorithms.size(); ++i) {
    Connector connector;
    connector.id = "conn" + std::to_string(i);
    connector.from.shape_id = "shape1";
    connector.to.shape_id = "shape2";
    connector.routing.algorithm = algorithms[i];

    connector_layer.add_connector(connector);

    auto *conn = connector_layer.get_connector(connector.id);
    REQUIRE(conn != nullptr);
    REQUIRE(conn->routing.algorithm == algorithms[i]);
  }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_CASE("ConnectorLayer handles nonexistent connector", "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;

  auto *conn = connector_layer.get_connector("nonexistent");
  REQUIRE(conn == nullptr);
}

TEST_CASE("ConnectorLayer handles compute path without shape layer",
          "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;
  // No shape layer set

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(connector);

  // Should not crash, but path will be empty
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.empty());
}

TEST_CASE("ConnectorLayer handles compute path with nonexistent shapes",
          "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "nonexistent1";
  connector.to.shape_id = "nonexistent2";
  connector.routing.algorithm = RoutingAlgorithm::Straight;

  connector_layer.add_connector(connector);
  connector_layer.compute_path("conn1");

  auto *conn = connector_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);
  REQUIRE(conn->path_points.empty());
}

TEST_CASE("ConnectorLayer handles empty connector list", "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;

  auto connectors = connector_layer.get_all_connectors();
  REQUIRE(connectors.empty());

  auto shape_connectors = connector_layer.get_connectors_for_shape("shape1");
  REQUIRE(shape_connectors.empty());
}

TEST_CASE("ConnectorLayer handles render to SVG without path", "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";
  // No path computed

  connector_layer.add_connector(connector);

  std::string svg = connector_layer.render_to_svg("conn1");
  REQUIRE(svg.empty()); // Should return empty string if no path
}

TEST_CASE("ConnectorLayer handles render nonexistent connector", "[connector_layer][edge_cases]") {
  ConnectorLayer connector_layer;

  std::string svg = connector_layer.render_to_svg("nonexistent");
  REQUIRE(svg.empty());
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("ConnectorLayer integrates with ShapeLayer", "[connector_layer][integration]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create a simple diagram with 3 shapes and 2 connectors
  Shape shape1;
  shape1.id = "shape1";
  shape1.type = "rect";
  shape1.geometry["x"] = 0.0f;
  shape1.geometry["y"] = 0.0f;
  shape1.geometry["width"] = 100.0f;
  shape1.geometry["height"] = 50.0f;

  ConnectionPoint cp1_right;
  cp1_right.id = "right";
  cp1_right.x_ratio = 1.0f;
  cp1_right.y_ratio = 0.5f;
  shape1.connection_points.push_back(cp1_right);

  Shape shape2;
  shape2.id = "shape2";
  shape2.type = "rect";
  shape2.geometry["x"] = 200.0f;
  shape2.geometry["y"] = 0.0f;
  shape2.geometry["width"] = 100.0f;
  shape2.geometry["height"] = 50.0f;

  ConnectionPoint cp2_left;
  cp2_left.id = "left";
  cp2_left.x_ratio = 0.0f;
  cp2_left.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2_left);

  ConnectionPoint cp2_right;
  cp2_right.id = "right";
  cp2_right.x_ratio = 1.0f;
  cp2_right.y_ratio = 0.5f;
  shape2.connection_points.push_back(cp2_right);

  Shape shape3;
  shape3.id = "shape3";
  shape3.type = "rect";
  shape3.geometry["x"] = 400.0f;
  shape3.geometry["y"] = 0.0f;
  shape3.geometry["width"] = 100.0f;
  shape3.geometry["height"] = 50.0f;

  ConnectionPoint cp3_left;
  cp3_left.id = "left";
  cp3_left.x_ratio = 0.0f;
  cp3_left.y_ratio = 0.5f;
  shape3.connection_points.push_back(cp3_left);

  shape_layer.add_shape(shape1);
  shape_layer.add_shape(shape2);
  shape_layer.add_shape(shape3);

  // Create connectors
  Connector conn1;
  conn1.id = "conn1";
  conn1.from.shape_id = "shape1";
  conn1.from.connection_point_id = "right";
  conn1.to.shape_id = "shape2";
  conn1.to.connection_point_id = "left";
  conn1.routing.algorithm = RoutingAlgorithm::Straight;

  Connector conn2;
  conn2.id = "conn2";
  conn2.from.shape_id = "shape2";
  conn2.from.connection_point_id = "right";
  conn2.to.shape_id = "shape3";
  conn2.to.connection_point_id = "left";
  conn2.routing.algorithm = RoutingAlgorithm::Bezier;

  connector_layer.add_connector(conn1);
  connector_layer.add_connector(conn2);

  // Compute all paths
  connector_layer.recompute_all_paths();

  // Verify paths are computed
  REQUIRE(!connector_layer.get_connector("conn1")->path_points.empty());
  REQUIRE(!connector_layer.get_connector("conn2")->path_points.empty());

  // Move shape2 and verify connectors update
  shape_layer.get_shape("shape2")->geometry["y"] = 100.0f;
  connector_layer.recompute_paths_for_shape("shape2");

  // Both connectors should be updated
  auto *c1 = connector_layer.get_connector("conn1");
  auto *c2 = connector_layer.get_connector("conn2");

  REQUIRE(!c1->path_points.empty());
  REQUIRE(!c2->path_points.empty());

  // Verify conn1 end point moved
  size_t c1_last = c1->path_points.size() - 2;
  REQUIRE(c1->path_points[c1_last + 1] == 125.0f); // y = 100 + 50 * 0.5

  // Verify conn2 start point moved
  REQUIRE(c2->path_points[1] == 125.0f); // y = 100 + 50 * 0.5
}

TEST_CASE("ConnectorLayer handles complex routing scenario", "[connector_layer][integration]") {
  ConnectorLayer connector_layer;
  ShapeLayer shape_layer;
  connector_layer.set_shape_layer(&shape_layer);

  // Create a grid of shapes
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      Shape shape;
      shape.id = "shape_" + std::to_string(i) + "_" + std::to_string(j);
      shape.type = "rect";
      shape.geometry["x"] = j * 150.0f;
      shape.geometry["y"] = i * 100.0f;
      shape.geometry["width"] = 80.0f;
      shape.geometry["height"] = 40.0f;

      // Add connection points
      ConnectionPoint cp_right;
      cp_right.id = "right";
      cp_right.x_ratio = 1.0f;
      cp_right.y_ratio = 0.5f;
      shape.connection_points.push_back(cp_right);

      ConnectionPoint cp_bottom;
      cp_bottom.id = "bottom";
      cp_bottom.x_ratio = 0.5f;
      cp_bottom.y_ratio = 1.0f;
      shape.connection_points.push_back(cp_bottom);

      ConnectionPoint cp_left;
      cp_left.id = "left";
      cp_left.x_ratio = 0.0f;
      cp_left.y_ratio = 0.5f;
      shape.connection_points.push_back(cp_left);

      ConnectionPoint cp_top;
      cp_top.id = "top";
      cp_top.x_ratio = 0.5f;
      cp_top.y_ratio = 0.0f;
      shape.connection_points.push_back(cp_top);

      shape_layer.add_shape(shape);
    }
  }

  // Create horizontal connectors
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      Connector conn;
      conn.id = "h_conn_" + std::to_string(i) + "_" + std::to_string(j);
      conn.from.shape_id = "shape_" + std::to_string(i) + "_" + std::to_string(j);
      conn.from.connection_point_id = "right";
      conn.to.shape_id = "shape_" + std::to_string(i) + "_" + std::to_string(j + 1);
      conn.to.connection_point_id = "left";
      conn.routing.algorithm = RoutingAlgorithm::Straight;

      connector_layer.add_connector(conn);
    }
  }

  // Create vertical connectors
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      Connector conn;
      conn.id = "v_conn_" + std::to_string(i) + "_" + std::to_string(j);
      conn.from.shape_id = "shape_" + std::to_string(i) + "_" + std::to_string(j);
      conn.from.connection_point_id = "bottom";
      conn.to.shape_id = "shape_" + std::to_string(i + 1) + "_" + std::to_string(j);
      conn.to.connection_point_id = "top";
      conn.routing.algorithm = RoutingAlgorithm::Straight;

      connector_layer.add_connector(conn);
    }
  }

  // Compute all paths
  connector_layer.recompute_all_paths();

  // Verify all connectors have paths
  auto all_connectors = connector_layer.get_all_connectors();
  REQUIRE(all_connectors.size() == 12); // 6 horizontal + 6 vertical

  for (auto *conn : all_connectors) {
    REQUIRE(!conn->path_points.empty());
  }

  // Verify specific shape has correct number of connectors
  auto shape_1_1_connectors = connector_layer.get_connectors_for_shape("shape_1_1");
  REQUIRE(shape_1_1_connectors.size() == 4); // Connected on all 4 sides
}

TEST_CASE("ConnectorLayer handles const correctness", "[connector_layer][const]") {
  ConnectorLayer connector_layer;

  Connector connector;
  connector.id = "conn1";
  connector.from.shape_id = "shape1";
  connector.to.shape_id = "shape2";

  connector_layer.add_connector(connector);

  // Test const methods
  const ConnectorLayer &const_layer = connector_layer;

  const Connector *conn = const_layer.get_connector("conn1");
  REQUIRE(conn != nullptr);

  auto all_connectors = const_layer.get_all_connectors();
  REQUIRE(all_connectors.size() == 1);

  auto shape_connectors = const_layer.get_connectors_for_shape("shape1");
  REQUIRE(shape_connectors.size() == 1);
}
