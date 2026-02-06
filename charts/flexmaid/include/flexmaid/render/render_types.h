#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace flex {
namespace modules {
namespace flexmaid {

struct LayoutData {
    struct Bounds {
        float x = 0, y = 0;
        float width = 0, height = 0;
        
        float left() const { return x - width / 2; }
        float right() const { return x + width / 2; }
        float top() const { return y - height / 2; }
        float bottom() const { return y + height / 2; }
    };
    
    struct Path {
        std::vector<std::pair<float, float>> points;
    };
    
    std::unordered_map<std::string, Bounds> node_bounds;
    std::vector<Path> edge_paths;
    float width = 0;
    float height = 0;
};

struct LayoutResult {
    bool success = false;
    std::string error;
    LayoutData data;
};

struct Theme {
    std::string background_color = "#ffffff";
    std::string primary_color = "#0066cc";
    std::string secondary_color = "#666666";
    std::string text_color = "#333333";
    std::string line_color = "#333333";
    std::string font_family = "Arial, sans-serif";
    int font_size = 14;
    float line_width = 2.0f;
    
    static Theme light();
    static Theme dark();
    static Theme modern();
    static Theme official();
};

// Mustache 渲染数据 (Sharable across all diagrams)
struct MustacheEdgeData {
    std::string path_d;
    std::string label;
    bool has_label;
    float label_x, label_y;
    float label_rect_x, label_rect_y, label_rect_w, label_rect_h;
    bool is_dotted, is_dashed, is_thick;
    std::string marker_start, marker_end;
};

struct MustacheAttrData {
    std::string type, name, row_bg;
    float row_y, row_h, col_div, ty;
};

struct MustacheSubgraphData {
    std::string label;
    std::string icon_char;
    float x, y, width, height;
    float text_x, text_y;
    float icon_x, icon_y;
    bool is_architecture;
};

struct MustacheNodeData {
    std::string id, label;
    float x, y, width, height, text_y, text_y_node;
    float rect_x, rect_y, radius, radius_inner, rect_x_end, rect_y_end, rect_x_sub, rect_x_sub_end;
    float lifeline_y2;
    float ry_actor, ry_actor_body, ry_actor_legs, ry_actor_arms, ry_actor_feet, x_actor_l, x_actor_r;
    bool is_architecture, is_sequence, is_participant;
    bool shape_rect, shape_round_rect, shape_circle, shape_diamond, shape_stadium;
    bool shape_cylinder, shape_subroutine, shape_parallelogram, shape_trapezoid, shape_note;
    bool shape_double_circle, shape_actor;
    std::vector<MustacheAttrData> rows;
    std::vector<MustacheAttrData> methods;
    bool has_rows, has_methods;
    float sep_y, sep2_y;
    bool has_sep2;
    std::string points_str, icon_char, icon_svg;
    std::string c4_type_label;
    float c4_label_x, c4_label_y;  // position for c4 type label
    float name_x;                   // x position for name (for grid layout)
    std::string primary_color, text_color;
};

struct MustacheContext {
    float width, height;
    std::string background_color, primary_color, secondary_color;
    std::string text_color, line_color, font_family;
    float font_size, line_width, line_width_thick, label_font_size;
    std::vector<MustacheNodeData> nodes;
    std::vector<MustacheEdgeData> edges;
    std::vector<MustacheSubgraphData> subgraphs;
};

} // namespace flexmaid
} // namespace modules
} // namespace flex
