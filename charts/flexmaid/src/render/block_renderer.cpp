#include "render/block_renderer.h"
#include "layout/edge_router.h"
#include <flexmaid.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <string>

namespace flex::modules::flexmaid {

namespace {

struct NodeLayout {
    std::string id;
    float x, y, width, height;
    int rank = 0;
    int order = 0;
    std::vector<std::string> in_edges;
    std::vector<std::string> out_edges;
};

struct LayoutConfig {
    float margin = 40.0f;
    float rank_spacing = 150.0f;
    float node_spacing = 60.0f;
};

std::pair<float, float> calc_node_size(const std::string& label, float font_size) {
    float tw = (float)label.length() * font_size * 0.6f;
    float w = std::max(100.0f, tw + 40.0f);
    float h = std::max(50.0f, font_size * 1.5f + 20.0f);
    return {w, h};
}

int compute_rank(const std::string& id, std::map<std::string, NodeLayout>& nodes, std::map<std::string, int>& memo) {
    if (memo.count(id)) return memo[id];
    auto& node = nodes[id];
    if (node.in_edges.empty()) return memo[id] = 0;
    int max_rank = -1;
    for (const auto& pred : node.in_edges) max_rank = std::max(max_rank, compute_rank(pred, nodes, memo));
    return memo[id] = max_rank + 1;
}

} // namespace

LayoutData BlockRenderer::do_layout(const UnifiedDiagram& diagram, const Theme& theme) {
    LayoutConfig config;
    std::map<std::string, NodeLayout> nodes;
    
    // 1. Initialize nodes with sizes
    for (const auto& [id, node] : diagram.nodes) {
        NodeLayout nl;
        nl.id = id;
        auto [w, h] = calc_node_size(node.label, (float)theme.font_size);
        nl.width = w; nl.height = h;
        nodes[id] = nl;
    }
    
    // 2. Build adjacency for ranking
    for (const auto& edge : diagram.edges) {
        if (nodes.count(edge.from) && nodes.count(edge.to)) {
            nodes[edge.from].out_edges.push_back(edge.to);
            nodes[edge.to].in_edges.push_back(edge.from);
        }
    }
    
    // 3. Compute ranks
    std::map<std::string, int> memo;
    int max_rank = 0;
    for (auto& [id, node] : nodes) {
        node.rank = compute_rank(id, nodes, memo);
        max_rank = std::max(max_rank, node.rank);
    }
    
    // 4. Assign coordinates
    std::vector<std::vector<std::string>> ranks(max_rank + 1);
    for (const auto& [id, node] : nodes) ranks[node.rank].push_back(id);
    
    LayoutData data;
    for (int r = 0; r <= max_rank; ++r) {
        float x = config.margin + r * config.rank_spacing;
        float y = config.margin;
        for (const auto& id : ranks[r]) {
            auto& nl = nodes[id];
            nl.x = x + nl.width / 2;
            nl.y = y + nl.height / 2;
            data.node_bounds[id] = {nl.x, nl.y, nl.width, nl.height};
            y += nl.height + config.node_spacing;
        }
    }
    
    // 5. Build edge paths with orthogonal routing
    for (const auto& edge : diagram.edges) {
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            auto points = EdgeRouter::route(from_it->second, to_it->second);
            LayoutData::Path path;
            path.points = points;
            data.edge_paths.push_back(path);
        }
    }

    // 6. Canvas dimensions
    for (const auto& [id, b] : data.node_bounds) {
        data.width = std::max(data.width, b.right() + config.margin);
        data.height = std::max(data.height, b.bottom() + config.margin);
    }
    
    return data;
}

} // namespace flex::modules::flexmaid
