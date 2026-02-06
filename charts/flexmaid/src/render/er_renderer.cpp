#include "render/er_renderer.h"
#include "layout/edge_router.h"
#include <flexmaid.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace flex::modules::flexmaid {

namespace {
float tw(const std::string& t, float fs) { return t.length() * fs * 0.55f; }
}

LayoutData ERRenderer::do_layout(const UnifiedDiagram& diagram, const Theme& theme) {
    const float MARGIN = 30, SPACING = 80, fs = (float)theme.font_size;
    LayoutData data;
    
    struct TmpEnt { 
        std::string id; 
        float w, h, type_col, row_h, name_h; 
        std::vector<std::pair<std::string,std::string>> attrs; 
    };
    std::vector<TmpEnt> tmps;
    
    for (const auto& [id, n] : diagram.nodes) {
        TmpEnt t;
        t.id = id;
        t.type_col = 40;
        float name_col = 40;
        
        for (const auto& [k, v] : n.props) {
            if (k.find("attr_") == 0) {
                std::string type, name;
                size_t sp = v.find(' ');
                if (sp != std::string::npos) { type = v.substr(0, sp); name = v.substr(sp + 1); }
                else { name = v; }
                t.type_col = std::max(t.type_col, tw(type, fs) + 16);
                name_col = std::max(name_col, tw(name, fs) + 16);
                t.attrs.push_back({type, name});
            }
        }
        
        t.row_h = fs * 2.0f;
        t.name_h = t.row_h * 1.3f;
        float name_w = tw(n.label, fs) + 24;
        t.w = std::max(name_w, t.type_col + name_col);
        t.w = std::max(t.w, 120.0f);
        t.h = t.name_h + std::max((float)t.attrs.size(), 1.0f) * t.row_h;
        tmps.push_back(t);
    }
    
    float max_w = 0;
    for (auto& t : tmps) max_w = std::max(max_w, t.w);
    
    float current_y = MARGIN;
    for (auto& t : tmps) {
        data.node_bounds[t.id] = {MARGIN + max_w / 2, current_y + t.h / 2, t.w, t.h};
        current_y += t.h + SPACING;
    }
    
    data.width = max_w + MARGIN * 2;
    data.height = current_y - SPACING + MARGIN;
    
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

MustacheNodeData ERRenderer::build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme) {
    MustacheNodeData n = ChartRenderer::build_node_data(node, bounds, diagram, theme);
    
    const float fs = (float)theme.font_size;
    const std::string ODD = "#FFFFFF", EVEN = "#F8F8FF";
    
    n.has_rows = true;
    float row_h = fs * 2.0f;
    float name_h = row_h * 1.3f;
    n.sep_y = n.rect_y + name_h;
    n.text_y_node = n.rect_y + name_h * 0.65f;
    
    // Recalculate type_col for alignment (ideally this should be in do_layout/props)
    float type_col = 40;
    std::vector<std::pair<std::string,std::string>> attrs;
    for (const auto& [k, v] : node.props) {
        if (k.find("attr_") == 0) {
            std::string type, name;
            size_t sp = v.find(' ');
            if (sp != std::string::npos) { type = v.substr(0, sp); name = v.substr(sp + 1); }
            else { name = v; }
            type_col = std::max(type_col, tw(type, fs) + 16);
            attrs.push_back({type, name});
        }
    }
    
    float current_row_y = n.sep_y;
    for (size_t j = 0; j < attrs.size(); j++) {
        MustacheAttrData r;
        r.type = attrs[j].first;
        r.name = attrs[j].second;
        r.row_bg = (j % 2 == 0) ? ODD : EVEN;
        r.row_y = current_row_y;
        r.row_h = row_h;
        r.col_div = n.rect_x + type_col;
        r.ty = current_row_y + row_h * 0.65f;
        n.rows.push_back(r);
        current_row_y += row_h;
    }
    
    return n;
}

} // namespace flex::modules::flexmaid
