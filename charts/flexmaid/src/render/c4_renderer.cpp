#include "render/c4_renderer.h"
#include "layout/edge_router.h"
#include <flexmaid.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace flex::modules::flexmaid {

namespace {
std::pair<float, float> c4_node_size(const std::string& type) {
    if (type == "Person") return {80, 100};
    if (type == "Db" || type == "Database") return {100, 80};
    return {140, 80};
}
}

LayoutData C4Renderer::do_layout(const UnifiedDiagram& diagram, const Theme& theme) {
    const float MARGIN = 40.0f, NODE_SPACING = 60.0f, BOUNDARY_PADDING = 30.0f;
    LayoutData data;
    std::map<std::string, std::string> node_to_boundary;
    
    float current_y = MARGIN;
    std::vector<std::string> root_nodes;
    for (const auto& [id, node] : diagram.nodes) {
        bool in_sub = false;
        for (const auto& sub : diagram.subgraphs) {
            for (const auto& nid : sub.node_ids) { if (nid == id) { in_sub = true; break; } }
            if (in_sub) { node_to_boundary[id] = sub.id; break; }
        }
        if (!in_sub) root_nodes.push_back(id);
    }
    
    float root_x = MARGIN;
    float max_root_h = 0.0f;
    for (const auto& id : root_nodes) {
        const auto* n = diagram.find_node(id);
        if (!n) continue;
        auto [w, h] = c4_node_size(n->get_prop("c4_type", "System"));
        data.node_bounds[id] = {root_x + w / 2, current_y + h / 2, w, h};
        root_x += w + NODE_SPACING;
        max_root_h = std::max(max_root_h, h);
    }
    if (!root_nodes.empty()) current_y += max_root_h + NODE_SPACING;
    
    for (const auto& sub : diagram.subgraphs) {
        if (sub.node_ids.empty()) continue;
        float inner_x = MARGIN + BOUNDARY_PADDING;
        float inner_y = current_y + 30 + BOUNDARY_PADDING;
        float max_h = 0.0f;
        for (const auto& nid : sub.node_ids) {
            const auto* n = diagram.find_node(nid);
            if (!n) continue;
            auto [w, h] = c4_node_size(n->get_prop("c4_type", "System"));
            data.node_bounds[nid] = {inner_x + w / 2, inner_y + h / 2, w, h};
            inner_x += w + NODE_SPACING;
            max_h = std::max(max_h, h);
        }
        current_y += 30 + BOUNDARY_PADDING * 2 + max_h + NODE_SPACING;
    }
    
    data.width = root_x + MARGIN;
    data.height = current_y + MARGIN;
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

MustacheNodeData C4Renderer::build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme) {
    MustacheNodeData n = ChartRenderer::build_node_data(node, bounds, diagram, theme);
    std::string type = node.get_prop("c4_type", "System");
    if (type == "Person") n.primary_color = "#08427B";
    else if (type == "Db" || type == "Database") { n.primary_color = "#438DD5"; n.shape_cylinder = true; n.shape_rect = false; }
    n.text_color = "#ffffff";
    return n;
}

} // namespace flex::modules::flexmaid
