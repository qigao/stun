#include <flexmaid/layout/dagre_layout.h>
#include <flexmaid/flexmaid.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <numeric>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace flex {
namespace modules {
namespace flexmaid {

DagreLayoutEngine::DagreLayoutEngine(const Config& config) : config_(config) {}

void DagreLayoutEngine::clear() {
    nodes_.clear();
    edges_.clear();
    ranks_.clear();
    size_cache_.clear(); // Clear performance cache
}

std::pair<float, float> DagreLayoutEngine::calculate_node_size(
    const std::string& label, NodeShape shape, const Theme& theme) {
    
    // Check cache first
    auto cache_key = std::make_pair(label, shape);
    auto cache_it = size_cache_.find(cache_key);
    if (cache_it != size_cache_.end()) {
        return cache_it->second;
    }
    
    // Calculate size
    float tw = label.length() * theme.font_size * 0.6f;
    float th = theme.font_size * 1.2f;
    float w = std::max(80.0f, tw + 20.0f);
    float h = std::max(40.0f, th + 20.0f);
    
    // Adjust for special shapes
    if (shape == NodeShape::Circle || shape == NodeShape::CircleFilled) {
        float r = std::max(w, h) / 2.0f;
        w = h = r * 2.0f;
    } else if (shape == NodeShape::Diamond) {
        w *= 1.4f;
        h *= 1.4f;
    }
    
    // Cache and return
    auto result = std::make_pair(w, h);
    size_cache_[cache_key] = result;
    return result;
}

void DagreLayoutEngine::build_graph(const UnifiedDiagram& diagram, const Theme& theme) {
    clear();
    
    // Performance: Reserve memory upfront for vectors (maps don't support reserve)
    edges_.reserve(diagram.edges.size());
    
    bool is_architecture = (diagram.type == DiagramType::Architecture);
    
    // Create nodes
    for (const auto& [id, node] : diagram.nodes) {
        GraphNode gn;
        gn.id = id;
        float w, h;
        if (is_architecture) {
            w = h = 60.0f;
        } else {
            auto [nw, nh] = calculate_node_size(node.label, node.shape, theme);
            w = nw;
            h = nh;
        }
        gn.width = w;
        gn.height = h;
        nodes_[node.id] = gn;
    }
    
    // Create edges
    for (const auto& edge : diagram.edges) {
        if (nodes_.count(edge.from) && nodes_.count(edge.to)) {
            GraphEdge ge;
            ge.from = edge.from;
            ge.to = edge.to;
            edges_.push_back(ge);
            
            nodes_[edge.from].out_edges.push_back(edge.to);
            nodes_[edge.to].in_edges.push_back(edge.from);
        }
    }
}

// Phase 1: Cycle Removal
void DagreLayoutEngine::remove_cycles() {
    std::set<std::string> visited;
    std::set<std::string> rec_stack;
    std::vector<GraphEdge*> to_reverse;
    
    for (auto& [id, node] : nodes_) {
        if (visited.find(id) == visited.end()) {
            dfs_cycle_detection(id, visited, rec_stack, to_reverse);
        }
    }
    
    // Reverse edges that create cycles
    for (auto* edge : to_reverse) {
        std::swap(edge->from, edge->to);
        edge->reversed = true;
        
        // Update adjacency lists
        auto& from_node = nodes_[edge->from];
        auto& to_node = nodes_[edge->to];
        
        from_node.out_edges.erase(
            std::remove(from_node.out_edges.begin(), from_node.out_edges.end(), edge->to),
            from_node.out_edges.end());
        to_node.in_edges.erase(
            std::remove(to_node.in_edges.begin(), to_node.in_edges.end(), edge->from),
            to_node.in_edges.end());
        
        from_node.out_edges.push_back(edge->to);
        to_node.in_edges.push_back(edge->from);
    }
}

void DagreLayoutEngine::dfs_cycle_detection(
    const std::string& node_id,
    std::set<std::string>& visited,
    std::set<std::string>& rec_stack,
    std::vector<GraphEdge*>& to_reverse) {
    
    visited.insert(node_id);
    rec_stack.insert(node_id);
    
    for (const auto& neighbor : nodes_[node_id].out_edges) {
        if (visited.find(neighbor) == visited.end()) {
            dfs_cycle_detection(neighbor, visited, rec_stack, to_reverse);
        } else if (rec_stack.find(neighbor) != rec_stack.end()) {
            // Found a back edge - mark it for reversal
            for (auto& edge : edges_) {
                if (edge.from == node_id && edge.to == neighbor) {
                    to_reverse.push_back(&edge);
                    break;
                }
            }
        }
    }
    
    rec_stack.erase(node_id);
}

// Phase 2: Layer Assignment
void DagreLayoutEngine::assign_ranks() {
    std::map<std::string, int> memo;
    
    // Compute rank for each node (longest path from sources)
    for (auto& [id, node] : nodes_) {
        node.rank = compute_rank(id, memo);
    }
    
    // Group nodes by rank
    int max_rank = 0;
    for (const auto& [id, node] : nodes_) {
        max_rank = std::max(max_rank, node.rank);
    }
    
    ranks_.resize(max_rank + 1);
    for (const auto& [id, node] : nodes_) {
        ranks_[node.rank].push_back(id);
    }
    
    // Insert dummy nodes for edges spanning multiple ranks
    std::vector<GraphEdge> new_edges;
    for (auto& edge : edges_) {
        int from_rank = nodes_[edge.from].rank;
        int to_rank = nodes_[edge.to].rank;
        int span = to_rank - from_rank;
        
        if (span > 1) {
            // Create dummy nodes
            std::string prev = edge.from;
            for (int r = from_rank + 1; r < to_rank; ++r) {
                std::string dummy_id = edge.from + "_to_" + edge.to + "_dummy_" + std::to_string(r);
                
                GraphNode dummy;
                dummy.id = dummy_id;
                dummy.width = 10;
                dummy.height = 10;
                dummy.rank = r;
                nodes_[dummy_id] = dummy;
                ranks_[r].push_back(dummy_id);
                edge.dummy_nodes.push_back(dummy_id);
                
                GraphEdge seg;
                seg.from = prev;
                seg.to = dummy_id;
                new_edges.push_back(seg);
                
                prev = dummy_id;
            }
            
            // Final segment to actual target
            GraphEdge final_seg;
            final_seg.from = prev;
            final_seg.to = edge.to;
            new_edges.push_back(final_seg);
        } else {
            new_edges.push_back(edge);
        }
    }
    
    edges_ = std::move(new_edges);
}

int DagreLayoutEngine::compute_rank(const std::string& node_id, std::map<std::string, int>& memo) {
    if (memo.count(node_id)) {
        return memo[node_id];
    }
    
    // Safety check: node must exist
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        memo[node_id] = 0;
        return 0;
    }
    
