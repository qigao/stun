#include "render/flowchart_renderer.h"
#include "layout/edge_router.h"
#include <flexmaid.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>

namespace flex::modules::flexmaid {

namespace {
std::pair<float, float> calc_node_size(const std::string& label, NodeShape shape, float font_size) {
    float tw = (float)label.length() * font_size * 0.6f;
    float w = std::max(80.0f, tw + 20.0f);
    float h = std::max(40.0f, font_size * 1.2f + 20.0f);
    if (shape == NodeShape::Circle || shape == NodeShape::CircleFilled) { float r = std::max(w, h) / 2.0f; w = h = r * 2.0f; }
    else if (shape == NodeShape::Diamond) { w *= 1.4f; h *= 1.4f; }
    return {w, h};
}

int compute_rank(const std::string& id, std::map<std::string, std::vector<std::string>>& in_edges, std::map<std::string, int>& memo) {
    if (memo.count(id)) return memo[id];
    if (in_edges[id].empty()) return memo[id] = 0;
    int max_rank = -1;
    for (const auto& pred : in_edges[id]) max_rank = std::max(max_rank, compute_rank(pred, in_edges, memo));
    return memo[id] = max_rank + 1;
}
}

LayoutData FlowchartRenderer::do_layout(const UnifiedDiagram& diagram, const Theme& theme) {
    const float MARGIN = 40.0f, RANK_SPACING = 120.0f, NODE_SPACING = 80.0f;
    LayoutData data;
    std::string direction = diagram.get_prop("direction", "TD");
    bool is_horizontal = (direction == "LR");
    
    std::map<std::string, std::pair<float, float>> sizes;
    std::map<std::string, std::vector<std::string>> in_edges;
    for (const auto& [id, node] : diagram.nodes) {
        sizes[id] = calc_node_size(node.label, node.shape, (float)theme.font_size);
        in_edges[id] = {};
    }
    for (const auto& edge : diagram.edges) {
        if (in_edges.count(edge.to)) in_edges[edge.to].push_back(edge.from);
    }
    
    std::map<std::string, int> memo;
    int max_rank = 0;
    for (const auto& [id, node] : diagram.nodes) max_rank = std::max(max_rank, compute_rank(id, in_edges, memo));
    
    std::vector<std::vector<std::string>> ranks(max_rank + 1);
    for (const auto& [id, node] : diagram.nodes) ranks[memo[id]].push_back(id);
    
    float canvas_w = 0, canvas_h = 0;
    for (int r = 0; r <= max_rank; ++r) {
        float primary = MARGIN + r * RANK_SPACING;
        float secondary = MARGIN;
        for (const auto& id : ranks[r]) {
            auto [w, h] = sizes[id];
            if (is_horizontal) {
                data.node_bounds[id] = {primary + w / 2, secondary + h / 2, w, h};
                secondary += h + NODE_SPACING;
            } else {
                data.node_bounds[id] = {secondary + w / 2, primary + h / 2, w, h};
                secondary += w + NODE_SPACING;
            }
            canvas_w = std::max(canvas_w, data.node_bounds[id].right() + MARGIN);
            canvas_h = std::max(canvas_h, data.node_bounds[id].bottom() + MARGIN);
        }
    }
    
    data.width = canvas_w; data.height = canvas_h;
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
    return data;
}

} // namespace flex::modules::flexmaid
