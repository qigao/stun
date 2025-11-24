#include <whiteboard/ddf/render_node.h>
#include <whiteboard/ddf/style_layer.h>
#include <whiteboard/ddf/stylesheet.h>
#include <whiteboard/ddf/expression_parser.h>
#include <nanovg.h>

namespace whiteboard {
namespace ddf {

void RenderNode::compute_styles(const StyleSheet& stylesheet) {
    // Get parent's computed style for inheritance
    std::map<std::string, std::string> parent_style;
    if (parent) {
        parent_style = parent->computed_style;
    }
    
    // Use StyleSheet's compute_style method which handles:
    // 1. Default styles for the type
    // 2. Inheritance from parent
    // 3. Matching stylesheet rules (sorted by specificity)
    // 4. Inline styles (highest priority)
    computed_style = stylesheet.compute_style(
        id, type, classes, pseudo_states, inline_style, parent_style);
    
    // Evaluate expressions in computed styles
    evaluate_expressions_in_styles();
    
    // Recursively compute children
    for (auto& child : children) {
        child->compute_styles(stylesheet);
    }
}

void RenderNode::evaluate_expressions_in_styles() {
    // Check if any style values contain expressions ({{ }})
    for (auto& [key, value] : computed_style) {
        if (is_expression(value)) {
            // Extract expression content (remove {{ and }})
            std::string expr = extract_expression(value);
            
            // Evaluate the expression
            std::string result = evaluate_expression(expr);
            
            // Update the computed style with the result
            computed_style[key] = result;
        }
    }
    
    // Also evaluate expressions in geometry
    for (auto& [key, value] : geometry) {
        // Geometry values are floats, but we might have expressions in text form
        // For now, geometry is already numeric, so skip
    }
}

bool RenderNode::is_expression(const std::string& str) const {
    return str.find("{{") != std::string::npos && str.find("}}") != std::string::npos;
}

std::string RenderNode::extract_expression(const std::string& str) const {
    size_t start = str.find("{{");
    size_t end = str.find("}}");
    
    if (start != std::string::npos && end != std::string::npos && end > start + 2) {
        return str.substr(start + 2, end - start - 2);
    }
    
    return str;
}

std::string RenderNode::evaluate_expression(const std::string& expr) {
    // Tokenize
    ExpressionTokenizer tokenizer;
    auto tokens = tokenizer.tokenize(expr);
    
    if (tokenizer.has_error()) {
        return expr;  // Return original on error
    }
    
    // Parse
    ExpressionParser parser;
    auto ast = parser.parse(tokens);
    
    if (parser.has_error() || !ast) {
        return expr;  // Return original on error
    }
    
    // Evaluate
    EvaluationContext context;
    // TODO: Populate context with actual data
    // For now, use empty context
    
    ExpressionEvaluator evaluator;
    Value result = evaluator.evaluate(ast.get(), context);
    
    if (evaluator.has_error()) {
        return expr;  // Return original on error
    }
    
    return result.to_string();
}

void RenderNode::compute_layout() {
    // TODO: Implement layout computation
    
    // Recursively compute children
    for (auto& child : children) {
        child->compute_layout();
    }
}

void RenderNode::compute_transforms() {
    // Initialize world transform
    if (local_transform.size() != 6) {
        // Default to identity transform if local_transform is not set
        local_transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    }
    
    // Accumulate transforms from parent
    if (parent && parent->world_transform.size() == 6) {
        // Multiply parent's world transform with local transform
        // Result = parent * local
        // Affine transform: [a, b, c, d, e, f] represents:
        // | a  c  e |
        // | b  d  f |
        // | 0  0  1 |
        
        float pa = parent->world_transform[0];
        float pb = parent->world_transform[1];
        float pc = parent->world_transform[2];
        float pd = parent->world_transform[3];
        float pe = parent->world_transform[4];
        float pf = parent->world_transform[5];
        
        float la = local_transform[0];
        float lb = local_transform[1];
        float lc = local_transform[2];
        float ld = local_transform[3];
        float le = local_transform[4];
        float lf = local_transform[5];
        
        // Matrix multiplication
        world_transform = {
            pa * la + pc * lb,           // a
            pb * la + pd * lb,           // b
            pa * lc + pc * ld,           // c
            pb * lc + pd * ld,           // d
            pa * le + pc * lf + pe,      // e
            pb * le + pd * lf + pf       // f
        };
    } else {
        // No parent or parent has no transform, use local transform
        world_transform = local_transform;
    }
    
    // Recursively compute children
    for (auto& child : children) {
        child->compute_transforms();
    }
}

void RenderNode::compute_bounds() {
    // TODO: Implement bounds computation
    
    // Recursively compute children
    for (auto& child : children) {
        child->compute_bounds();
    }
}

void RenderNode::render(NVGcontext* ctx) {
    if (!ctx) return;
    
    // Save graphics state
    nvgSave(ctx);
    
    // Apply world transform
    apply_transform(ctx);
    
    // Render this node
    render_self(ctx);
    
    // Render children (tree traversal)
    for (auto& child : children) {
        child->render(ctx);
    }
    
    // Restore graphics state
    nvgRestore(ctx);
}

RenderNode* RenderNode::query_selector(const std::string& selector) {
    // TODO: Implement CSS-like query selector
    return nullptr;
}

std::vector<RenderNode*> RenderNode::query_selector_all(const std::string& selector) {
    // TODO: Implement CSS-like query selector all
    return {};
}

void RenderNode::render_self(NVGcontext* ctx) {
    if (!ctx) return;
    
    // Parse and apply styles
    ::NVGcolor fill_color = nvgRGBA(0, 0, 0, 0);  // Transparent by default
    ::NVGcolor stroke_color = nvgRGBA(0, 0, 0, 255);  // Black by default
    float stroke_width = 1.0f;
    float opacity = 1.0f;
    
    // Parse fill color
    if (computed_style.count("fill")) {
        fill_color = parse_color(computed_style["fill"]);
    }
    
    // Parse stroke color
    if (computed_style.count("stroke")) {
        stroke_color = parse_color(computed_style["stroke"]);
    }
    
    // Parse stroke width
    if (computed_style.count("stroke-width")) {
        try {
            stroke_width = std::stof(computed_style["stroke-width"]);
        } catch (...) {
            stroke_width = 1.0f;
        }
    }
    
    // Parse opacity
    if (computed_style.count("opacity")) {
        try {
            opacity = std::stof(computed_style["opacity"]);
        } catch (...) {
            opacity = 1.0f;
        }
    }
    
    // Apply global opacity
    if (opacity < 1.0f) {
        nvgGlobalAlpha(ctx, opacity);
    }
    
    // Render based on shape type
    if (type == "rect") {
        render_rect(ctx, fill_color, stroke_color, stroke_width);
    }
    else if (type == "circle") {
        render_circle(ctx, fill_color, stroke_color, stroke_width);
    }
    else if (type == "ellipse") {
        render_ellipse(ctx, fill_color, stroke_color, stroke_width);
    }
    else if (type == "path") {
        render_path(ctx, fill_color, stroke_color, stroke_width);
    }
    else if (type == "text") {
        render_text(ctx, fill_color);
    }
    else if (type == "line") {
        render_line(ctx, stroke_color, stroke_width);
    }
    else if (type == "polygon") {
        render_polygon(ctx, fill_color, stroke_color, stroke_width);
    }
    else if (type == "polyline") {
        render_polyline(ctx, stroke_color, stroke_width);
    }
    // "group" type doesn't render itself, only its children
    
    // Reset global alpha
    if (opacity < 1.0f) {
        nvgGlobalAlpha(ctx, 1.0f);
    }
}

void RenderNode::apply_transform(NVGcontext* ctx) {
    if (!ctx) return;
    
    // Apply world transform if it exists
    if (world_transform.size() == 6) {
        // NanoVG transform format: [a, b, c, d, e, f]
        // Represents affine matrix:
        // | a  c  e |
        // | b  d  f |
        // | 0  0  1 |
        nvgTransform(ctx, 
                    world_transform[0],  // a (scale x)
                    world_transform[1],  // b (skew y)
                    world_transform[2],  // c (skew x)
                    world_transform[3],  // d (scale y)
                    world_transform[4],  // e (translate x)
                    world_transform[5]); // f (translate y)
    }
}

// Helper methods for rendering different shape types

void RenderNode::render_rect(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width) {
    float x = geometry.count("x") ? geometry["x"] : 0.0f;
    float y = geometry.count("y") ? geometry["y"] : 0.0f;
    float width = geometry.count("width") ? geometry["width"] : 0.0f;
    float height = geometry.count("height") ? geometry["height"] : 0.0f;
    float rx = geometry.count("rx") ? geometry["rx"] : 0.0f;  // Corner radius
    
    nvgBeginPath(ctx);
    
    if (rx > 0.0f) {
        // Rounded rectangle
        nvgRoundedRect(ctx, x, y, width, height, rx);
    } else {
        // Regular rectangle
        nvgRect(ctx, x, y, width, height);
    }
    
    // Fill if fill color has alpha > 0
    if (fill.a > 0.0f) {
        nvgFillColor(ctx, fill);
        nvgFill(ctx);
    }
    
    // Stroke if stroke width > 0 and stroke color has alpha > 0
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_circle(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width) {
    float cx = geometry.count("cx") ? geometry["cx"] : 0.0f;
    float cy = geometry.count("cy") ? geometry["cy"] : 0.0f;
    float r = geometry.count("r") ? geometry["r"] : 0.0f;
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, r);
    
    if (fill.a > 0.0f) {
        nvgFillColor(ctx, fill);
        nvgFill(ctx);
    }
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_ellipse(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width) {
    float cx = geometry.count("cx") ? geometry["cx"] : 0.0f;
    float cy = geometry.count("cy") ? geometry["cy"] : 0.0f;
    float rx = geometry.count("rx") ? geometry["rx"] : 0.0f;
    float ry = geometry.count("ry") ? geometry["ry"] : 0.0f;
    
    nvgBeginPath(ctx);
    nvgEllipse(ctx, cx, cy, rx, ry);
    
    if (fill.a > 0.0f) {
        nvgFillColor(ctx, fill);
        nvgFill(ctx);
    }
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_path(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width) {
    // Path data should be in geometry["d"] as SVG path string
    if (!geometry.count("d")) return;
    
    // For now, we'll implement a simple path parser
    // A full SVG path parser would be more complex
    // This is a minimal implementation for basic paths
    
    // TODO: Implement full SVG path parsing
    // For now, just skip rendering complex paths
    
    if (fill.a > 0.0f) {
        nvgFillColor(ctx, fill);
        nvgFill(ctx);
    }
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_text(NVGcontext* ctx, ::NVGcolor fill) {
    float x = geometry.count("x") ? geometry["x"] : 0.0f;
    float y = geometry.count("y") ? geometry["y"] : 0.0f;
    
    // Parse text style properties
    float font_size = 14.0f;
    if (computed_style.count("font-size")) {
        try {
            std::string size_str = computed_style["font-size"];
            // Remove "px" suffix if present
            if (size_str.size() > 2 && size_str.substr(size_str.size() - 2) == "px") {
                size_str = size_str.substr(0, size_str.size() - 2);
            }
            font_size = std::stof(size_str);
        } catch (...) {
            font_size = 14.0f;
        }
    }
    
    // Set font properties
    nvgFontSize(ctx, font_size);
    
    // Set text alignment
    int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
    if (computed_style.count("text-align")) {
        const std::string& align_str = computed_style["text-align"];
        if (align_str == "center") {
            align = NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE;
        } else if (align_str == "right") {
            align = NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE;
        }
    }
    nvgTextAlign(ctx, align);
    
    // Set fill color
    nvgFillColor(ctx, fill);
    
    // Render text
    if (!text.empty()) {
        nvgText(ctx, x, y, text.c_str(), nullptr);
    }
}

void RenderNode::render_line(NVGcontext* ctx, ::NVGcolor stroke, float stroke_width) {
    float x1 = geometry.count("x1") ? geometry["x1"] : 0.0f;
    float y1 = geometry.count("y1") ? geometry["y1"] : 0.0f;
    float x2 = geometry.count("x2") ? geometry["x2"] : 0.0f;
    float y2 = geometry.count("y2") ? geometry["y2"] : 0.0f;
    
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);
    nvgLineTo(ctx, x2, y2);
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_polygon(NVGcontext* ctx, ::NVGcolor fill, ::NVGcolor stroke, float stroke_width) {
    // Points should be in geometry["points"] as space-separated coordinate pairs
    if (!geometry.count("points")) return;
    
    // Parse points (simplified - assumes points are stored as individual x0, y0, x1, y1, etc.)
    std::vector<float> points;
    for (int i = 0; geometry.count("x" + std::to_string(i)); ++i) {
        points.push_back(geometry["x" + std::to_string(i)]);
        points.push_back(geometry["y" + std::to_string(i)]);
    }
    
    if (points.size() < 4) return;  // Need at least 2 points
    
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, points[0], points[1]);
    for (size_t i = 2; i < points.size(); i += 2) {
        nvgLineTo(ctx, points[i], points[i + 1]);
    }
    nvgClosePath(ctx);
    
    if (fill.a > 0.0f) {
        nvgFillColor(ctx, fill);
        nvgFill(ctx);
    }
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

void RenderNode::render_polyline(NVGcontext* ctx, ::NVGcolor stroke, float stroke_width) {
    // Similar to polygon but without closing the path
    if (!geometry.count("points")) return;
    
    std::vector<float> points;
    for (int i = 0; geometry.count("x" + std::to_string(i)); ++i) {
        points.push_back(geometry["x" + std::to_string(i)]);
        points.push_back(geometry["y" + std::to_string(i)]);
    }
    
    if (points.size() < 4) return;
    
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, points[0], points[1]);
    for (size_t i = 2; i < points.size(); i += 2) {
        nvgLineTo(ctx, points[i], points[i + 1]);
    }
    
    if (stroke_width > 0.0f && stroke.a > 0.0f) {
        nvgStrokeColor(ctx, stroke);
        nvgStrokeWidth(ctx, stroke_width);
        nvgStroke(ctx);
    }
}

// Helper function to convert HSL to RGB
static void hsl_to_rgb(float h, float s, float l, int& r, int& g, int& b) {
    // Normalize hue to 0-1 range
    h = h / 360.0f;
    while (h < 0) h += 1.0f;
    while (h > 1) h -= 1.0f;
    
    // Normalize saturation and lightness to 0-1 range
    s = s / 100.0f;
    l = l / 100.0f;
    
    auto hue_to_rgb = [](float p, float q, float t) -> float {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f/6.0f) return p + (q - p) * 6 * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6;
        return p;
    };
    
    float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    
    r = static_cast<int>(hue_to_rgb(p, q, h + 1.0f/3.0f) * 255);
    g = static_cast<int>(hue_to_rgb(p, q, h) * 255);
    b = static_cast<int>(hue_to_rgb(p, q, h - 1.0f/3.0f) * 255);
}

::NVGcolor RenderNode::parse_color(const std::string& color_str) {
    // Parse color string (supports hex, rgb/rgba, hsl/hsla, and named colors)
    if (color_str.empty()) {
        return nvgRGBA(0, 0, 0, 0);
    }
    
    // Handle hex colors (#RGB, #RRGGBB, #RRGGBBAA)
    if (color_str[0] == '#') {
        std::string hex = color_str.substr(1);
        
        if (hex.length() == 3) {
            // #RGB -> #RRGGBB
            int r = std::stoi(hex.substr(0, 1), nullptr, 16) * 17;
            int g = std::stoi(hex.substr(1, 1), nullptr, 16) * 17;
            int b = std::stoi(hex.substr(2, 1), nullptr, 16) * 17;
            return nvgRGB(r, g, b);
        }
        else if (hex.length() == 6) {
            // #RRGGBB
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            return nvgRGB(r, g, b);
        }
        else if (hex.length() == 8) {
            // #RRGGBBAA
            int r = std::stoi(hex.substr(0, 2), nullptr, 16);
            int g = std::stoi(hex.substr(2, 2), nullptr, 16);
            int b = std::stoi(hex.substr(4, 2), nullptr, 16);
            int a = std::stoi(hex.substr(6, 2), nullptr, 16);
            return nvgRGBA(r, g, b, a);
        }
    }
    
    // Handle rgb() and rgba() format
    if (color_str.substr(0, 4) == "rgb(") {
        // Parse rgb(r, g, b)
        size_t start = 4;
        size_t end = color_str.find(',', start);
        int r = std::stoi(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(',', start);
        int g = std::stoi(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(')', start);
        int b = std::stoi(color_str.substr(start, end - start));
        
        return nvgRGB(r, g, b);
    }
    
    if (color_str.substr(0, 5) == "rgba(") {
        // Parse rgba(r, g, b, a)
        size_t start = 5;
        size_t end = color_str.find(',', start);
        int r = std::stoi(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(',', start);
        int g = std::stoi(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(',', start);
        int b = std::stoi(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(')', start);
        float a = std::stof(color_str.substr(start, end - start));
        
        return nvgRGBA(r, g, b, static_cast<int>(a * 255));
    }
    
    // Handle hsl() format
    if (color_str.substr(0, 4) == "hsl(") {
        // Parse hsl(h, s%, l%)
        size_t start = 4;
        size_t end = color_str.find(',', start);
        float h = std::stof(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(',', start);
        std::string s_str = color_str.substr(start, end - start);
        // Remove % if present
        if (s_str.find('%') != std::string::npos) {
            s_str = s_str.substr(0, s_str.find('%'));
        }
        float s = std::stof(s_str);
        
        start = end + 1;
        end = color_str.find(')', start);
        std::string l_str = color_str.substr(start, end - start);
        // Remove % if present
        if (l_str.find('%') != std::string::npos) {
            l_str = l_str.substr(0, l_str.find('%'));
        }
        float l = std::stof(l_str);
        
        int r, g, b;
        hsl_to_rgb(h, s, l, r, g, b);
        return nvgRGB(r, g, b);
    }
    
    // Handle hsla() format
    if (color_str.substr(0, 5) == "hsla(") {
        // Parse hsla(h, s%, l%, a)
        size_t start = 5;
        size_t end = color_str.find(',', start);
        float h = std::stof(color_str.substr(start, end - start));
        
        start = end + 1;
        end = color_str.find(',', start);
        std::string s_str = color_str.substr(start, end - start);
        if (s_str.find('%') != std::string::npos) {
            s_str = s_str.substr(0, s_str.find('%'));
        }
        float s = std::stof(s_str);
        
        start = end + 1;
        end = color_str.find(',', start);
        std::string l_str = color_str.substr(start, end - start);
        if (l_str.find('%') != std::string::npos) {
            l_str = l_str.substr(0, l_str.find('%'));
        }
        float l = std::stof(l_str);
        
        start = end + 1;
        end = color_str.find(')', start);
        float a = std::stof(color_str.substr(start, end - start));
        
        int r, g, b;
        hsl_to_rgb(h, s, l, r, g, b);
        return nvgRGBA(r, g, b, static_cast<int>(a * 255));
    }
    
    // Handle named colors (basic set)
    if (color_str == "transparent") return nvgRGBA(0, 0, 0, 0);
    if (color_str == "black") return nvgRGB(0, 0, 0);
    if (color_str == "white") return nvgRGB(255, 255, 255);
    if (color_str == "red") return nvgRGB(255, 0, 0);
    if (color_str == "green") return nvgRGB(0, 128, 0);
    if (color_str == "blue") return nvgRGB(0, 0, 255);
    if (color_str == "yellow") return nvgRGB(255, 255, 0);
    if (color_str == "cyan") return nvgRGB(0, 255, 255);
    if (color_str == "magenta") return nvgRGB(255, 0, 255);
    if (color_str == "gray" || color_str == "grey") return nvgRGB(128, 128, 128);
    
    // Default to black if color cannot be parsed
    return nvgRGB(0, 0, 0);
}

} // namespace ddf
} // namespace whiteboard