    const auto& node = it->second;
    if (node.in_edges.empty()) {
        memo[node_id] = 0;
        return 0;
    }
    
    int max_pred_rank = -1;
    for (const auto& pred : node.in_edges) {
        // Safety check: predecessor must exist
        if (nodes_.find(pred) == nodes_.end()) {
            continue;
        }
        
        int pred_rank = compute_rank(pred, memo);
        max_pred_rank = std::max(max_pred_rank, pred_rank);
    }
    
    int rank = max_pred_rank + 1;
    memo[node_id] = rank;
    return rank;
}

// Phase 3: Crossing Minimization
void DagreLayoutEngine::minimize_crossings(int max_iterations) {
    // Initialize order within each rank
    for (int r = 0; r < (int)ranks_.size(); ++r) {
        for (int i = 0; i < (int)ranks_[r].size(); ++i) {
            nodes_[ranks_[r][i]].order = i;
        }
    }
    
    // Iteratively improve using barycenter heuristic
    for (int iter = 0; iter < max_iterations; ++iter) {
        bool changed = false;
        
        // Sweep down
        for (int r = 1; r < (int)ranks_.size(); ++r) {
            std::vector<std::pair<float, std::string>> order_pairs;
            for (const auto& id : ranks_[r]) {
                float bc = barycenter(id, true); // Use predecessors
                order_pairs.push_back({bc, id});
            }
            
            std::sort(order_pairs.begin(), order_pairs.end());
            
            for (int i = 0; i < (int)order_pairs.size(); ++i) {
                if (nodes_[order_pairs[i].second].order != i) {
                    changed = true;
                }
                nodes_[order_pairs[i].second].order = i;
                ranks_[r][i] = order_pairs[i].second;
            }
        }
        
        // Sweep up
        for (int r = (int)ranks_.size() - 2; r >= 0; --r) {
            std::vector<std::pair<float, std::string>> order_pairs;
            for (const auto& id : ranks_[r]) {
                float bc = barycenter(id, false); // Use successors
                order_pairs.push_back({bc, id});
            }
            
            std::sort(order_pairs.begin(), order_pairs.end());
            
            for (int i = 0; i < (int)order_pairs.size(); ++i) {
                if (nodes_[order_pairs[i].second].order != i) {
                    changed = true;
                }
                nodes_[order_pairs[i].second].order = i;
                ranks_[r][i] = order_pairs[i].second;
            }
        }
        
        if (!changed) break;
    }
}

float DagreLayoutEngine::barycenter(const std::string& node_id, bool use_predecessors) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return 0.0f;
    }
    
    const auto& node = it->second;
    const auto& neighbors = use_predecessors ? node.in_edges : node.out_edges;
    
    if (neighbors.empty()) {
        return (float)node.order;
    }
    
    float sum = 0;
    int count = 0;
    for (const auto& neighbor : neighbors) {
        auto neighbor_it = nodes_.find(neighbor);
        if (neighbor_it != nodes_.end()) {
            sum += neighbor_it->second.order;
            count++;
        }
    }
    
    if (count == 0) {
        return (float)node.order;
    }
    
    return sum / count;
}

