#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include <memory>

// Forward declarations (C types from nanovg.h)
struct NVGcontext;
struct NVGcolor;

namespace whiteboard {
namespace ddf {

// Forward declarations
class StyleSheet;

/**
 * @brief Render node for CSS-like rendering pipeline
 * 
 * RenderNode represents a node in the render tree (similar to DOM).
 * It supports CSS-like styling, transforms, and hierarchical rendering.
 */
class RenderNode {
public:
    RenderNode() = default;
    ~RenderNode() = default;

    // Identity
    std::string id;
    std::string type;  // "rect", "circle", "group", etc.
    std::vector<std::string> classes;
    std::set<std::string> pseudo_states;  // "hover", "selected", "active"

    // Tree structure (like DOM)
    RenderNode* parent = nullptr;
    std::vector<std::unique_ptr<RenderNode>> children;

    // Styles
    std::map<std::string, std::string> inline_style;  // Highest priority
    std::map<std::string, std::string> computed_style;  // After cascade

    // Geometry
    std::map<std::string, float> geometry;  // x, y, width, height, etc.

    // Transform (stored as 6 values: a, b, c, d, e, f for affine transform)
    std::vector<float> local_transform;
    std::vector<float> world_transform;  // Accumulated from parents

    // Bounding box (for culling) - x, y, width, height
    float bounds_x = 0.0f;
    float bounds_y = 0.0f;
    float bounds_width = 0.0f;
    float bounds_height = 0.0f;

    // Data binding
    std::string data_node_id;  // Empty string means no binding

    // Text content (for text nodes)
    std::string text;

    // Methods
    void compute_styles(const StyleSheet& stylesheet);
    void compute_layout();
    void compute_transforms();
    void compute_bounds();
    void render(NVGcontext* ctx);

    // CSS-like queries
    RenderNode* query_selector(const std::string& selector);
    std::vector<RenderNode*> query_selector_all(const std::string& selector);
    
    // Color parsing (public for testing)
    ::NVGcolor parse_color(const std::string& color_str);

private:
    // Expression evaluation helpers
    void evaluate_expressions_in_styles();
    bool is_expression(const std::string& str) const;
    std::string extract_expression(const std::string& str) const;
    std::string evaluate_expression(const std::string& expr);
    void render_self(NVGcontext* ctx);
    void apply_transform(NVGcontext* ctx);
    
    // Helper methods for rendering different shape types
    void render_rect(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width);
    void render_circle(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width);
    void render_ellipse(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width);
    void render_path(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width);
    void render_text(NVGcontext* ctx, ::NVGcolor fill);
    void render_line(NVGcontext* ctx, ::NVGcolor stroke, float stroke_width);
    void render_polygon(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width);
    void render_polyline(NVGcontext* ctx, ::NVGcolor stroke, float stroke_width);
};

} // namespace ddf
} // namespace whiteboard
