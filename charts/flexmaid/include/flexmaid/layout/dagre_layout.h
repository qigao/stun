#pragma once

#include "../ir/unified_diagram.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace flex {
namespace modules {
namespace flexmaid {

struct Theme;
struct LayoutData;

/**
 * @brief Improved DAG-based layout engine using Sugiyama framework
 * 
 * This implements a hierarchical graph layout algorithm similar to Dagre,
 * which is used by Mermaid.js. The algorithm has 4 main phases:
 * 
 * 1. Cycle Removal: Make the graph acyclic by reversing edges
 * 2. Layer Assignment: Assign nodes to horizontal layers (ranks)
 * 3. Crossing Minimization: Reduce edge crossings between layers
 * 4. Coordinate Assignment: Assign actual x,y positions to nodes
 */
class DagreLayoutEngine {
public:
    struct Config {
        float node_spacing = 50.0f;      // Horizontal spacing between nodes
        float rank_spacing = 80.0f;      // Vertical spacing between ranks
        float edge_spacing = 10.0f;      // Spacing for edge routing
        float margin_x = 50.0f;          // Left/right margin
        float margin_y = 70.0f;          // Top/bottom margin (extra for subgraph headers)
        bool align_edges = true;         // Align edges to reduce bends
        std::string direction = "TB";    // TB, BT, LR, RL
    };

    DagreLayoutEngine(const Config& config = Config());

    /**
     * @brief Compute layout for a flowchart diagram
     */
    void layout_flowchart(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);

    /**
     * @brief Compute layout for a sequence diagram
     */
    void layout_sequence(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);

    /**
     * @brief Compute layout for a class diagram
     */
    void layout_class(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);

    /**
     * @brief Compute layout for a state diagram
     */
    void layout_state(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);

    /**
     * @brief Compute layout for an ER diagram
     */
    void layout_er(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);
    
    /**
     * @brief Compute layout for a block diagram
     */
    void layout_block(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme);

private:
    Config config_;

    // Internal graph representation
    struct GraphNode {
        std::string id;
        float width, height;
        int rank = -1;              // Layer assignment
        int order = -1;             // Order within layer
        float x = 0, y = 0;         // Final coordinates
        std::vector<std::string> in_edges;
        std::vector<std::string> out_edges;
    };

    struct GraphEdge {
        std::string from, to;
        bool reversed = false;      // True if reversed for cycle removal
        std::vector<std::string> dummy_nodes; // For long edges
    };

    std::map<std::string, GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
    std::vector<std::vector<std::string>> ranks_; // ranks_[i] = list of node IDs in rank i
    
    // Performance: Cache for node size calculations
    std::map<std::pair<std::string, NodeShape>, std::pair<float, float>> size_cache_;

    // Phase 1: Cycle removal using DFS
    void remove_cycles();
    void dfs_cycle_detection(const std::string& node_id, std::set<std::string>& visited,
                             std::set<std::string>& rec_stack, std::vector<GraphEdge*>& to_reverse);

    // Phase 2: Layer assignment using longest path
    void assign_ranks();
    int compute_rank(const std::string& node_id, std::map<std::string, int>& memo);

    // Phase 3: Crossing minimization using barycenter heuristic
    void minimize_crossings(int max_iterations = 24);
    float barycenter(const std::string& node_id, bool use_predecessors);
    int count_crossings();

    // Phase 4: Coordinate assignment
    void assign_coordinates();
    void position_nodes_in_rank(int rank_idx);

    // Edge routing
    void route_edges(LayoutData& data);
    std::vector<std::pair<float, float>> route_edge(const GraphEdge& edge);

    // Utility functions
    void build_graph(const UnifiedDiagram& diagram, const Theme& theme);
    void clear();
    std::pair<float, float> calculate_node_size(const std::string& label, NodeShape shape, const Theme& theme);
};

} // namespace flexmaid
} // namespace modules
} // namespace flex