// Phase 4: Coordinate Assignment
void DagreLayoutEngine::assign_coordinates() {
    // Assign Y coordinates based on ranks
    float current_y = config_.margin_y;
    std::vector<float> rank_heights(ranks_.size());
    
    for (int r = 0; r < (int)ranks_.size(); ++r) {
        float max_height = 0;
        for (const auto& id : ranks_[r]) {
            max_height = std::max(max_height, nodes_[id].height);
        }
        rank_heights[r] = max_height;
        
        for (const auto& id : ranks_[r]) {
            nodes_[id].y = current_y + max_height / 2;
        }
        
        current_y += max_height + config_.rank_spacing;
    }
    
    // Assign X coordinates based on order
    for (int r = 0; r < (int)ranks_.size(); ++r) {
        position_nodes_in_rank(r);
    }
}

void DagreLayoutEngine::position_nodes_in_rank(int rank_idx) {
    const auto& rank = ranks_[rank_idx];
    
    if (rank.empty()) return;
    
    // Calculate total width needed with adaptive spacing
    float total_width = 0;
    std::vector<float> node_widths;
    
    for (const auto& id : rank) {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) {
            node_widths.push_back(it->second.width);
            total_width += it->second.width;
        }
    }
    
    // Adaptive spacing: larger for fewer nodes, smaller for many nodes
    float base_spacing = config_.node_spacing;
    float adaptive_spacing = base_spacing;
    
    if (rank.size() > 5) {
        // Reduce spacing for crowded ranks
        adaptive_spacing = base_spacing * 0.7f;
    } else if (rank.size() <= 2) {
        // Increase spacing for sparse ranks
        adaptive_spacing = base_spacing * 1.5f;
    }
    
    total_width += (rank.size() - 1) * adaptive_spacing;
    
    // Center the rank
    float current_x = config_.margin_x;
    
    for (size_t i = 0; i < rank.size(); ++i) {
        auto it = nodes_.find(rank[i]);
        if (it != nodes_.end()) {
            auto& node = it->second;
            node.x = current_x;
            current_x += node.width + adaptive_spacing;
        }
    }
}

