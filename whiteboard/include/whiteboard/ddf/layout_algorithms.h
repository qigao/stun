#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace whiteboard {
namespace ddf {

// Forward declarations
class DataLayer;
struct DataNode;

/**
 * @brief Simple 2D vector for layout positions
 */
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
};

/**
 * @brief Direction for tree layout
 */
enum class TreeDirection { TopDown, BottomUp, LeftRight, RightLeft };

/**
 * @brief Parameters for tree layout algorithm
 */
struct TreeLayoutParams {
  TreeDirection direction = TreeDirection::TopDown;
  float horizontal_spacing = 50.0f;
  float vertical_spacing = 80.0f;
  float sibling_spacing = 30.0f;
  float subtree_spacing = 50.0f;
};

/**
 * @brief Parameters for force-directed layout algorithm
 */
struct ForceDirectedLayoutParams {
  float repulsion_strength = 1000.0f;
  float attraction_strength = 0.1f;
  float damping = 0.9f;
  int max_iterations = 500;
  float convergence_threshold = 0.1f;
  float ideal_edge_length = 100.0f;
};

/**
 * @brief Parameters for grid layout algorithm
 */
struct GridLayoutParams {
  int columns = 0; // 0 = auto-calculate
  float cell_width = 150.0f;
  float cell_height = 100.0f;
  float horizontal_spacing = 20.0f;
  float vertical_spacing = 20.0f;
};

/**
 * @brief Result of a layout computation
 */
struct LayoutResult {
  std::map<std::string, Vec2> positions; // node_id -> position
  Vec2 bounds_min;
  Vec2 bounds_max;
};

/**
 * @brief Tree layout algorithm
 *
 * Implements hierarchical tree layout with support for multiple directions.
 * Uses the Reingold-Tilford algorithm for optimal tree layout.
 */
class TreeLayout {
public:
  TreeLayout(const TreeLayoutParams &params = TreeLayoutParams());

  /**
   * @brief Compute tree layout for hierarchical data
   * @param data_layer The data layer containing nodes and relationships
   * @param root_node_id The ID of the root node (if empty, finds root automatically)
   * @return Layout result with node positions
   */
  LayoutResult compute(const DataLayer &data_layer, const std::string &root_node_id = "");

  void set_params(const TreeLayoutParams &params) { params_ = params; }
  const TreeLayoutParams &get_params() const { return params_; }

private:
  struct TreeNode {
    std::string node_id;
    TreeNode *parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;

    // Layout computation fields
    float x = 0.0f;
    float y = 0.0f;
    float mod = 0.0f;    // Modifier for Reingold-Tilford
    float prelim = 0.0f; // Preliminary x coordinate
    float width = 0.0f;
    float height = 0.0f;
    int depth = 0;
  };

  std::unique_ptr<TreeNode> build_tree(const DataLayer &data_layer, const std::string &root_id);
  void find_roots(const DataLayer &data_layer, std::vector<std::string> &roots);
  void add_children(TreeNode *node, const DataLayer &data_layer);

  void compute_layout(TreeNode *root);
  void first_walk(TreeNode *node);
  void second_walk(TreeNode *node, float modsum);
  void compute_depths(TreeNode *node, int depth);
  float get_node_size(TreeNode *node, bool is_width);

  void collect_positions(TreeNode *node, LayoutResult &result);
  void transform_coordinates(LayoutResult &result);

  TreeLayoutParams params_;
};

/**
 * @brief Force-directed layout algorithm
 *
 * Implements force-directed graph layout using spring-electrical model.
 * Nodes repel each other while edges act as springs.
 */
class ForceDirectedLayout {
public:
  ForceDirectedLayout(const ForceDirectedLayoutParams &params = ForceDirectedLayoutParams());

  /**
   * @brief Compute force-directed layout for graph data
   * @param data_layer The data layer containing nodes and relationships
   * @return Layout result with node positions
   */
  LayoutResult compute(const DataLayer &data_layer);

  void set_params(const ForceDirectedLayoutParams &params) { params_ = params; }
  const ForceDirectedLayoutParams &get_params() const { return params_; }

private:
  struct ForceNode {
    std::string node_id;
    Vec2 position;
    Vec2 velocity;
    Vec2 force;
  };

  void initialize_positions(std::vector<ForceNode> &nodes);
  void compute_forces(std::vector<ForceNode> &nodes, const DataLayer &data_layer);
  Vec2 compute_repulsion(const ForceNode &n1, const ForceNode &n2);
  Vec2 compute_attraction(const ForceNode &n1, const ForceNode &n2);
  void apply_forces(std::vector<ForceNode> &nodes, float dt);
  bool has_converged(const std::vector<ForceNode> &nodes);

  ForceDirectedLayoutParams params_;
};

/**
 * @brief Grid layout algorithm
 *
 * Arranges nodes in a regular grid pattern.
 */
class GridLayout {
public:
  GridLayout(const GridLayoutParams &params = GridLayoutParams());

  /**
   * @brief Compute grid layout for nodes
   * @param data_layer The data layer containing nodes
   * @return Layout result with node positions
   */
  LayoutResult compute(const DataLayer &data_layer);

  void set_params(const GridLayoutParams &params) { params_ = params; }
  const GridLayoutParams &get_params() const { return params_; }

private:
  int calculate_columns(int node_count);

  GridLayoutParams params_;
};

} // namespace ddf
} // namespace whiteboard
