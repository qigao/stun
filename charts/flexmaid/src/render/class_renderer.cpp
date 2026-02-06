#include "render/class_renderer.h"
#include "layout/edge_router.h"
#include <flexmaid.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace flex::modules::flexmaid {

namespace {

struct ClassBox {
    std::string id, name;
    std::vector<std::string> attributes;
    std::vector<std::string> methods;
    float x, y, width, height;
    int rank = 0;
    std::vector<std::string> in_edges, out_edges;
};

void parse_class_members(const Node& node, ClassBox& cb) {
    cb.name = node.label;
    for (const auto& [key, value] : node.props) {
        if (key.find("attr_") == 0) cb.attributes.push_back(value);
        else if (key.find("method_") == 0) cb.methods.push_back(value);
    }
}

void calc_class_size(ClassBox& cb, float font_size) {
    float line_height = font_size * 1.5f;
    float padding = 10.0f;
    float section_min_height = line_height;
    
    float max_width = cb.name.length() * font_size * 0.6f;
    for (const auto& attr : cb.attributes) max_width = std::max(max_width, (float)attr.length() * font_size * 0.5f);
    for (const auto& method : cb.methods) max_width = std::max(max_width, (float)method.length() * font_size * 0.5f);
    cb.width = std::max(120.0f, max_width + padding * 2);
    
    float name_section = line_height + padding;
    float attr_section = std::max(section_min_height, (float)cb.attributes.size() * line_height * 0.8f + padding);
    float method_section = std::max(section_min_height, (float)cb.methods.size() * line_height * 0.8f + padding);
    cb.height = name_section + attr_section + method_section;
}

int compute_rank(const std::string& id, std::map<std::string, ClassBox>& classes,
                 std::map<std::string, int>& memo, std::set<std::string>& visiting) {
    if (memo.count(id)) return memo[id];
    if (visiting.count(id)) { memo[id] = 0; return 0; }
    if (!classes.count(id)) { memo[id] = 0; return 0; }
    
    visiting.insert(id);
    auto& cb = classes[id];
    if (cb.in_edges.empty()) { memo[id] = 0; visiting.erase(id); return 0; }
    
    int max_rank = -1;
    for (const auto& pred : cb.in_edges) {
        if (classes.count(pred)) max_rank = std::max(max_rank, compute_rank(pred, classes, memo, visiting));
    }
    visiting.erase(id);
    memo[id] = max_rank + 1;
    return memo[id];
}

} // namespace

LayoutData ClassRenderer::do_layout(const UnifiedDiagram& diagram, const Theme& theme) {
    const float MARGIN = 50.0f, RANK_SPACING = 200.0f, NODE_SPACING = 80.0f;
    LayoutData data;
    std::map<std::string, ClassBox> boxes;
    
    for (const auto& [id, node] : diagram.nodes) {
        if (id == "1" || id == "0" || id == "*" || id == "many" || id == "n") continue;
        ClassBox cb; cb.id = id;
        parse_class_members(node, cb);
        calc_class_size(cb, (float)theme.font_size);
        boxes[id] = cb;
    }
    for (const auto& edge : diagram.edges) {
        if (boxes.count(edge.from) && boxes.count(edge.to)) {
            boxes[edge.from].out_edges.push_back(edge.to);
            boxes[edge.to].in_edges.push_back(edge.from);
        }
    }
    
    std::map<std::string, int> memo;
    std::set<std::string> visiting;
    int max_rank = 0;
    for (auto& [id, cb] : boxes) {
        cb.rank = compute_rank(id, boxes, memo, visiting);
        max_rank = std::max(max_rank, cb.rank);
    }
    
    std::vector<std::vector<std::string>> ranks(max_rank + 1);
    for (const auto& [id, cb] : boxes) ranks[cb.rank].push_back(id);
    
    float canvas_w = 0, canvas_h = 0;
    for (int r = 0; r <= max_rank; ++r) {
        // Parent classes (rank 0) at top, children below
        float y = MARGIN + r * RANK_SPACING;
        float x = MARGIN;
        for (const auto& id : ranks[r]) {
            auto& cb = boxes[id];
            cb.x = x + cb.width / 2; cb.y = y + cb.height / 2;
            data.node_bounds[id] = {cb.x, cb.y, cb.width, cb.height};
            x += cb.width + NODE_SPACING;
            canvas_w = std::max(canvas_w, x + MARGIN);
            canvas_h = std::max(canvas_h, y + cb.height + MARGIN);
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

MustacheNodeData ClassRenderer::build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme) {
    MustacheNodeData n = ChartRenderer::build_node_data(node, bounds, diagram, theme);
    
    // Mermaid official class diagram colors
    n.primary_color = "#ECECFF";
    n.text_color = "#333333";
    
    const float fs = (float)theme.font_size;
    const float LINE_HEIGHT = fs * 1.5f;
    
    n.has_rows = true; n.has_methods = true; n.has_sep2 = true;
    n.text_y_node = n.rect_y + LINE_HEIGHT * 0.65f;
    n.sep_y = n.rect_y + LINE_HEIGHT;
    
    std::vector<std::string> attrs, methods;
    for (const auto& [key, value] : node.props) {
        if (key.find("attr_") == 0) attrs.push_back(value);
        else if (key.find("method_") == 0) methods.push_back(value);
    }
    
    float current_y = n.sep_y + LINE_HEIGHT * 0.8f;
    for (const auto& attr : attrs) {
        MustacheAttrData r;
        r.name = attr;
        r.row_y = current_y;
        r.row_h = LINE_HEIGHT * 0.8f;
        r.ty = current_y + r.row_h * 0.5f;
        n.rows.push_back(r);
        current_y += r.row_h;
    }
    if (attrs.empty()) current_y += LINE_HEIGHT * 0.5f;
    
    n.sep2_y = current_y;
    current_y += LINE_HEIGHT * 0.8f;
    for (const auto& method : methods) {
        MustacheAttrData m;
        m.name = method;
        m.row_y = current_y;
        m.row_h = LINE_HEIGHT * 0.8f;
        m.ty = current_y + m.row_h * 0.5f;
        n.methods.push_back(m);
        current_y += m.row_h;
    }
    
    return n;
}

} // namespace flex::modules::flexmaid