void DagreLayoutEngine::route_edges(LayoutData& data) {
    for (const auto& edge : edges_) {
        auto points = route_edge(edge);
        
        LayoutData::Path path;
        path.points = points;
        data.edge_paths.push_back(path);
    }
}

std::vector<std::pair<float, float>> DagreLayoutEngine::route_edge(const GraphEdge& edge) {
    std::vector<std::pair<float, float>> points;
    
    auto from_it = nodes_.find(edge.from);
    auto to_it = nodes_.find(edge.to);
    
    if (from_it == nodes_.end() || to_it == nodes_.end()) {
        return points; // Empty path for invalid edges
    }
    
    const auto& from_node = from_it->second;
    const auto& to_node = to_it->second;
    
    // Calculate connection points (center of nodes)
    float from_cx = from_node.x;
    float from_cy = from_node.y;
    float to_cx = to_node.x;
    float to_cy = to_node.y;
    
    // EDGE CLIPPING: Calculate intersection with node boundaries
    auto clip_point = [](float x, float y, float target_x, float target_y, float w, float h) {
        float dx = target_x - x;
        float dy = target_y - y;
        
        if (std::abs(dx) < 0.001f && std::abs(dy) < 0.001f) return std::make_pair(x, y);
        
        float scale_x = (dx != 0) ? (w / 2.0f) / std::abs(dx) : 1e9f;
        float scale_y = (dy != 0) ? (h / 2.0f) / std::abs(dy) : 1e9f;
        float scale = std::min(scale_x, scale_y);
        
        return std::make_pair(x + dx * scale, y + dy * scale);
    };

    auto [start_x, start_y] = clip_point(from_cx, from_cy, to_cx, to_cy, from_node.width, from_node.height);
    auto [end_x, end_y] = clip_point(to_cx, to_cy, from_cx, from_cy, to_node.width, to_node.height);

    // Always use TB routing internally - direction transform happens after
    float mid_y = (start_y + end_y) / 2;
    points.push_back({start_x, start_y});
    if (std::abs(start_x - end_x) > 5.0f) {
        points.push_back({start_x, mid_y});
        points.push_back({end_x, mid_y});
    }
    points.push_back({end_x, end_y});
    
    return points;
}

void DagreLayoutEngine::layout_flowchart(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Update config from diagram properties
    config_.direction = diagram.get_prop("direction", "TB");
    
    // Build internal graph
    build_graph(diagram, theme);
    
    if (nodes_.empty()) {
        data.width = 200;
        data.height = 100;
        return;
    }
    
    // Run Sugiyama algorithm (internally always TB)
    remove_cycles();
    assign_ranks();
    minimize_crossings();
    assign_coordinates();
    
    // DIR TRANSFORM: Handle non-TB directions
    bool is_horizontal = (config_.direction == "LR");
    
    // Copy results to LayoutData
    for (const auto& [id, node] : nodes_) {
        // Skip dummy nodes
        if (id.find("_dummy_") != std::string::npos) continue;
        
        float final_x = node.x;
        float final_y = node.y;
        float final_w = node.width;
        float final_h = node.height;
        
        if (is_horizontal) {
            std::swap(final_x, final_y);
            std::swap(final_w, final_h);
        }
        
        data.node_bounds[id] = {final_x, final_y, final_w, final_h};
    }
    
    // Route edges - matching diagram edges to internal edges
    size_t edge_idx = 0;
    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        
        // Find the corresponding GraphEdge (they are added in the same order in build_graph)
        if (edge_idx < edges_.size()) {
            path.points = route_edge(edges_[edge_idx]);
            
            // Apply coordinate swap for LR direction
            if (is_horizontal) {
                for (auto& p : path.points) {
                    std::swap(p.first, p.second);
                }
            }
        }
        
        data.edge_paths.push_back(path);
        edge_idx++;
    }
    
    // Calculate canvas size
    float max_x = 0, max_y = 0;
    for (const auto& [id, bounds] : data.node_bounds) {
        max_x = std::max(max_x, bounds.right());
        max_y = std::max(max_y, bounds.bottom());
    }
    
    data.width = max_x + config_.margin_x;
    data.height = max_y + config_.margin_y;
}

void DagreLayoutEngine::layout_sequence(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    const float SPACING_X = 200, SPACING_Y = 60, TOP_MARGIN = 50;
    
    std::vector<std::string> participants;
    std::vector<std::string> notes;
    
    for (const auto& [id, node] : diagram.nodes) {
        if (node.get_prop("is_note") == "true") {
            notes.push_back(id);
        } else {
            participants.push_back(id);
        }
    }
    
    // 1. Layout participants in a row
    for (size_t i = 0; i < participants.size(); ++i) {
        const auto& id = participants[i];
        const auto* node = diagram.find_node(id);
        float tw = node->label.length() * theme.font_size * 0.6f;
        float w = std::max(80.0f, tw + 20);
        float h = 40;
        data.node_bounds[id] = {(float)i * SPACING_X + SPACING_X / 2, TOP_MARGIN, w, h};
    }

    // 2. Layout messages (edges) vertically
    float current_y = TOP_MARGIN + 60;
    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, current_y}, {to_it->second.x, current_y}};
            
            // Check if there's a note for this message/timeline? (Simplification)
            current_y += SPACING_Y;
        }
        data.edge_paths.push_back(path);
    }
    
    // 3. Layout notes near their targets
    for (const auto& note_id : notes) {
        const auto* node = diagram.find_node(note_id);
        std::string target = node->get_prop("target");
        std::string dir = node->get_prop("direction");
        
        float tw = node->label.length() * theme.font_size * 0.6f;
        float w = std::max(120.0f, tw + 20);
        float h = std::max(40.0f, (float)theme.font_size * 2.0f);
        
        float x = SPACING_X / 2, y = current_y;
        if (auto it = data.node_bounds.find(target); it != data.node_bounds.end()) {
            x = it->second.x;
            if (dir == "left") x -= (it->second.width / 2 + w / 2 + 20);
            else if (dir == "right") x += (it->second.width / 2 + w / 2 + 20);
            
            // Try to place it at some y level. For now, just at the end or halfway
            y = TOP_MARGIN + 100 + (rand() % 200); 
        }
        
        data.node_bounds[note_id] = {x, y, w, h};
    }

    data.width = std::max(400.0f, (float)participants.size() * SPACING_X);
    data.height = std::max(300.0f, current_y + 100);
}

void DagreLayoutEngine::layout_class(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Use DAG layout for class diagrams too
    layout_flowchart(diagram, data, theme);
}

void DagreLayoutEngine::layout_state(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Use simple grid layout for state diagrams to avoid issues with special syntax
    const float SPACING_X = 200, SPACING_Y = 150;
    
    int cols = std::max(1, (int)std::ceil(std::sqrt(diagram.nodes.size())));
    
    int index = 0;
    for (const auto& [id, node] : diagram.nodes) {
        int row = index / cols;
        int col = index % cols;
        
        auto [w, h] = calculate_node_size(node.label, node.shape, theme);
        
        float x = col * SPACING_X + SPACING_X / 2;
        float y = row * SPACING_Y + SPACING_Y / 2;
        
        data.node_bounds[id] = {x, y, w, h};
        index++;
    }
    
    // Simple edge routing
    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, from_it->second.y}, 
                          {to_it->second.x, to_it->second.y}};
        }
        data.edge_paths.push_back(path);
    }
    
    int rows = (int)std::ceil((float)diagram.nodes.size() / cols);
    data.width = cols * SPACING_X + config_.margin_x * 2;
    data.height = rows * SPACING_Y + config_.margin_y * 2;
}

void DagreLayoutEngine::layout_er(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Use DAG layout for ER diagrams
    layout_flowchart(diagram, data, theme);
}

void DagreLayoutEngine::layout_block(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Block diagrams use flowchart layout - direction is already set to "LR" in create_diagram()
    layout_flowchart(diagram, data, theme);
}

} // namespace flexmaid
} // namespace modules
} // namespace flex
