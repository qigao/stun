/*
 * NanoVG CSS - Painter implementation
 *
 * Translates CSS properties to NanoVG drawing calls.
 */

#include "cssbox_internal.h"
#include <glad/glad.h>
#include "cssbox_filters.h"
#include <cssbox_filters.h>
#include <nanovg_rough.h>
#include "cssbox_svg_path.h"
#include "cssbox_path_sampler.h"
#include <fmtlog.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <unordered_set>
#include <vector>

// Include re2c-generated gradient lexer
extern "C" {
#include "css_gradient_lexer_gen.c"
}

// Helper to extract ID from url(#id) or #id
static std::string extract_url_id(const std::string& url) {
    // Handle direct #id format (SVG href attribute)
    if (url.size() > 1 && url[0] == '#') {
        return url.substr(1);
    }

    // Handle url(#id) format (CSS url() function)
    size_t start_pos = url.find("#");
    size_t end_pos = url.find(")");
    if (start_pos != std::string::npos && end_pos != std::string::npos && start_pos < end_pos) {
        return url.substr(start_pos + 1, end_pos - start_pos - 1);
    }

    return "";
}

// ============================================================================
// Box Creation Helpers
// ============================================================================

/**
 * @brief Create cssboxBox from typed ResolvedLayout (NEW - preferred)
 */
static cssboxBox layout_to_box(const cssbox::ResolvedLayout& layout) {
    cssboxBox box;
    box.x = layout.x;
    box.y = layout.y;
    box.width = layout.width;
    box.height = layout.height;
    for (int i = 0; i < 4; i++) {
        box.padding[i] = layout.padding[i];
        box.margin[i] = layout.margin[i];
        box.border_width[i] = layout.border[i];
        box.border_radius[i] = layout.radius[i];
    }
    return box;
}

/**
 * @brief Convert element to box for painting
 */
static cssboxBox element_to_box(const cssboxElement* element) {
    return layout_to_box(element->layout);
}

static float safe_stof(const std::string& value, float fallback = 0.f) {
    try {
        size_t processed = 0;
        float parsed = std::stof(value, &processed);
        return processed == 0 ? fallback : parsed;
    } catch (const std::exception&) {
        return fallback;
    }
}

namespace {

using StrokeStyle = cssboxPainter::StrokeStyle;
constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    float x;
    float y;
};

Vec2 operator+(const Vec2& a, const Vec2& b) {
    return {a.x + b.x, a.y + b.y};
}

Vec2 operator-(const Vec2& a, const Vec2& b) {
    return {a.x - b.x, a.y - b.y};
}

Vec2 operator*(const Vec2& v, float scalar) {
    return {v.x * scalar, v.y * scalar};
}

float length(const Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Vec2 normalize(const Vec2& v) {
    float len = length(v);
    if (len < 1e-4f) {
        return {0.0f, 0.0f};
    }
    return {v.x / len, v.y / len};
}

Vec2 perpendicular(const Vec2& v) {
    return {-v.y, v.x};
}

float salted_noise(float seed) {
    float value = std::sinf(seed * 12.9898f) * 43758.5453f;
    return value - std::floor(value);
}

Vec2 jitter_with_salt(const Vec2& v, float salt, float amplitude, float channel) {
    // Excalidraw-style roughness: perpendicular offset with controlled randomness
    float offset = (salted_noise(salt + channel * 17.0f) - 0.5f) * 2.0f * amplitude;
    return {v.x + offset, v.y + offset};
}

Vec2 offset_point(const Vec2& point, const Vec2& tangent, float salt, float roughness, float channel) {
    // Calculate perpendicular direction (normal)
    Vec2 normal = perpendicular(normalize(tangent));
    
    // Generate offset with multiple frequencies for organic feel
    float offset = 0.0f;
    offset += (salted_noise(salt + channel * 1.3f) - 0.5f) * roughness;
    offset += (salted_noise(salt + channel * 2.7f + 100.0f) - 0.5f) * roughness * 0.5f;
    
    return point + normal * offset;
}

class DashPen {
public:
    explicit DashPen(const StrokeStyle& style)
        : style_(style) {
        if (!style_.dash_array.empty()) {
            for (float entry : style_.dash_array) {
                if (entry > 0.0f) {
                    pattern_.push_back(entry);
                    total_pattern_length_ += entry;
                }
            }
        }
        if (pattern_.empty() || total_pattern_length_ <= 0.0f) {
            solid_ = true;
        } else {
            solid_ = false;
            float offset = std::fmod(style_.dash_offset, total_pattern_length_);
            if (offset < 0.0f) offset += total_pattern_length_;
            skip(offset);
        }
    }

    bool solid() const { return solid_; }

    void stroke_segment(NVGcontext* vg, const Vec2& start, const Vec2& end) {
        Vec2 dir = end - start;
        float seg_len = length(dir);
        if (seg_len < 1e-4f) return;
        if (solid_) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, start.x, start.y);
            nvgLineTo(vg, end.x, end.y);
            nvgStroke(vg);
            return;
        }

        Vec2 unit = dir * (1.0f / seg_len);
        float remaining = seg_len;
        Vec2 cursor = start;

        while (remaining > 1e-4f) {
            float current = pattern_[pattern_index_];
            float available = current - segment_pos_;
            float step = std::min(remaining, available);
            Vec2 next = cursor + unit * step;
            if (draw_segment_) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, cursor.x, cursor.y);
                nvgLineTo(vg, next.x, next.y);
                nvgStroke(vg);
            }

            remaining -= step;
            cursor = next;
            segment_pos_ += step;

            if (segment_pos_ >= current - 1e-4f) {
                segment_pos_ = 0.0f;
                pattern_index_ = (pattern_index_ + 1) % pattern_.size();
                draw_segment_ = !draw_segment_;
            }
        }
    }

private:
    void skip(float length_to_skip) {
        if (solid_) return;
        float remaining = length_to_skip;
        while (remaining > 1e-4f) {
            float current = pattern_[pattern_index_];
            float available = current - segment_pos_;
            float step = std::min(remaining, available);
            remaining -= step;
            segment_pos_ += step;
            if (segment_pos_ >= current - 1e-4f) {
                segment_pos_ = 0.0f;
                pattern_index_ = (pattern_index_ + 1) % pattern_.size();
                draw_segment_ = !draw_segment_;
            }
        }
    }

    const StrokeStyle& style_;
    std::vector<float> pattern_;
    size_t pattern_index_ = 0;
    float segment_pos_ = 0.0f;
    bool draw_segment_ = true;
    bool solid_ = true;
    float total_pattern_length_ = 0.0f;
};

void apply_stroke_state(NVGcontext* vg, const StrokeStyle& stroke, float width_variation = 0.0f) {
    float width = stroke.width;
    if (width_variation > 0.0f) {
        // Add random variation to stroke width (±variation)
        width += (salted_noise(width_variation) - 0.5f) * 2.0f * width_variation;
        width = std::max(width, stroke.width * 0.5f);  // Don't go below 50% of original
    }
    nvgStrokeWidth(vg, width);
    nvgStrokeColor(vg, stroke.color);
    nvgLineCap(vg, stroke.line_cap);
    nvgLineJoin(vg, stroke.line_join);
    nvgMiterLimit(vg, stroke.miter_limit);
}

void stroke_polyline(NVGcontext* vg,
                     const StrokeStyle& stroke,
                     const std::vector<Vec2>& points,
                     bool closed) {
    if (points.size() < 2 || !stroke.enabled) return;

    apply_stroke_state(vg, stroke);

    if (stroke.dash_array.empty()) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, points[0].x, points[0].y);
        for (size_t i = 1; i < points.size(); ++i) {
            nvgLineTo(vg, points[i].x, points[i].y);
        }
        if (closed) {
            nvgLineTo(vg, points[0].x, points[0].y);
        }
        nvgStroke(vg);
        return;
    }

    DashPen pen(stroke);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        pen.stroke_segment(vg, points[i], points[i + 1]);
    }
    if (closed) {
        pen.stroke_segment(vg, points.back(), points.front());
    }
}

std::vector<Vec2> build_freehand_outline(const std::vector<Vec2>& raw_points,
                                         float stroke_width,
                                         float salt) {
    if (raw_points.size() < 2) return {};

    std::vector<Vec2> left;
    std::vector<Vec2> right;
    left.reserve(raw_points.size());
    right.reserve(raw_points.size());

    for (size_t i = 0; i < raw_points.size(); ++i) {
        Vec2 prev = (i == 0) ? raw_points[i] : raw_points[i - 1];
        Vec2 next = (i + 1 >= raw_points.size()) ? raw_points[i] : raw_points[i + 1];
        Vec2 dir = normalize(next - prev);
        Vec2 normal = perpendicular(dir);
        if (length(normal) < 1e-4f) {
            normal = {0.0f, 1.0f};
        } else {
            normal = normalize(normal);
        }

        float noise = salted_noise(salt + static_cast<float>(i) * 0.37f) - 0.5f;
        float thickness = stroke_width * (0.55f + 0.35f * noise);
        left.push_back(raw_points[i] + normal * thickness);
        right.push_back(raw_points[i] - normal * thickness);
    }

    std::vector<Vec2> outline;
    outline.reserve(left.size() + right.size());
    outline.insert(outline.end(), left.begin(), left.end());
    for (auto it = right.rbegin(); it != right.rend(); ++it) {
        outline.push_back(*it);
    }
    return outline;
}

// Parse SVG points attribute (e.g., "100,100 200,150 150,250")
std::vector<Vec2> parse_svg_points(const std::string& points_str) {
    std::vector<Vec2> points;
    if (points_str.empty()) return points;
    
    const char* p = points_str.c_str();
    
    while (*p) {
        // Skip whitespace and commas
        while (*p && (isspace(*p) || *p == ',')) p++;
        if (!*p) break;
        
        // Parse x coordinate
        char* end_x;
        float x = strtof(p, &end_x);
        if (p == end_x) break;  // No number found
        p = end_x;
        
        // Skip optional comma/whitespace
        while (*p && (isspace(*p) || *p == ',')) p++;
        if (!*p) break;
        
        // Parse y coordinate
        char* end_y;
        float y = strtof(p, &end_y);
        if (p == end_y) break;  // No number found
        p = end_y;
        
        points.push_back({x, y});
    }
    
    return points;
}

// Vertex structure for SVG path marker tracking
struct PathVertex {
    float x, y;
    float angle_in;   // Angle of incoming segment
    float angle_out;  // Angle of outgoing segment
};

// Helper to track vertices for marker-mid rendering
// For simple segments (L, H, V, A): prev_angle_out == angle_out
// For curves (C, S, Q, T): prev_angle_out is start tangent, angle_out is end tangent
static void track_path_vertex(std::vector<PathVertex>& vertices, bool has_first,
                              float curr_x, float curr_y,
                              float angle_out, float prev_angle_out) {
    if (vertices.empty() && !has_first) return;

    PathVertex v;
    v.x = curr_x;
    v.y = curr_y;
    v.angle_out = angle_out;
    if (!vertices.empty()) {
        vertices.back().angle_out = prev_angle_out;
    }
    v.angle_in = angle_out;
    vertices.push_back(v);
}

}  // namespace

// Forward declaration for Sprint 31 background image rendering
static void render_background_image(const cssboxElement* element,
                                    const cssboxBox& box,
                                    cssboxRenderer* renderer);

cssboxPainter::cssboxPainter(NVGcontext* vg, cssboxRenderer* renderer) 
    : vg_(vg), renderer_(renderer), filter_context_(nullptr) {
    // Initialize filter context only if we have a valid NanoVG context
    if (vg_) {
        filter_context_ = cssboxCreateFilterContext();
    }
}

cssboxPainter::~cssboxPainter() {
    if (filter_context_) {
        cssboxDeleteFilterContext(filter_context_);
    }
}

void cssboxPainter::apply_filters(const cssboxElement* element, float& opacity) {
    if (element->style.filters.empty()) return;
    
    // Separate opacity from other filters
    bool has_advanced_filters = false;
    for (const auto& filter : element->style.filters) {
        if (filter.type == cssbox::FilterType::OPACITY) {
            opacity *= filter.value;
        } else {
            has_advanced_filters = true;
        }
    }
    
    // If no advanced filters, we're done
    if (!has_advanced_filters || !filter_context_) return;
    
    // Apply advanced filters using OpenGL shaders
    // This requires rendering to FBO, applying shaders, then compositing
    // For now, we'll apply them in the render loop when needed
}

void cssboxPainter::render_with_filters(const cssboxElement* element, 
                                        const cssboxBox& box,
                                        std::function<void()> render_fn) {
    if (element->style.filters.empty() || !filter_context_ || !vg_) {
        render_fn();
        return;
    }
    
    // Check if we have any advanced filters (non-opacity)
    bool has_advanced = false;
    for (const auto& filter : element->style.filters) {
        if (filter.type != cssbox::FilterType::OPACITY) {
            has_advanced = true;
            break;
        }
    }
    
    if (!has_advanced) {
        render_fn();
        return;
    }
    
    // Convert filters to C API format
    std::vector<cssboxFilter> c_filters;
    for (const auto& filter : element->style.filters) {
        cssboxFilterType type;
        switch (filter.type) {
            case cssbox::FilterType::BLUR:
                type = cssbox_FILTER_BLUR;
                break;
            case cssbox::FilterType::BRIGHTNESS:
                type = cssbox_FILTER_BRIGHTNESS;
                break;
            case cssbox::FilterType::CONTRAST:
                type = cssbox_FILTER_CONTRAST;
                break;
            case cssbox::FilterType::GRAYSCALE:
                type = cssbox_FILTER_GRAYSCALE;
                break;
            case cssbox::FilterType::HUE_ROTATE:
                type = cssbox_FILTER_HUE_ROTATE;
                break;
            case cssbox::FilterType::INVERT:
                type = cssbox_FILTER_INVERT;
                break;
            case cssbox::FilterType::SATURATE:
                type = cssbox_FILTER_SATURATE;
                break;
            case cssbox::FilterType::SEPIA:
                type = cssbox_FILTER_SEPIA;
                break;
            case cssbox::FilterType::OPACITY:
                type = cssbox_FILTER_OPACITY;
                break;
            default:
                continue;
        }
        c_filters.push_back({type, filter.value});
    }
    
    // Apply filters using framebuffer rendering
    auto callback = [](NVGcontext* vg, void* user_data) {
        auto* fn = static_cast<std::function<void()>*>(user_data);
        (*fn)();
    };
    
    cssboxApplyFilters(filter_context_, vg_, box.x, box.y, box.width, box.height,
                       c_filters.data(), (int)c_filters.size(), callback, &render_fn);
}

// Sprint 33: Helper function to render a single border side with style
// Convert BorderStyle enum to render behavior
static void render_border_side(NVGcontext* vg,
                               float x1, float y1, float x2, float y2,
                               float width, NVGcolor color,
                               cssbox::BorderStyle style)
{
    if (style == cssbox::BorderStyle::NONE ||
        style == cssbox::BorderStyle::HIDDEN ||
        width <= 0) {
        return;
    }

    if (style == cssbox::BorderStyle::SOLID) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
    else if (style == cssbox::BorderStyle::DASHED) {
        nvgLineCap(vg, NVG_BUTT);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);

        float dash_length = width * 3.0f;
        float gap_length = width * 3.0f;

        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);
        if (length < 0.01f) return;

        float ux = dx / length;
        float uy = dy / length;
        float pos = 0;
        bool draw = true;

        while (pos < length) {
            float segment_len = draw ? dash_length : gap_length;
            float end_pos = std::min(pos + segment_len, length);
            if (draw) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, x1 + ux * pos, y1 + uy * pos);
                nvgLineTo(vg, x1 + ux * end_pos, y1 + uy * end_pos);
                nvgStroke(vg);
            }
            pos = end_pos;
            draw = !draw;
        }
    }
    else if (style == cssbox::BorderStyle::DOTTED) {
        nvgLineCap(vg, NVG_ROUND);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);

        float dot_spacing = width * 2.0f;
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);
        if (length < 0.01f) return;

        float ux = dx / length;
        float uy = dy / length;
        int num_dots = static_cast<int>(length / dot_spacing) + 1;

        for (int i = 0; i < num_dots; i++) {
            float t = (num_dots > 1) ? (float)i / (num_dots - 1) : 0.0f;
            float px = x1 + dx * t;
            float py = y1 + dy * t;
            nvgBeginPath(vg);
            nvgCircle(vg, px, py, width * 0.5f);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
    }
    else if (style == cssbox::BorderStyle::DOUBLE) {
        float gap = width / 3.0f;
        float line_width = width / 3.0f;

        // Outer line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, line_width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);

        // Inner line (offset perpendicular)
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);
        if (length > 0.01f) {
            float nx = -dy / length * (line_width + gap);
            float ny = dx / length * (line_width + gap);
            nvgBeginPath(vg);
            nvgMoveTo(vg, x1 + nx, y1 + ny);
            nvgLineTo(vg, x2 + nx, y2 + ny);
            nvgStroke(vg);
        }
    }
    // GROOVE, RIDGE, INSET, OUTSET - fall through to solid for now
    else {
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
}

// DEPRECATED: String-based overload for backward compatibility
static void render_border_side(NVGcontext* vg,
                               float x1, float y1, float x2, float y2,
                               float width, NVGcolor color,
                               const std::string& style)
{
    if (style == "none" || style == "hidden" || width <= 0) {
        return;  // Don't render
    }

    if (style == "solid") {
        // Solid line (default)
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
    else if (style == "dashed") {
        // Dashed line
        nvgLineCap(vg, NVG_BUTT);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);

        float dash_length = width * 3.0f;  // Dash is 3x border width
        float gap_length = width * 3.0f;   // Gap is 3x border width

        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float ux = dx / length;  // Unit vector
        float uy = dy / length;

        float pos = 0;
        bool draw = true;

        while (pos < length) {
            float segment_len = draw ? dash_length : gap_length;
            float end_pos = std::min(pos + segment_len, length);

            if (draw) {
                float sx = x1 + ux * pos;
                float sy = y1 + uy * pos;
                float ex = x1 + ux * end_pos;
                float ey = y1 + uy * end_pos;

                nvgBeginPath(vg);
                nvgMoveTo(vg, sx, sy);
                nvgLineTo(vg, ex, ey);
                nvgStroke(vg);
            }

            pos = end_pos;
            draw = !draw;
        }
    }
    else if (style == "dotted") {
        // Dotted line (circles)
        nvgStrokeColor(vg, color);
        nvgFillColor(vg, color);

        float dot_spacing = width * 2.5f;  // Space between dot centers

        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float ux = dx / length;
        float uy = dy / length;

        float pos = width * 0.5f;  // Start half a dot width in

        while (pos < length - width * 0.5f) {
            float cx = x1 + ux * pos;
            float cy = y1 + uy * pos;

            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, width * 0.5f);
            nvgFill(vg);

            pos += dot_spacing;
        }
    }
    else if (style == "double") {
        // Double line (two parallel lines)
        float third_width = width / 3.0f;
        float offset = third_width;

        // Calculate perpendicular offset
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = sqrtf(dx*dx + dy*dy);

        if (length < 0.01f) return;  // Avoid division by zero

        float px = -dy / length * offset;  // Perpendicular
        float py = dx / length * offset;

        nvgStrokeWidth(vg, third_width);
        nvgStrokeColor(vg, color);

        // First line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1 + px, y1 + py);
        nvgLineTo(vg, x2 + px, y2 + py);
        nvgStroke(vg);

        // Second line
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1 - px, y1 - py);
        nvgLineTo(vg, x2 - px, y2 - py);
        nvgStroke(vg);
    }
    else {
        // Unknown style - default to solid
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeWidth(vg, width);
        nvgStrokeColor(vg, color);
        nvgStroke(vg);
    }
}

// Sprint 33: Render borders with per-side styles
// Sprint 34: Enhanced with per-side colors
// Refactored: Now uses typed border properties from element->style
static void render_styled_borders(NVGcontext* vg, const cssboxElement* element,
                                  const cssboxBox& box)
{
    // Get border widths from box (already resolved from layout)
    float top_width = box.border_width[0];
    float right_width = box.border_width[1];
    float bottom_width = box.border_width[2];
    float left_width = box.border_width[3];

    // Use typed border colors from element->style.border
    auto to_nvg = [](const cssbox::Color& c) {
        return nvgRGBAf(c.r, c.g, c.b, c.a);
    };
    NVGcolor top_color = to_nvg(element->style.border.color[0]);
    NVGcolor right_color = to_nvg(element->style.border.color[1]);
    NVGcolor bottom_color = to_nvg(element->style.border.color[2]);
    NVGcolor left_color = to_nvg(element->style.border.color[3]);

    // Use typed border styles from element->style.border
    cssbox::BorderStyle top_style = element->style.border.style[0];
    cssbox::BorderStyle right_style = element->style.border.style[1];
    cssbox::BorderStyle bottom_style = element->style.border.style[2];
    cssbox::BorderStyle left_style = element->style.border.style[3];

    // Calculate border positions (centered on box edge)
    float half_top = top_width * 0.5f;
    float half_right = right_width * 0.5f;
    float half_bottom = bottom_width * 0.5f;
    float half_left = left_width * 0.5f;

    // Top border
    if (top_width > 0) {
        render_border_side(vg,
            box.x, box.y + half_top,
            box.x + box.width, box.y + half_top,
            top_width, top_color, top_style);
    }

    // Right border
    if (right_width > 0) {
        render_border_side(vg,
            box.x + box.width - half_right, box.y,
            box.x + box.width - half_right, box.y + box.height,
            right_width, right_color, right_style);
    }

    // Bottom border
    if (bottom_width > 0) {
        render_border_side(vg,
            box.x + box.width, box.y + box.height - half_bottom,
            box.x, box.y + box.height - half_bottom,
            bottom_width, bottom_color, bottom_style);
    }

    // Left border
    if (left_width > 0) {
        render_border_side(vg,
            box.x + half_left, box.y + box.height,
            box.x + half_left, box.y,
            left_width, left_color, left_style);
    }
}

// Helper function to render text shadows - extracted from duplicated code
static void render_text_shadows(NVGcontext* vg, const cssboxElement* element,
                                float text_x, float text_y, const std::string& text_content) {
    if (element->text_shadows.empty()) return;

    // Render each shadow in reverse order (last shadow first, so first shadow is on top)
    for (auto it = element->text_shadows.rbegin(); it != element->text_shadows.rend(); ++it) {
        const TextShadow& shadow = *it;

        float shadow_x = text_x + shadow.offset_x;
        float shadow_y = text_y + shadow.offset_y;

        if (shadow.blur_radius > 0) {
            // Simple blur approximation: render text multiple times with decreasing alpha
            int blur_samples = std::min(5, (int)shadow.blur_radius);
            for (int i = 0; i < blur_samples; i++) {
                float offset = (i - blur_samples / 2.0f) * (shadow.blur_radius / blur_samples);
                float alpha_multiplier = 1.0f - (std::abs(i - blur_samples / 2.0f) / (blur_samples / 2.0f));

                NVGcolor blurred_color = shadow.color;
                blurred_color.a *= alpha_multiplier * 0.3f;

                nvgFillColor(vg, blurred_color);
                nvgText(vg, shadow_x + offset, shadow_y + offset, text_content.c_str(), nullptr);
            }
        } else {
            nvgFillColor(vg, shadow.color);
            nvgText(vg, shadow_x, shadow_y, text_content.c_str(), nullptr);
        }
    }
}

// Helper to render text content with proper styling and alignment
void cssboxPainter::paint_text_content(const cssboxElement* element,
                                        float content_x, float content_y,
                                        float content_width, float content_height) {
    if (element->text_content.empty()) return;

    NVGcolor text_color = element->style.color;
    float font_size = element->style.font_size;
    std::string font_face = compute_font_face(element);
    int align = compute_text_align(element);

    nvgFontSize(vg_, font_size);
    nvgFontFace(vg_, font_face.c_str());
    nvgTextAlign(vg_, align);

    float text_x = content_x;
    float text_y = content_y;

    if (align & NVG_ALIGN_CENTER) {
        text_x += content_width / 2.0f;
    } else if (align & NVG_ALIGN_RIGHT) {
        text_x += content_width;
    }

    if (align & NVG_ALIGN_MIDDLE) {
        text_y += content_height / 2.0f;
    } else if (align & NVG_ALIGN_BOTTOM) {
        text_y += content_height;
    }

    render_text_shadows(vg_, element, text_x, text_y, element->text_content);

    nvgFillColor(vg_, text_color);
    nvgText(vg_, text_x, text_y, element->text_content.c_str(), nullptr);

    apply_text_decoration(element, text_x, text_y, text_color);
}

void cssboxPainter::paint_element(const cssboxElement* element) {
    // DEBUG: Log what we're trying to paint
    if (element->type == "svg" || element->type == "circle" || element->type == "rect") {
        logd("[PAINTER] paint_element id='{}' type='{}' display={} visible={}",
             element->id, element->type, (int)element->style.display, element->visible);
    }

    // Check visibility from typed property
    if (element->style.display == cssbox::Display::NONE) {
        logi("[PAINTER] Skipping '{}' - display is NONE", element->id);
        return;
    }
    if (!element->visible) {
        logi("[PAINTER] Skipping '{}' - not visible", element->id);
        return;
    }

    // Skip rendering definition-only elements (they're used by reference only)
    // Retain mode: Use static hash set for O(1) lookup instead of 3 string comparisons
    static const std::unordered_set<std::string> definition_only_types = {
        "pattern", "marker", "clipPath", "defs", "linearGradient", "radialGradient"
    };
    if (definition_only_types.find(element->type) != definition_only_types.end()) {
        return;
    }

    // Save state
    nvgSave(vg_);

    // Check for clip-path property
    bool has_clip = false;
    auto clip_it = element->inline_style.find("clip-path");
    if (clip_it != element->inline_style.end()) {
        std::string clip_id = extract_url_id(clip_it->second);
        auto clip_path_it = renderer_->clip_paths_.find(clip_id);
        if (clip_path_it != renderer_->clip_paths_.end()) {
            begin_clip_path(clip_path_it->second);
            has_clip = true;
        }
    }

    // Apply transform
    nvgTransform(vg_,
                 element->transform[0], element->transform[1],
                 element->transform[2], element->transform[3],
                 element->transform[4], element->transform[5]);

    // Note: SVG viewBox transform is now handled in compute_absolute_transform
    // (cssbox.cpp), so we don't need to apply it again here

    // Apply opacity from typed property
    float combined_opacity = element->style.opacity;
    
    // Apply filter effects
    apply_filters(element, combined_opacity);
    
    nvgGlobalAlpha(vg_, combined_opacity);

    // Draw based on element type
    // HTML elements (div, button, span with text) are treated as boxes
    // SVG containers (svg, g) are also treated as boxes
    // NOTE: SVG <rect> is NOT a box element - it has its own paint_rect_shape()
    // Retain mode: Use static hash set for O(1) lookup instead of 12 string comparisons
    static const std::unordered_set<std::string> box_element_types = {
        "group", "div", "button", "input", "panel",
        "screen", "window", "widget", "svg", "g", "symbol"
    };
    bool is_box_element = (box_element_types.find(element->type) != box_element_types.end());

    if (element->type == "textPath") {
        const auto& box = element_to_box(element);
        paint_text_path(element, box);
        nvgRestore(vg_);
        return;
    }

    if (!element->stroke_points.empty()) {
        paint_freehand_path(element);
    }
    else if (is_box_element) {
        // Use typed layout system (prefer element->layout, fallback to element->computed)
        const auto& box = element_to_box(element);

        // DEBUG: Trace element rendering
        logd("[PAINTER] Rendering type='{}' id='{}' class='{}' at ({:.1f}, {:.1f}) size ({:.1f} x {:.1f})",
             element->type, element->id.empty() ? "(empty)" : element->id,
             element->classes.empty() ? "(no-class)" : element->classes[0],
             box.x, box.y, box.width, box.height);

        // Skip rendering if size is zero
        if (box.width <= 0 || box.height <= 0) {
            nvgRestore(vg_);
            return;
        }

        // Sprint 27: Render box-shadows first (so they're behind the element)
        paint_box_shadows(element, box);

        // Create path for background fill
        nvgBeginPath(vg_);
        bool has_radius = (box.border_radius[0] > 0 || box.border_radius[1] > 0 ||
                           box.border_radius[2] > 0 || box.border_radius[3] > 0);
        if (has_radius) {
            float avg_radius = (box.border_radius[0] + box.border_radius[1] +
                                box.border_radius[2] + box.border_radius[3]) / 4.0f;
            nvgRoundedRect(vg_, box.x, box.y, box.width, box.height, avg_radius);
            // printf("[PATH] Created rounded rect for id='%s' at (%.1f,%.1f) size (%.1f x %.1f) radius=%.1f\n",
            //        element->id.c_str(), box.x, box.y, box.width, box.height, avg_radius);
        } else {
            nvgRect(vg_, box.x, box.y, box.width, box.height);
            // printf("[PATH] Created rect for id='%s' at (%.1f,%.1f) size (%.1f x %.1f)\n",
            //        element->id.c_str(), box.x, box.y, box.width, box.height);
        }

        // Fill background
        apply_background(element, box);
        // Sprint 31: Render background image (on top of color/gradient)
        render_background_image(element, box, renderer_);

        // Stroke border
        // Sprint 33: Render borders with per-side styles
        render_styled_borders(vg_, element, box);
        if (element->type == "rect") {
            paint_rect_stroke(element, box);
        }
        // Render text content if present (for buttons, labels, etc.)
        // Use content box (account for padding), not border box
        // Retain mode: Use box.padding (from typed layout) instead of element->computed
        float padding_top = box.padding[0];
        float padding_right = box.padding[1];
        float padding_bottom = box.padding[2];
        float padding_left = box.padding[3];
        paint_text_content(element,
                          box.x + padding_left,
                          box.y + padding_top,
                          box.width - padding_left - padding_right,
                          box.height - padding_top - padding_bottom);
    }
    else if (element->type == "text" || element->type == "span") {
        const auto& box = element_to_box(element);
        paint_text_content(element, box.x, box.y, box.width, box.height);
    }
    else if (element->type == "line") {
        const auto& box = element_to_box(element);
        paint_line_shape(element, box);
    }
    else if (element->type == "circle" || element->type == "ellipse") {
        const auto& box = element_to_box(element);
        paint_circle_shape(element, box);
    }
    else if (element->type == "rect") {
        const auto& box = element_to_box(element);
        paint_rect_shape(element, box);
    }
    else if (element->type == "path") {
        const auto& box = element_to_box(element);
        paint_svg_path(element, box);
    }
    else if (element->type == "polygon") {
        const auto& box = element_to_box(element);
        paint_polygon(element, box);
    }
    else if (element->type == "polyline") {
        const auto& box = element_to_box(element);
        paint_polyline(element, box);
    }

    // Call custom paint callback if provided
    if (element->custom_paint) {
        // TODO: custom_paint signature should be updated to not need computed_style
        element->custom_paint(vg_, element, element->inline_style);
    }

    // End clip path if needed
    if (has_clip) {
        end_clip_path();
    }

    // Restore state
    nvgRestore(vg_);
}

void cssboxPainter::apply_background(const cssboxElement* element,
                                      const cssboxBox& box) {
    // 60fps: Read from TYPED property (not inline_style!)
    const auto& bg = element->style.background;

    logd("[PAINTER] apply_background id='{}' bg_type={} gradient_css='{}' cached_ptr={}",
         element->id, (int)bg.type, bg.gradient_css, (void*)element->cached_fill_gradient);

    // Use cached gradient pointer for SVG gradients (O(1) instead of O(log n) map lookup)
    if (element->cached_fill_gradient) {
        NVGpaint gradient_paint;
        if (element->cached_fill_gradient->type == GradientData::LINEAR) {
            gradient_paint = create_linear_gradient(*element->cached_fill_gradient, box);
        } else {
            gradient_paint = create_radial_gradient(*element->cached_fill_gradient, box);
        }
        nvgFillPaint(vg_, gradient_paint);
        nvgFill(vg_);
        return;
    }
    
    // Check for pattern fill (SVG style)
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second.find("url(#") != std::string::npos) {
        // Try pattern (patterns are not cached yet, less common than gradients)
        if (apply_pattern_fill(fill_it->second, box)) {
            return;
        }
    }

    // Handle different background types
    if (bg.type == cssbox::BackgroundType::COLOR) {
        NVGcolor bg_color = bg.color;

        // logi("[PAINTER] COLOR background: r={} g={} b={} a={}", 
        //      bg_color.r, bg_color.g, bg_color.b, bg_color.a);

        // Check if background color is transparent (alpha == 0)
        if (bg_color.a <= 0.001f) {
            logi("[PAINTER] Skipping transparent background");
            return;  // Skip transparent backgrounds
        }

        nvgFillColor(vg_, bg_color);
        nvgFill(vg_);
        // logi("[PAINTER] Background filled");
    }
    else if (bg.type == cssbox::BackgroundType::GRADIENT) {
        // Parse and render gradient from CSS string
        if (!bg.gradient_css.empty()) {
            // Check CSS gradient cache first
            auto cache_it = renderer_->css_gradient_cache_.find(bg.gradient_css);
            const GradientData* gradient_data = nullptr;

            if (cache_it != renderer_->css_gradient_cache_.end()) {
                // Cache hit - use cached parsed gradient
                gradient_data = &cache_it->second;
            } else {
                // Cache miss - parse and cache
                GradientData parsed = parse_gradient(bg.gradient_css);
                auto [it, inserted] = renderer_->css_gradient_cache_.emplace(bg.gradient_css, std::move(parsed));
                gradient_data = &it->second;
            }

            // Create NVGpaint from cached GradientData
            NVGpaint gradient_paint;
            if (gradient_data->type == GradientData::LINEAR) {
                gradient_paint = create_linear_gradient(*gradient_data, box);
            } else {
                gradient_paint = create_radial_gradient(*gradient_data, box);
            }
            nvgFillPaint(vg_, gradient_paint);
            nvgFill(vg_);
        }
    }
    else if (bg.type == cssbox::BackgroundType::IMAGE) {
        // Background image rendering implemented via render_background_image()
        // bg.image_handle contains the NanoVG image handle
    }
    // If type == NONE, skip (transparent background)
}

// Sprint 27: Render box-shadows using parsed BoxShadow data structure
// Sprint 39: Added inset shadow support
void cssboxPainter::paint_box_shadows(const cssboxElement* element,
                                       const cssboxBox& box) {
    if (element->box_shadows.empty()) return;

    // Get border radius for rounded shadows
    float border_radius = (box.border_radius[0] + box.border_radius[1] +
                           box.border_radius[2] + box.border_radius[3]) / 4.0f;

    // Render each shadow in reverse order (last shadow first, so first shadow is on top)
    for (auto it = element->box_shadows.rbegin(); it != element->box_shadows.rend(); ++it) {
        const BoxShadow& shadow = *it;

        // Sprint 39: Handle inset shadows
        if (shadow.inset) {
            // Inset shadows appear inside the element bounds
            nvgSave(vg_);

            // Clip to element bounds
            nvgScissor(vg_, box.x, box.y, box.width, box.height);

            // Create inset shadow paint
            // For inset shadows, we invert the offsets and draw from inside
            NVGpaint shadow_paint = nvgBoxGradient(vg_,
                box.x + shadow.offset_x,
                box.y + shadow.offset_y,
                box.width - shadow.spread_radius * 2,
                box.height - shadow.spread_radius * 2,
                border_radius,
                shadow.blur_radius,
                nvgRGBA(0, 0, 0, 0),      // Inner color (transparent)
                shadow.color               // Outer color (shadow color)
            );

            // Draw shadow rectangle covering the entire element
            nvgBeginPath(vg_);
            float shadow_margin = shadow.blur_radius + shadow.spread_radius + 5.0f;

            // Outer rectangle (larger than element)
            nvgRect(vg_,
                box.x - shadow_margin,
                box.y - shadow_margin,
                box.width + shadow_margin * 2,
                box.height + shadow_margin * 2);

            // Inner rectangle (the actual shadow area, shrunk by spread)
            if (border_radius > 0) {
                nvgRoundedRect(vg_,
                    box.x + shadow.offset_x - shadow.spread_radius,
                    box.y + shadow.offset_y - shadow.spread_radius,
                    box.width + shadow.spread_radius * 2,
                    box.height + shadow.spread_radius * 2,
                    border_radius);
            } else {
                nvgRect(vg_,
                    box.x + shadow.offset_x - shadow.spread_radius,
                    box.y + shadow.offset_y - shadow.spread_radius,
                    box.width + shadow.spread_radius * 2,
                    box.height + shadow.spread_radius * 2);
            }
            nvgPathWinding(vg_, NVG_HOLE);

            nvgFillPaint(vg_, shadow_paint);
            nvgFill(vg_);

            nvgRestore(vg_);
        } else {
            // Outset shadows (existing implementation)
            nvgSave(vg_);

            // Create shadow paint with blur
            // nvgBoxGradient creates a soft shadow effect with feathered edges
            NVGpaint shadow_paint = nvgBoxGradient(vg_,
                box.x + shadow.offset_x,
                box.y + shadow.offset_y,
                box.width + shadow.spread_radius * 2,
                box.height + shadow.spread_radius * 2,
                border_radius + shadow.spread_radius,  // Corner radius
                shadow.blur_radius,                     // Blur feather
                shadow.color,                           // Inner color
                nvgRGBA(0, 0, 0, 0)                     // Outer color (transparent)
            );

            // Draw shadow rectangle (expanded by blur to accommodate feathering)
            nvgBeginPath(vg_);
            float shadow_margin = shadow.blur_radius + 5.0f;
            nvgRect(vg_,
                box.x + shadow.offset_x - shadow_margin,
                box.y + shadow.offset_y - shadow_margin,
                box.width + shadow.spread_radius * 2 + shadow_margin * 2,
                box.height + shadow.spread_radius * 2 + shadow_margin * 2);

            // Cut out the center (where the actual element will be)
            // This creates a "frame" effect so the shadow only shows outside the element
            if (border_radius > 0) {
                nvgRoundedRect(vg_, box.x, box.y, box.width, box.height, border_radius);
            } else {
                nvgRect(vg_, box.x, box.y, box.width, box.height);
            }
            nvgPathWinding(vg_, NVG_HOLE);

            nvgFillPaint(vg_, shadow_paint);
            nvgFill(vg_);

            nvgRestore(vg_);
        }
    }
}

NVGpaint cssboxPainter::create_gradient(const std::string& gradient_css,
                                        const cssboxBox& box) {
    // Phase 3: Parse gradient and dispatch to appropriate handler
    GradientData gradient = parse_gradient(gradient_css);

    if (gradient.type == GradientData::LINEAR) {
        return create_linear_gradient(gradient, box);
    } else {
        return create_radial_gradient(gradient, box);
    }
}

// Helper: Parse color from gradient lexer tokens
static NVGcolor parse_gradient_color(CSSGradientLexer* lexer, CSSGradientToken* token) {
    if (token->type == GRAD_HEX_COLOR) {
        std::string hex(token->start, token->length);
        return cssbox_utils::parse_color(hex);
    }
    else if (token->type == GRAD_NAMED_COLOR) {
        std::string name(token->start, token->length);
        return cssbox_utils::parse_color(name);
    }
    else if (token->type == GRAD_RGB || token->type == GRAD_RGBA) {
        // Collect rgb(r,g,b) or rgba(r,g,b,a)
        std::string color_str(token->start, token->length);
        *token = CSSGradientLexer_next_token(lexer);
        if (token->type == GRAD_LPAREN) {
            color_str += "(";
            while (token->type != GRAD_END) {
                *token = CSSGradientLexer_next_token(lexer);
                if (token->type == GRAD_RPAREN) {
                    color_str += ")";
                    break;
                }
                color_str += std::string(token->start, token->length);
            }
        }
        return cssbox_utils::parse_color(color_str);
    }
    return nvgRGB(0, 0, 0);
}

GradientData cssboxPainter::parse_gradient(const std::string& gradient_css) {
    GradientData result;
    CSSGradientLexer lexer;
    CSSGradientLexer_init(&lexer, gradient_css.c_str());

    CSSGradientToken token = CSSGradientLexer_next_token(&lexer);

    // Determine gradient type
    if (token.type == GRAD_LINEAR || token.type == GRAD_REPEATING_LINEAR) {
        result.type = GradientData::LINEAR;
    } else if (token.type == GRAD_RADIAL || token.type == GRAD_REPEATING_RADIAL) {
        result.type = GradientData::RADIAL;
    } else {
        return result;
    }

    // Skip opening parenthesis
    token = CSSGradientLexer_next_token(&lexer);
    if (token.type != GRAD_LPAREN) return result;

    token = CSSGradientLexer_next_token(&lexer);

    // Parse direction/shape/position
    if (result.type == GradientData::LINEAR) {
        // Linear gradient: check for "to" direction or angle
        if (token.type == GRAD_TO) {
            token = CSSGradientLexer_next_token(&lexer);
            float angle = 180.0f;  // default: to bottom

            // Collect direction keywords
            bool has_top = false, has_bottom = false, has_left = false, has_right = false;
            while (token.type == GRAD_TOP || token.type == GRAD_BOTTOM ||
                   token.type == GRAD_LEFT || token.type == GRAD_RIGHT) {
                if (token.type == GRAD_TOP) has_top = true;
                else if (token.type == GRAD_BOTTOM) has_bottom = true;
                else if (token.type == GRAD_LEFT) has_left = true;
                else if (token.type == GRAD_RIGHT) has_right = true;
                token = CSSGradientLexer_next_token(&lexer);
            }

            // Convert to angle
            if (has_top && has_right) angle = 45.0f;
            else if (has_top && has_left) angle = 315.0f;
            else if (has_bottom && has_right) angle = 135.0f;
            else if (has_bottom && has_left) angle = 225.0f;
            else if (has_top) angle = 0.0f;
            else if (has_bottom) angle = 180.0f;
            else if (has_right) angle = 90.0f;
            else if (has_left) angle = 270.0f;

            result.angle = angle;
        }
        else if (token.type == GRAD_DEG) {
            result.angle = token.value;
            token = CSSGradientLexer_next_token(&lexer);
        }
        else if (token.type == GRAD_RAD) {
            result.angle = token.value * 180.0f / 3.14159f;
            token = CSSGradientLexer_next_token(&lexer);
        }
        else if (token.type == GRAD_TURN) {
            result.angle = token.value * 360.0f;
            token = CSSGradientLexer_next_token(&lexer);
        }
    }
    else {
        // Radial gradient: check for shape and position
        if (token.type == GRAD_CIRCLE) {
            result.shape = "circle";
            token = CSSGradientLexer_next_token(&lexer);
        } else if (token.type == GRAD_ELLIPSE) {
            result.shape = "ellipse";
            token = CSSGradientLexer_next_token(&lexer);
        }

        // Check for "at" position
        if (token.type == GRAD_AT) {
            token = CSSGradientLexer_next_token(&lexer);
            std::string position;
            while (token.type != GRAD_COMMA && token.type != GRAD_END && token.type != GRAD_RPAREN) {
                if (!position.empty()) position += " ";
                position += std::string(token.start, token.length);
                token = CSSGradientLexer_next_token(&lexer);
            }
            result.position = position;
        }
    }

    // Skip comma after direction/shape if present
    if (token.type == GRAD_COMMA) {
        token = CSSGradientLexer_next_token(&lexer);
    }

    // Parse color stops
    std::vector<std::pair<NVGcolor, float>> stops;
    float position = -1.0f;

    while (token.type != GRAD_END && token.type != GRAD_RPAREN) {
        NVGcolor color = nvgRGB(0, 0, 0);
        position = -1.0f;

        // Parse color
        if (token.type == GRAD_HEX_COLOR || token.type == GRAD_NAMED_COLOR ||
            token.type == GRAD_RGB || token.type == GRAD_RGBA) {
            color = parse_gradient_color(&lexer, &token);
            token = CSSGradientLexer_next_token(&lexer);
        }

        // Parse optional position
        if (token.type == GRAD_PERCENT) {
            position = token.value / 100.0f;
            token = CSSGradientLexer_next_token(&lexer);
        } else if (token.type == GRAD_PX) {
            // Pixel positions need box context, store raw for now
            position = token.value / 100.0f;  // Approximate
            token = CSSGradientLexer_next_token(&lexer);
        }

        stops.push_back({color, position});

        // Skip comma
        if (token.type == GRAD_COMMA) {
            token = CSSGradientLexer_next_token(&lexer);
        }
    }

    // Distribute positions for stops without explicit position
    size_t num_stops = stops.size();
    for (size_t i = 0; i < num_stops; ++i) {
        if (stops[i].second < 0.0f) {
            stops[i].second = (num_stops > 1) ? static_cast<float>(i) / (num_stops - 1) : 0.0f;
        }
    }

    // Convert to GradientStop
    for (const auto& [color, pos] : stops) {
        result.stops.push_back({pos, color});
    }

    // Ensure at least 2 stops
    if (result.stops.size() < 2) {
        if (result.stops.empty()) {
            result.stops.push_back({0.0f, nvgRGB(0, 0, 0)});
        }
        result.stops.push_back({1.0f, nvgRGB(255, 255, 255)});
    }

    return result;
}

NVGpaint cssboxPainter::create_linear_gradient(const GradientData& gradient,
                                                const cssboxBox& box) {
    // Convert angle to start/end points
    float rad = (gradient.angle - 90.0f) * 3.14159f / 180.0f;
    float cx = box.x + box.width / 2.0f;
    float cy = box.y + box.height / 2.0f;
    float len = std::max(box.width, box.height);

    float sx = cx - std::cos(rad) * len / 2.0f;
    float sy = cy - std::sin(rad) * len / 2.0f;
    float ex = cx + std::cos(rad) * len / 2.0f;
    float ey = cy + std::sin(rad) * len / 2.0f;

    // NanoVG only supports 2-color gradients
    // Use first and last stop
    NVGcolor start_color = gradient.stops.front().color;
    NVGcolor end_color = gradient.stops.back().color;

    return nvgLinearGradient(vg_, sx, sy, ex, ey, start_color, end_color);
}

NVGpaint cssboxPainter::create_radial_gradient(const GradientData& gradient,
                                                const cssboxBox& box) {
    // Parse position
    float cx = box.x + box.width / 2.0f;   // Default: center
    float cy = box.y + box.height / 2.0f;

    // Parse position string (e.g., "center center", "top left", "50% 50%")
    if (!gradient.position.empty()) {
        std::istringstream pos_stream(gradient.position);
        std::string x_pos, y_pos;
        pos_stream >> x_pos >> y_pos;

        // Parse X position
        if (x_pos == "left") cx = box.x;
        else if (x_pos == "center") cx = box.x + box.width / 2.0f;
        else if (x_pos == "right") cx = box.x + box.width;
        else if (x_pos.find('%') != std::string::npos) {
            cx = box.x + (safe_stof(x_pos) / 100.0f) * box.width;
        } else if (x_pos.find("px") != std::string::npos) {
            cx = box.x + safe_stof(x_pos);
        }

        // Parse Y position
        if (y_pos == "top") cy = box.y;
        else if (y_pos == "center") cy = box.y + box.height / 2.0f;
        else if (y_pos == "bottom") cy = box.y + box.height;
        else if (y_pos.find('%') != std::string::npos) {
            cy = box.y + (safe_stof(y_pos) / 100.0f) * box.height;
        } else if (y_pos.find("px") != std::string::npos) {
            cy = box.y + safe_stof(y_pos);
        }
    }

    // Compute radius
    float radius;
    if (gradient.shape == "circle") {
        radius = std::min(box.width, box.height) / 2.0f;
    } else {
        // Ellipse - use average radius
        radius = (box.width + box.height) / 4.0f;
    }

    // NanoVG only supports 2-color radial gradients
    NVGcolor inner_color = gradient.stops.front().color;
    NVGcolor outer_color = gradient.stops.back().color;

    return nvgRadialGradient(vg_, cx, cy, 0, radius, inner_color, outer_color);
}

void cssboxPainter::create_rounded_rect_path(const cssboxBox& box) {
    nvgBeginPath(vg_);

    // Check if any border radius is specified
    bool has_radius = (box.border_radius[0] > 0 || box.border_radius[1] > 0 ||
                       box.border_radius[2] > 0 || box.border_radius[3] > 0);

    if (has_radius) {
        // Sprint 36: Per-corner border radius
        // Order: [0]=top-left, [1]=top-right, [2]=bottom-right, [3]=bottom-left
        float tl = box.border_radius[0];  // top-left
        float tr = box.border_radius[1];  // top-right
        float br = box.border_radius[2];  // bottom-right
        float bl = box.border_radius[3];  // bottom-left

        float x = box.x;
        float y = box.y;
        float w = box.width;
        float h = box.height;

        // Create path with per-corner radii using arc commands
        // Start from top-left corner, move clockwise

        // Top-left corner
        nvgMoveTo(vg_, x + tl, y);

        // Top edge
        nvgLineTo(vg_, x + w - tr, y);

        // Top-right corner
        if (tr > 0) {
            nvgArc(vg_, x + w - tr, y + tr, tr, -NVG_PI/2, 0, NVG_CW);
        }

        // Right edge
        nvgLineTo(vg_, x + w, y + h - br);

        // Bottom-right corner
        if (br > 0) {
            nvgArc(vg_, x + w - br, y + h - br, br, 0, NVG_PI/2, NVG_CW);
        }

        // Bottom edge
        nvgLineTo(vg_, x + bl, y + h);

        // Bottom-left corner
        if (bl > 0) {
            nvgArc(vg_, x + bl, y + h - bl, bl, NVG_PI/2, NVG_PI, NVG_CW);
        }

        // Left edge
        nvgLineTo(vg_, x, y + tl);

        // Top-left corner (closing)
        if (tl > 0) {
            nvgArc(vg_, x + tl, y + tl, tl, NVG_PI, 3*NVG_PI/2, NVG_CW);
        }

        nvgClosePath(vg_);
    } else {
        nvgRect(vg_, box.x, box.y, box.width, box.height);
    }
}


// ============================================================================
// Typography Support (Sprint 11)
// ============================================================================

std::string cssboxPainter::compute_font_face(const cssboxElement* element) {
    // Get base font family from typed property
    std::string family = element->style.font_family;

    // font_weight is typed enum - check if bold
    bool is_bold = (element->style.font_weight >= cssbox::FontWeight::BOLD);  // 700+

    // font_style is typed enum
    bool is_italic = (element->style.font_style == cssbox::FontStyle::ITALIC ||
                     element->style.font_style == cssbox::FontStyle::OBLIQUE);

    // Apply weight and style suffixes
    if (is_bold && is_italic) {
        family += "-BoldItalic";
    } else if (is_bold) {
        family += "-Bold";
    } else if (is_italic) {
        family += "-Italic";
    }

    return family;
}

int cssboxPainter::compute_text_align(const cssboxElement* element) {
    int h_align = NVG_ALIGN_LEFT;
    int v_align = NVG_ALIGN_TOP;

    // Horizontal alignment from typed enum
    switch (element->style.text_align) {
        case cssbox::TextAlign::LEFT:   h_align = NVG_ALIGN_LEFT; break;
        case cssbox::TextAlign::CENTER: h_align = NVG_ALIGN_CENTER; break;
        case cssbox::TextAlign::RIGHT:  h_align = NVG_ALIGN_RIGHT; break;
        case cssbox::TextAlign::JUSTIFY: h_align = NVG_ALIGN_LEFT; break;  // NanoVG doesn't support justify
    }

    // Retain mode: Use typed enum for O(1) lookup instead of map lookup
    switch (element->style.vertical_align) {
        case cssbox::VerticalAlign::TOP:      v_align = NVG_ALIGN_TOP; break;
        case cssbox::VerticalAlign::MIDDLE:   v_align = NVG_ALIGN_MIDDLE; break;
        case cssbox::VerticalAlign::BOTTOM:   v_align = NVG_ALIGN_BOTTOM; break;
        case cssbox::VerticalAlign::BASELINE: v_align = NVG_ALIGN_BASELINE; break;
    }

    return h_align | v_align;
}

std::string cssboxPainter::apply_text_transform(const std::string& text,
                                                 const std::string& transform) {
    if (transform == "uppercase") {
        std::string result = text;
        for (char& c : result) {
            c = std::toupper(c);
        }
        return result;
    } else if (transform == "lowercase") {
        std::string result = text;
        for (char& c : result) {
            c = std::tolower(c);
        }
        return result;
    } else if (transform == "capitalize") {
        std::string result = text;
        bool capitalize_next = true;
        for (char& c : result) {
            if (std::isspace(c)) {
                capitalize_next = true;
            } else if (capitalize_next) {
                c = std::toupper(c);
                capitalize_next = false;
            } else {
                c = std::tolower(c);
            }
        }
        return result;
    }

    return text;  // "none" or unrecognized
}

void cssboxPainter::apply_text_decoration(const cssboxElement* element,
                                           float text_x, float text_y,
                                           NVGcolor text_color) {
    // Retain mode: Use typed enum for O(1) lookup instead of map lookup
    if (element->style.text_decoration == cssbox::TextDecoration::NONE) {
        return;  // No text decoration
    }

    // Get text bounds
    float bounds[4];
    nvgTextBounds(vg_, text_x, text_y, element->text_content.c_str(), nullptr, bounds);

    float line_y;
    switch (element->style.text_decoration) {
        case cssbox::TextDecoration::UNDERLINE:
            line_y = bounds[3] + 2;  // Below text
            break;
        case cssbox::TextDecoration::LINE_THROUGH:
            line_y = (bounds[1] + bounds[3]) / 2.0f;  // Middle
            break;
        case cssbox::TextDecoration::OVERLINE:
            line_y = bounds[1] - 2;  // Above text
            break;
        default:
            return;  // NONE or unrecognized
    }

    // Draw decoration line
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, bounds[0], line_y);
    nvgLineTo(vg_, bounds[2], line_y);
    nvgStrokeColor(vg_, text_color);
    nvgStrokeWidth(vg_, 1.0f);
    nvgStroke(vg_);
}

cssboxPainter::StrokeStyle cssboxPainter::resolve_stroke_style(
    const cssboxElement* element,
    const cssboxBox* box,
    float absolute_hint) const {

    StrokeStyle stroke;
    float reference = absolute_hint;
    if (box) {
        reference = std::max(box->width, box->height);
    }

    // === Hybrid approach: Prefer typed properties, fallback to inline_style ===
    // SVG elements often set stroke via inline_style (from XML attributes)
    // CSS elements use typed properties from stylesheet

    const auto& svg_stroke = element->style.svg_stroke;

    // Check if typed properties are set (from CSS)
    if (svg_stroke.enabled) {
        stroke.enabled = true;
        stroke.color = svg_stroke.color;
        stroke.width = svg_stroke.width;
        stroke.miter_limit = svg_stroke.miter_limit;
        stroke.dash_array = svg_stroke.dash_array;
        stroke.dash_offset = svg_stroke.dash_offset;

        switch (svg_stroke.line_cap) {
            case cssbox::SVGStroke::LineCap::ROUND:  stroke.line_cap = NVG_ROUND; break;
            case cssbox::SVGStroke::LineCap::SQUARE: stroke.line_cap = NVG_SQUARE; break;
            default: stroke.line_cap = NVG_BUTT; break;
        }
        switch (svg_stroke.line_join) {
            case cssbox::SVGStroke::LineJoin::ROUND: stroke.line_join = NVG_ROUND; break;
            case cssbox::SVGStroke::LineJoin::BEVEL: stroke.line_join = NVG_BEVEL; break;
            default: stroke.line_join = NVG_MITER; break;
        }
    }
    // Fallback: Read from inline_style (SVG attributes)
    else {
        auto stroke_it = element->inline_style.find("stroke");
        if (stroke_it != element->inline_style.end()) {
            const std::string& value = stroke_it->second;
            if (value != "none" && value != "transparent") {
                stroke.color = cssbox_utils::parse_color(value);
                stroke.enabled = true;
            }
        }

        auto width_it = element->inline_style.find("stroke-width");
        if (width_it != element->inline_style.end()) {
            float parsed_width = cssbox_utils::parse_length(width_it->second, reference);
            if (parsed_width > 0.0f) {
                stroke.width = parsed_width;
                if (stroke.enabled || stroke.color.a > 0) stroke.enabled = true;
            }
        }

        auto cap_it = element->inline_style.find("stroke-linecap");
        if (cap_it != element->inline_style.end()) {
            const std::string& cap = cap_it->second;
            if (cap == "round") stroke.line_cap = NVG_ROUND;
            else if (cap == "square") stroke.line_cap = NVG_SQUARE;
        }

        auto join_it = element->inline_style.find("stroke-linejoin");
        if (join_it != element->inline_style.end()) {
            const std::string& join = join_it->second;
            if (join == "round") stroke.line_join = NVG_ROUND;
            else if (join == "bevel") stroke.line_join = NVG_BEVEL;
        }

        auto miter_it = element->inline_style.find("stroke-miterlimit");
        if (miter_it != element->inline_style.end()) {
            float limit = safe_stof(miter_it->second, stroke.miter_limit);
            if (limit > 0.0f) stroke.miter_limit = limit;
        }

        auto dash_it = element->inline_style.find("stroke-dasharray");
        if (dash_it != element->inline_style.end() && dash_it->second != "none") {
            std::string normalized = dash_it->second;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');
            std::stringstream ss(normalized);
            std::string token;
            while (ss >> token) {
                float dash_value = cssbox_utils::parse_length(token, stroke.width);
                if (dash_value > 0.0f) {
                    stroke.dash_array.push_back(dash_value);
                }
            }
        }

        auto offset_it = element->inline_style.find("stroke-dashoffset");
        if (offset_it != element->inline_style.end()) {
            stroke.dash_offset = cssbox_utils::parse_length(offset_it->second, stroke.width);
        }
    }

    return stroke;
}

void cssboxPainter::paint_rect_stroke(const cssboxElement* element,
                                      const cssboxBox& box) {
    auto stroke = resolve_stroke_style(element, &box, std::max(box.width, box.height));
    if (!stroke.enabled || stroke.width <= 0.0f) return;

    std::vector<Vec2> corners = {
        {box.x, box.y},
        {box.x + box.width, box.y},
        {box.x + box.width, box.y + box.height},
        {box.x, box.y + box.height}
    };

    stroke_polyline(vg_, stroke, corners, true);
}

static bool extract_coordinate(const cssboxElement* element,
                               const char* key,
                               float context,
                               float origin,
                               float& out_value) {
    // TODO: SVG coordinate properties not in typed system
    // Reading from inline_style temporarily
    auto it = element->inline_style.find(key);
    if (it == element->inline_style.end()) {
        return false;
    }

    // SVG coordinates are relative to viewBox, not screen
    // The origin offset is only added if there's no parent SVG transform
    out_value = cssbox_utils::parse_length(it->second, context);
    return true;
}

void cssboxPainter::paint_line_shape(const cssboxElement* element,
                                     const cssboxBox& box) {
    float diagonal = std::sqrt(box.width * box.width + box.height * box.height);
    auto stroke = resolve_stroke_style(element, &box, diagonal);
    if (!stroke.enabled || stroke.width <= 0.0f) return;

    Vec2 start {box.x, box.y};
    Vec2 end {box.x + box.width, box.y + box.height};

    if (element->line_geometry.defined) {
        start = {element->line_geometry.x1, element->line_geometry.y1};
        end = {element->line_geometry.x2, element->line_geometry.y2};
    } else {
        extract_coordinate(element, "x1", box.width, box.x, start.x);
        extract_coordinate(element, "y1", box.height, box.y, start.y);
        extract_coordinate(element, "x2", box.width, box.x, end.x);
        extract_coordinate(element, "y2", box.height, box.y, end.y);
    }

    // Check if rough rendering is enabled
    bool use_rough = (element->style.svg_stroke.rendering == cssbox::StrokeRendering::ROUGH);
    
    if (use_rough) {
        // Use rough rendering for line
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.stroke_enabled = 1;
        opts.stroke_color = stroke.color;
        opts.stroke_width = stroke.width;
        opts.fill_enabled = 0;
        opts.roughness = element->style.svg_stroke.roughness;
        opts.bowing = element->style.svg_stroke.bowing;
        opts.stroke_count = element->style.svg_stroke.stroke_count;
        opts.seed = element->style.svg_stroke.seed;
        
        nvgRoughLine(vg_, start.x, start.y, end.x, end.y, opts);
        return;
    }

    if (element->has_stroke_salt) {
        float amplitude = stroke.width * 0.35f;
        start = jitter_with_salt(start, element->stroke_salt, amplitude, 1.0f);
        end = jitter_with_salt(end, element->stroke_salt, amplitude, 2.0f);
    }

    std::vector<Vec2> line = {start, end};
    stroke_polyline(vg_, stroke, line, false);
    
    // SVG Markers: Render markers at line endpoints
    // Retain mode: Use existing extract_url_id() instead of duplicate lambda
    auto marker_start_it = element->inline_style.find("marker-start");
    if (marker_start_it != element->inline_style.end()) {
        std::string marker_id = extract_url_id(marker_start_it->second);
        auto marker_it = renderer_->markers_.find(marker_id);
        if (marker_it != renderer_->markers_.end()) {
            float angle = atan2f(end.y - start.y, end.x - start.x);
            render_marker(marker_it->second, start.x, start.y, angle);
        }
    }

    auto marker_end_it = element->inline_style.find("marker-end");
    if (marker_end_it != element->inline_style.end()) {
        std::string marker_id = extract_url_id(marker_end_it->second);
        auto marker_it = renderer_->markers_.find(marker_id);
        if (marker_it != renderer_->markers_.end()) {
            float angle = atan2f(end.y - start.y, end.x - start.x);
            render_marker(marker_it->second, end.x, end.y, angle);
        }
    }
}

void cssboxPainter::paint_circle_shape(const cssboxElement* element,
                                       const cssboxBox& box) {
    float cx = box.x + box.width * 0.5f;
    float cy = box.y + box.height * 0.5f;
    float rx = box.width * 0.5f;
    float ry = box.height * 0.5f;

    // Retain mode: Prefer typed geometry (O(1)), fallback to inline_style only if not defined
    if (element->circle_geometry.defined) {
        cx = element->circle_geometry.cx;
        cy = element->circle_geometry.cy;
        rx = element->circle_geometry.rx > 0 ? element->circle_geometry.rx : rx;
        ry = element->circle_geometry.ry > 0 ? element->circle_geometry.ry : ry;
    } else {
        // Fallback: Parse from inline_style (SVG attributes)
        extract_coordinate(element, "cx", box.width, box.x, cx);
        extract_coordinate(element, "cy", box.height, box.y, cy);

        auto r_it = element->inline_style.find("r");
        if (r_it != element->inline_style.end()) {
            float r_value = cssbox_utils::parse_length(r_it->second, std::max(box.width, box.height));
            if (r_value > 0.0f) {
                rx = ry = r_value;
            }
        }
        auto rx_it = element->inline_style.find("rx");
        if (rx_it != element->inline_style.end()) {
            rx = cssbox_utils::parse_length(rx_it->second, box.width);
        }
        auto ry_it = element->inline_style.find("ry");
        if (ry_it != element->inline_style.end()) {
            ry = cssbox_utils::parse_length(ry_it->second, box.height);
        }
    }

    // Check if rough rendering is enabled
    bool use_rough = (element->style.svg_stroke.rendering == cssbox::StrokeRendering::ROUGH);

    // Debug: Log element classes and fill from typed style
    std::string classes_str;
    for (const auto& cls : element->classes) {
        classes_str += cls + " ";
    }

    // logi("[PAINTER] paint_circle id='{}' classes='{}' svg_fill.enabled={} svg_fill.color=({:.2f},{:.2f},{:.2f},{:.2f})",
    //      element->id, classes_str,
    //      element->style.svg_fill.enabled,
    //      element->style.svg_fill.color.r, element->style.svg_fill.color.g,
    //      element->style.svg_fill.color.b, element->style.svg_fill.color.a);

    // === Fill priority: pattern > gradient > typed solid > inline solid ===
    bool has_fill = false;

    // 1. Check for pattern fill FIRST (url(#pattern-id) in inline_style)
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() &&
        fill_it->second.find("url(#") != std::string::npos) {
        cssboxBox fill_box;
        fill_box.x = cx - rx;
        fill_box.y = cy - ry;
        fill_box.width = rx * 2;
        fill_box.height = ry * 2;

        nvgBeginPath(vg_);
        if (std::abs(rx - ry) < 0.001f) {
            nvgCircle(vg_, cx, cy, rx);
        } else {
            nvgEllipse(vg_, cx, cy, rx, ry);
        }
        has_fill = apply_pattern_fill(fill_it->second, fill_box);
    }

    // 2. Check for cached gradient (O(1) pointer check)
    if (!has_fill && element->cached_fill_gradient) {
        nvgBeginPath(vg_);
        if (std::abs(rx - ry) < 0.001f) {
            nvgCircle(vg_, cx, cy, rx);
        } else {
            nvgEllipse(vg_, cx, cy, rx, ry);
        }

        cssboxBox gradient_box;
        gradient_box.x = cx - rx;
        gradient_box.y = cy - ry;
        gradient_box.width = rx * 2;
        gradient_box.height = ry * 2;

        NVGpaint gradient_paint;
        if (element->cached_fill_gradient->type == GradientData::LINEAR) {
            gradient_paint = create_linear_gradient(*element->cached_fill_gradient, gradient_box);
        } else {
            gradient_paint = create_radial_gradient(*element->cached_fill_gradient, gradient_box);
        }

        nvgFillPaint(vg_, gradient_paint);
        nvgFill(vg_);
        has_fill = true;
    }

    // 3. Check for typed svg_fill (from CSS)
    if (!has_fill && element->style.svg_fill.enabled) {
        NVGcolor fill_color = element->style.svg_fill.color;

        nvgBeginPath(vg_);
        if (std::abs(rx - ry) < 0.001f) {
            nvgCircle(vg_, cx, cy, rx);
        } else {
            nvgEllipse(vg_, cx, cy, rx, ry);
        }

        if (use_rough) {
            NVGRoughOptions opts = nvgRoughDefaultOptions();
            opts.fill_enabled = 1;
            opts.fill_color = fill_color;
            opts.stroke_enabled = 0;
            opts.roughness = element->style.svg_stroke.roughness;
            opts.seed = element->style.svg_stroke.seed;

            if (std::abs(rx - ry) < 0.001f) {
                nvgRoughCircle(vg_, cx, cy, rx, opts);
            } else {
                nvgRoughEllipse(vg_, cx, cy, rx, ry, opts);
            }
        } else {
            nvgFillColor(vg_, fill_color);
            nvgFill(vg_);
        }
        has_fill = true;
    }

    // 4. Fallback: solid color from inline_style
    if (!has_fill && fill_it != element->inline_style.end() &&
        fill_it->second != "none") {
        NVGcolor fill_color = cssbox_utils::parse_color(fill_it->second);
        nvgBeginPath(vg_);
        if (std::abs(rx - ry) < 0.001f) {
            nvgCircle(vg_, cx, cy, rx);
        } else {
            nvgEllipse(vg_, cx, cy, rx, ry);
        }
        nvgFillColor(vg_, fill_color);
        nvgFill(vg_);
    }

    auto stroke = resolve_stroke_style(element, &box, std::max(rx, ry));
    
    // printf("[PAINTER] Stroke resolved: enabled=%d width=%.2f use_rough=%d\n", 
    //        stroke.enabled, stroke.width, use_rough);
    
    if (!stroke.enabled || stroke.width <= 0.0f) {
        // printf("[PAINTER] Skipping stroke - not enabled or zero width\n");
        return;
    }
    
    if (use_rough) {
        // printf("[PAINTER] *** USING ROUGH RENDERING ***\n");
        // Use rough rendering for stroke
        NVGRoughOptions opts = nvgRoughDefaultOptions();
        opts.stroke_enabled = 1;
        opts.stroke_color = stroke.color;
        opts.stroke_width = stroke.width;
        opts.fill_enabled = 0;
        opts.roughness = element->style.svg_stroke.roughness;
        opts.bowing = element->style.svg_stroke.bowing;
        opts.stroke_count = element->style.svg_stroke.stroke_count;
        opts.seed = element->style.svg_stroke.seed;
        
        if (std::abs(rx - ry) < 0.001f) {
            nvgRoughCircle(vg_, cx, cy, rx, opts);
        } else {
            nvgRoughEllipse(vg_, cx, cy, rx, ry, opts);
        }
        return;
    }

    // Convert to polyline for hand-drawn effect or dashed strokes
    const int segments = 96;
    std::vector<Vec2> outline;
    outline.reserve(segments);
    
    for (int i = 0; i < segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float angle = t * 2.0f * kPi;
        float x = cx + std::cos(angle) * rx;
        float y = cy + std::sin(angle) * ry;
        
        if (element->has_stroke_salt) {
            // Excalidraw-style: perpendicular offset based on tangent
            float next_angle = ((i + 1) % segments) * 2.0f * kPi / segments;
            Vec2 tangent = {
                std::cos(next_angle) * rx - x,
                std::sin(next_angle) * ry - y
            };
            float roughness = std::max(rx, ry) * 0.015f + stroke.width * 0.4f;
            Vec2 offset = offset_point({x, y}, tangent, element->stroke_salt, roughness, static_cast<float>(i));
            outline.push_back(offset);
        } else {
            outline.push_back({x, y});
        }
    }
    
    // Apply stroke with width variation for hand-drawn effect (Excalidraw-style)
    if (element->has_stroke_salt && stroke.enabled && stroke.width > 0.0f) {
        // Draw each segment separately with varying width (like RoughJS roughness)
        const int passes = 2;
        
        for (int pass = 0; pass < passes; ++pass) {
            NVGcolor color = stroke.color;
            if (pass > 0) {
                color.a *= 0.4f;  // More transparent second pass
            }
            
            for (size_t i = 0; i < outline.size(); ++i) {
                size_t next = (i + 1) % outline.size();
                
                // Generate dramatic organic width variation (like hand pressure)
                float noise1 = salted_noise(element->stroke_salt + static_cast<float>(i) * 0.08f + pass * 100.0f);
                float noise2 = salted_noise(element->stroke_salt + static_cast<float>(i) * 0.23f + pass * 200.0f);
                float noise3 = salted_noise(element->stroke_salt + static_cast<float>(i) * 0.47f + pass * 300.0f);
                
                // Combine multiple noise frequencies for natural hand-drawn variation
                // Use much larger coefficients for dramatic effect
                float combined_noise = (noise1 - 0.5f) * 2.5f + (noise2 - 0.5f) * 1.2f + (noise3 - 0.5f) * 0.6f;
                float width_var = stroke.width * 0.7f * combined_noise;  // 70% variation range
                float segment_width = stroke.width + width_var;
                
                // Allow very wide range: 30% to 250% of base width (like real hand drawing with pressure)
                segment_width = std::max(segment_width, stroke.width * 0.3f);
                segment_width = std::min(segment_width, stroke.width * 2.5f);
                
                // Draw individual segment with its own width
                nvgBeginPath(vg_);
                nvgMoveTo(vg_, outline[i].x, outline[i].y);
                nvgLineTo(vg_, outline[next].x, outline[next].y);
                nvgStrokeWidth(vg_, segment_width);
                nvgStrokeColor(vg_, color);
                nvgLineCap(vg_, NVG_ROUND);  // Round caps for smooth connections
                nvgLineJoin(vg_, stroke.line_join);
                nvgStroke(vg_);
            }
        }
    } else {
        stroke_polyline(vg_, stroke, outline, true);
    }
}

void cssboxPainter::paint_rect_shape(const cssboxElement* element,
                                     const cssboxBox& box) {
    float x = box.x;
    float y = box.y;
    float w = box.width;
    float h = box.height;
    float rx = 0.0f;
    float ry = 0.0f;

    // Retain mode: Prefer typed geometry (O(1)), fallback to inline_style only if not defined
    if (element->rect_geometry.defined) {
        x = element->rect_geometry.x;
        y = element->rect_geometry.y;
        w = element->rect_geometry.width > 0 ? element->rect_geometry.width : w;
        h = element->rect_geometry.height > 0 ? element->rect_geometry.height : h;
        rx = element->rect_geometry.rx;
        ry = element->rect_geometry.ry;
    } else {
        // Fallback: Parse from inline_style (SVG attributes)
        extract_coordinate(element, "x", box.width, box.x, x);
        extract_coordinate(element, "y", box.height, box.y, y);
        extract_coordinate(element, "width", box.width, 0, w);
        extract_coordinate(element, "height", box.height, 0, h);

        auto rx_it = element->inline_style.find("rx");
        if (rx_it != element->inline_style.end()) {
            rx = cssbox_utils::parse_length(rx_it->second, w);
        }
        auto ry_it = element->inline_style.find("ry");
        if (ry_it != element->inline_style.end()) {
            ry = cssbox_utils::parse_length(ry_it->second, h);
        }
    }

    // === Fill priority: pattern > gradient > typed solid > inline solid ===
    auto create_rect_path = [&]() {
        nvgBeginPath(vg_);
        if (rx > 0 || ry > 0) {
            float radius = (rx > 0 && ry > 0) ? std::max(rx, ry) : (rx > 0 ? rx : ry);
            nvgRoundedRect(vg_, x, y, w, h, radius);
        } else {
            nvgRect(vg_, x, y, w, h);
        }
    };

    cssboxBox fill_box;
    fill_box.x = x;
    fill_box.y = y;
    fill_box.width = w;
    fill_box.height = h;

    bool has_fill = false;

    // 1. Check for pattern fill FIRST (url(#pattern-id) in inline_style)
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() &&
        fill_it->second.find("url(#") != std::string::npos) {
        create_rect_path();
        has_fill = apply_pattern_fill(fill_it->second, fill_box);
    }

    // 2. Check for cached gradient (O(1) pointer check)
    if (!has_fill && element->cached_fill_gradient) {
        create_rect_path();

        NVGpaint gradient_paint;
        if (element->cached_fill_gradient->type == GradientData::LINEAR) {
            gradient_paint = create_linear_gradient(*element->cached_fill_gradient, fill_box);
        } else {
            gradient_paint = create_radial_gradient(*element->cached_fill_gradient, fill_box);
        }

        nvgFillPaint(vg_, gradient_paint);
        nvgFill(vg_);
        has_fill = true;
    }

    // 3. Check for typed svg_fill (from CSS)
    if (!has_fill && element->style.svg_fill.enabled) {
        create_rect_path();
        nvgFillColor(vg_, element->style.svg_fill.color);
        nvgFill(vg_);
        has_fill = true;
    }

    // 4. Fallback: solid color from inline_style
    if (!has_fill && fill_it != element->inline_style.end() &&
        fill_it->second != "none") {
        create_rect_path();
        NVGcolor fill_color = cssbox_utils::parse_color(fill_it->second);
        nvgFillColor(vg_, fill_color);
        nvgFill(vg_);
        has_fill = true;
    }

    cssboxBox stroke_box;
    stroke_box.x = x;
    stroke_box.y = y;
    stroke_box.width = w;
    stroke_box.height = h;
    auto stroke = resolve_stroke_style(element, &stroke_box, std::max(w, h));
    
    if (stroke.enabled && stroke.width > 0.0f) {
        std::vector<Vec2> corners = {
            {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}
        };
        stroke_polyline(vg_, stroke, corners, true);
    }
}

#ifndef NVG_PI
#define NVG_PI 3.14159265358979323846264338327f
#endif

static float cssbox__sqr(float x) { return x*x; }
static float cssbox__vec_mag(float x, float y) { return sqrtf(x*x + y*y); }

static float cssbox__vec_angle(float ux, float uy, float vx, float vy) {
    float sign = (ux * vy - uy * vx < 0) ? -1.0f : 1.0f;
    float um = cssbox__vec_mag(ux, uy);
    float vm = cssbox__vec_mag(vx, vy);
    float dot = ux * vx + uy * vy;
    float div = dot / (um * vm);
    if (div > 1.0f) div = 1.0f;
    if (div < -1.0f) div = -1.0f;
    return sign * acosf(div);
}

static void cssbox__arc_to(NVGcontext* vg, float x1, float y1, float rx, float ry, 
                          float angle, bool large_arc, bool sweep, float x2, float y2) {
    if (rx == 0 || ry == 0) {
        nvgLineTo(vg, x2, y2);
        return;
    }
    
    rx = fabsf(rx);
    ry = fabsf(ry);
    
    float dx2 = (x1 - x2) / 2.0f;
    float dy2 = (y1 - y2) / 2.0f;
    
    float x1p = cosf(angle)*dx2 + sinf(angle)*dy2;
    float y1p = -sinf(angle)*dx2 + cosf(angle)*dy2;
    
    float rxs = cssbox__sqr(rx);
    float rys = cssbox__sqr(ry);
    float x1ps = cssbox__sqr(x1p);
    float y1ps = cssbox__sqr(y1p);
    
    float cr = x1ps/rxs + y1ps/rys;
    if (cr > 1.0f) {
        float s = sqrtf(cr);
        rx *= s;
        ry *= s;
        rxs = cssbox__sqr(rx);
        rys = cssbox__sqr(ry);
    }
    
    float dq = (rxs*y1ps + rys*x1ps);
    float pq = (rxs*rys - dq) / dq;
    float cp = sqrtf(fmaxf(0.0f, pq));
    if (large_arc == sweep) cp = -cp;
    
    float cxp = cp * rx * y1p / ry;
    float cyp = cp * -ry * x1p / rx;
    
    float cx = cosf(angle)*cxp - sinf(angle)*cyp + (x1 + x2)/2.0f;
    float cy = sinf(angle)*cxp + cosf(angle)*cyp + (y1 + y2)/2.0f;
    
    float theta1 = cssbox__vec_angle(1.0f, 0.0f, (x1p - cxp)/rx, (y1p - cyp)/ry);
    float dtheta = cssbox__vec_angle((x1p - cxp)/rx, (y1p - cyp)/ry, (-x1p - cxp)/rx, (-y1p - cyp)/ry);
    
    if (!sweep && dtheta > 0) dtheta -= NVG_PI*2;
    else if (sweep && dtheta < 0) dtheta += NVG_PI*2;
    
    int segments = (int)ceilf(fabsf(dtheta) / (NVG_PI/2.0f));
    float delta = dtheta / segments;
    float t = theta1;
    
    for (int i = 0; i < segments; i++) {
        float t1 = t;
        float t2 = t + delta;
        
        // Calculate kappa for Bezier approximation
        // k = 4/3 * tan(delta/4)
        float k = (4.0f/3.0f) * tanf(delta/4.0f);
        
        float cos_t1 = cosf(t1), sin_t1 = sinf(t1);
        float cos_t2 = cosf(t2), sin_t2 = sinf(t2);
        
        // Unit circle points
        float p1x = cos_t1, p1y = sin_t1;
        float p2x = cos_t2, p2y = sin_t2;
        
        // Control points on unit circle
        float cp1x = p1x - k * p1y;
        float cp1y = p1y + k * p1x;
        float cp2x = p2x + k * p2y;
        float cp2y = p2y - k * p2x;
        
        // Transform to ellipse and rotate/translate
        auto transform = [&](float px, float py, float& outx, float& outy) {
            float ex = rx * px;
            float ey = ry * py;
            outx = cosf(angle)*ex - sinf(angle)*ey + cx;
            outy = sinf(angle)*ex + cosf(angle)*ey + cy;
        };
        
        float bez_cp1x, bez_cp1y, bez_cp2x, bez_cp2y, bez_endx, bez_endy;
        transform(cp1x, cp1y, bez_cp1x, bez_cp1y);
        transform(cp2x, cp2y, bez_cp2x, bez_cp2y);
        transform(p2x, p2y, bez_endx, bez_endy);
        
        nvgBezierTo(vg, bez_cp1x, bez_cp1y, bez_cp2x, bez_cp2y, bez_endx, bez_endy);
        
        t += delta;
    }
}

void cssboxPainter::paint_svg_path(const cssboxElement* element, const cssboxBox& box) {
    auto d_it = element->inline_style.find("d");
    if (d_it == element->inline_style.end() || d_it->second.empty()) return;

    auto commands = cssbox::SVGPathParser::parse(d_it->second);
    if (commands.empty()) return;

    nvgBeginPath(vg_);

    float current_x = 0, current_y = 0;
    float start_x = 0, start_y = 0;
    float last_control_x = 0, last_control_y = 0;

    // Track vertices for marker-mid rendering
    std::vector<PathVertex> vertices;
    float first_x = 0, first_y = 0;
    bool has_first = false;

    for (const auto& cmd : commands) {
        float x, y, x1, y1, x2, y2;

        switch (cmd.type) {
            case 'M':  // MoveTo
                x = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                nvgMoveTo(vg_, x, y);
                current_x = start_x = x;
                current_y = start_y = y;
                last_control_x = current_x;
                last_control_y = current_y;
                if (!has_first) {
                    first_x = x;
                    first_y = y;
                    has_first = true;
                }
                break;

            case 'L':  // LineTo
                x = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                nvgLineTo(vg_, x, y);
                {
                    float angle = atan2f(y - current_y, x - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, angle, angle);
                }
                current_x = x;
                current_y = y;
                last_control_x = current_x;
                last_control_y = current_y;
                break;

            case 'H':  // Horizontal line
                x = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                nvgLineTo(vg_, x, current_y);
                {
                    float angle = atan2f(0, x - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, angle, angle);
                }
                current_x = x;
                last_control_x = current_x;
                last_control_y = current_y;
                break;

            case 'V':  // Vertical line
                y = cmd.relative ? current_y + cmd.params[0] : cmd.params[0];
                nvgLineTo(vg_, current_x, y);
                {
                    float angle = atan2f(y - current_y, 0);
                    track_path_vertex(vertices, has_first, current_x, current_y, angle, angle);
                }
                current_y = y;
                last_control_x = current_x;
                last_control_y = current_y;
                break;

            case 'C':  // Cubic Bezier
                x1 = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y1 = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                x2 = cmd.relative ? current_x + cmd.params[2] : cmd.params[2];
                y2 = cmd.relative ? current_y + cmd.params[3] : cmd.params[3];
                x = cmd.relative ? current_x + cmd.params[4] : cmd.params[4];
                y = cmd.relative ? current_y + cmd.params[5] : cmd.params[5];
                nvgBezierTo(vg_, x1, y1, x2, y2, x, y);
                {
                    float end_tangent = atan2f(y - y2, x - x2);
                    float start_tangent = atan2f(y1 - current_y, x1 - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, end_tangent, start_tangent);
                }
                last_control_x = x2;
                last_control_y = y2;
                current_x = x;
                current_y = y;
                break;
            
            case 'S': // Smooth Cubic Bezier
                // First control point is reflection of last control point
                x1 = 2 * current_x - last_control_x;
                y1 = 2 * current_y - last_control_y;
                x2 = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y2 = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                x = cmd.relative ? current_x + cmd.params[2] : cmd.params[2];
                y = cmd.relative ? current_y + cmd.params[3] : cmd.params[3];
                nvgBezierTo(vg_, x1, y1, x2, y2, x, y);
                {
                    float end_tangent = atan2f(y - y2, x - x2);
                    float start_tangent = atan2f(y1 - current_y, x1 - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, end_tangent, start_tangent);
                }
                last_control_x = x2;
                last_control_y = y2;
                current_x = x;
                current_y = y;
                break;

            case 'Q':  // Quadratic Bezier
                x1 = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y1 = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                x = cmd.relative ? current_x + cmd.params[2] : cmd.params[2];
                y = cmd.relative ? current_y + cmd.params[3] : cmd.params[3];
                nvgQuadTo(vg_, x1, y1, x, y);
                {
                    float end_tangent = atan2f(y - y1, x - x1);
                    float start_tangent = atan2f(y1 - current_y, x1 - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, end_tangent, start_tangent);
                }
                last_control_x = x1;
                last_control_y = y1;
                current_x = x;
                current_y = y;
                break;
            
            case 'T': // Smooth Quadratic Bezier
                // Control point is reflection of last control point
                x1 = 2 * current_x - last_control_x;
                y1 = 2 * current_y - last_control_y;
                x = cmd.relative ? current_x + cmd.params[0] : cmd.params[0];
                y = cmd.relative ? current_y + cmd.params[1] : cmd.params[1];
                nvgQuadTo(vg_, x1, y1, x, y);
                {
                    float end_tangent = atan2f(y - y1, x - x1);
                    float start_tangent = atan2f(y1 - current_y, x1 - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, end_tangent, start_tangent);
                }
                last_control_x = x1;
                last_control_y = y1;
                current_x = x;
                current_y = y;
                break;
                
            case 'A': // Arc
                {
                    float rx = cmd.params[0];
                    float ry = cmd.params[1];
                    float angle = cmd.params[2] * NVG_PI / 180.0f; // Convert to radians
                    bool large_arc = cmd.params[3] != 0.0f;
                    bool sweep = cmd.params[4] != 0.0f;
                    x = cmd.relative ? current_x + cmd.params[5] : cmd.params[5];
                    y = cmd.relative ? current_y + cmd.params[6] : cmd.params[6];

                    cssbox__arc_to(vg_, current_x, current_y, rx, ry, angle, large_arc, sweep, x, y);

                    float arc_angle = atan2f(y - current_y, x - current_x);
                    track_path_vertex(vertices, has_first, current_x, current_y, arc_angle, arc_angle);

                    current_x = x;
                    current_y = y;
                    last_control_x = current_x;
                    last_control_y = current_y;
                }
                break;

            case 'Z':  // ClosePath
                nvgClosePath(vg_);
                
                // If closing path, connect last point to first point
                if (!vertices.empty() && has_first) {
                    // Update angle_out for the last vertex
                    vertices.back().angle_out = atan2f(start_y - current_y, start_x - current_x);
                    // Update angle_in for the first vertex
                    if (!vertices.empty()) { // Ensure vertices is not empty before accessing front
                        vertices.front().angle_in = vertices.back().angle_out;
                    }
                }
                
                current_x = start_x;
                current_y = start_y;
                last_control_x = current_x;
                last_control_y = current_y;
                break;
        }
    }

    // Apply fill and stroke - hybrid approach for SVG compatibility
    // Check for gradient/pattern first via cached pointer, then typed, then inline_style
    bool has_fill = false;

    if (element->cached_fill_gradient) {
        NVGpaint gradient_paint;
        if (element->cached_fill_gradient->type == GradientData::LINEAR) {
            gradient_paint = create_linear_gradient(*element->cached_fill_gradient, box);
        } else {
            gradient_paint = create_radial_gradient(*element->cached_fill_gradient, box);
        }
        nvgFillPaint(vg_, gradient_paint);
        nvgFill(vg_);
        has_fill = true;
    } else if (element->style.svg_fill.enabled) {
        nvgFillColor(vg_, element->style.svg_fill.color);
        nvgFill(vg_);
        has_fill = true;
    } else {
        // Fallback: check inline_style for SVG elements
        auto fill_it = element->inline_style.find("fill");
        if (fill_it != element->inline_style.end() && fill_it->second != "none") {
            if (apply_pattern_fill(fill_it->second, box)) {
                has_fill = true;
            } else {
                NVGcolor fill_color = cssbox_utils::parse_color(fill_it->second);
                nvgFillColor(vg_, fill_color);
                nvgFill(vg_);
                has_fill = true;
            }
        }
    }

    auto stroke = resolve_stroke_style(element, &box, std::max(box.width, box.height));
    if (stroke.enabled && stroke.width > 0.0f) {
        apply_stroke_state(vg_, stroke);
        nvgStroke(vg_);
    }

    // Render markers
    // Retain mode: Use existing extract_url_id() instead of duplicate lambda

    // marker-start
    auto marker_start_it = element->inline_style.find("marker-start");
    if (marker_start_it != element->inline_style.end() && has_first) {
        std::string marker_id = extract_url_id(marker_start_it->second);
        auto marker_it = renderer_->markers_.find(marker_id);
        if (marker_it != renderer_->markers_.end()) {
            float angle = vertices.empty() ? 0.0f : vertices[0].angle_out;
            render_marker(marker_it->second, first_x, first_y, angle);
        }
    }

    // marker-mid
    auto marker_mid_it = element->inline_style.find("marker-mid");
    if (marker_mid_it != element->inline_style.end()) {
        std::string marker_id = extract_url_id(marker_mid_it->second);
        auto marker_it = renderer_->markers_.find(marker_id);
        if (marker_it != renderer_->markers_.end()) {
            // Render at all intermediate vertices (skip first and last)
            for (size_t i = 0; i < vertices.size(); i++) {
                // Use average of incoming and outgoing angles
                float angle = (vertices[i].angle_in + vertices[i].angle_out) / 2.0f;
                render_marker(marker_it->second, vertices[i].x, vertices[i].y, angle);
            }
        }
    }

    // marker-end
    auto marker_end_it = element->inline_style.find("marker-end");
    if (marker_end_it != element->inline_style.end()) {
        std::string marker_id = extract_url_id(marker_end_it->second);
        auto marker_it = renderer_->markers_.find(marker_id);
        if (marker_it != renderer_->markers_.end()) {
            float angle = vertices.empty() ? 0.0f : vertices.back().angle_in;
            render_marker(marker_it->second, current_x, current_y, angle);
        }
    }
}

void cssboxPainter::paint_freehand_path(const cssboxElement* element) {
    if (element->stroke_points.size() < 2) return;
    auto stroke = resolve_stroke_style(element, nullptr, 1.0f);
    if (!stroke.enabled || stroke.width <= 0.0f) return;

    std::vector<Vec2> raw;
    raw.reserve(element->stroke_points.size());
    for (const auto& point : element->stroke_points) {
        raw.push_back({point.x, point.y});
    }

    float salt = element->has_stroke_salt ? element->stroke_salt : 0.0f;
    auto outline = build_freehand_outline(raw, stroke.width, salt);
    if (outline.empty()) return;

    nvgBeginPath(vg_);
    nvgMoveTo(vg_, outline[0].x, outline[0].y);
    for (size_t i = 1; i < outline.size(); ++i) {
        nvgLineTo(vg_, outline[i].x, outline[i].y);
    }
    nvgClosePath(vg_);
    nvgFillColor(vg_, stroke.color);
    nvgFill(vg_);
}

void cssboxPainter::paint_polygon(const cssboxElement* element, const cssboxBox& box) {
    auto it = element->inline_style.find("points");
    if (it == element->inline_style.end()) return;
    
    std::vector<Vec2> points = parse_svg_points(it->second);
    if (points.size() < 3) return;  // Need at least 3 points for a polygon
    
    // Check if fill is enabled
    bool has_fill = false;
    NVGcolor fill_color = nvgRGBA(0, 0, 0, 255);
    
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second != "none") {
        has_fill = true;
        fill_color = cssbox_utils::parse_color(fill_it->second);
    }
    
    // Resolve stroke style
    float diagonal = std::sqrt(box.width * box.width + box.height * box.height);
    auto stroke = resolve_stroke_style(element, &box, diagonal);
    
    // Fill polygon
    if (has_fill) {
        nvgBeginPath(vg_);
        nvgMoveTo(vg_, points[0].x, points[0].y);
        for (size_t i = 1; i < points.size(); i++) {
            nvgLineTo(vg_, points[i].x, points[i].y);
        }
        nvgClosePath(vg_);
        nvgFillColor(vg_, fill_color);
        nvgFill(vg_);
    }
    
    // Stroke polygon (closed path)
    if (stroke.enabled && stroke.width > 0) {
        stroke_polyline(vg_, stroke, points, true);  // closed=true
    }
}

void cssboxPainter::paint_polyline(const cssboxElement* element, const cssboxBox& box) {
    auto it = element->inline_style.find("points");
    if (it == element->inline_style.end()) return;
    
    std::vector<Vec2> points = parse_svg_points(it->second);
    if (points.size() < 2) return;  // Need at least 2 points for a polyline
    
    // Check if fill is enabled (polylines can have fill, though it's uncommon)
    bool has_fill = false;
    NVGcolor fill_color = nvgRGBA(0, 0, 0, 255);
    
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second != "none") {
        has_fill = true;
        fill_color = cssbox_utils::parse_color(fill_it->second);
    }
    
    // Resolve stroke style
    float diagonal = std::sqrt(box.width * box.width + box.height * box.height);
    auto stroke = resolve_stroke_style(element, &box, diagonal);
    
    // Fill polyline (if specified)
    if (has_fill) {
        nvgBeginPath(vg_);
        nvgMoveTo(vg_, points[0].x, points[0].y);
        for (size_t i = 1; i < points.size(); i++) {
            nvgLineTo(vg_, points[i].x, points[i].y);
        }
        // Note: polyline fill does NOT close the path automatically
        nvgFillColor(vg_, fill_color);
        nvgFill(vg_);
    }
    
    // Stroke polyline (open path)
    if (stroke.enabled && stroke.width > 0) {
        stroke_polyline(vg_, stroke, points, false);  // closed=false
    }
}



// ============================================================================
// Background Image Support (Sprint 31)
// ============================================================================

/**
 * @brief Load background image with caching
 *
 * Loads an image from file and caches it in the renderer's image cache.
 * Subsequent calls with the same path will return the cached image handle.
 *
 * @param path Image file path
 * @param renderer Renderer with image cache
 * @return NanoVG image handle, or -1 if loading failed
 */
static int load_background_image(const std::string& path, cssboxRenderer* renderer) {
    // Check cache first
    auto it = renderer->image_cache.find(path);
    if (it != renderer->image_cache.end()) {
        return it->second;  // Return cached image
    }

    // Load new image
    int image = nvgCreateImage(renderer->vg, path.c_str(), 0);

    if (image > 0) {
        // Cache for future use
        renderer->image_cache[path] = image;
    }

    return image;
}

/**
 * @brief Render background image
 *
 * Renders a background image with support for size, position, and repeat modes.
 * Called from apply_background() when background-image is present.
 *
 * @param element Element to render background for
 * @param box Element's box dimensions
 * @param renderer Renderer (for image cache access)
 */
static void render_background_image(const cssboxElement* element,
                                    const cssboxBox& box,
                                    cssboxRenderer* renderer) {
    // Get background-image from element's inline style (not yet in typed system)
    // TODO: Add background_image to typed ComputedStyle
    auto image_it = element->inline_style.find("background-image");
    if (image_it == element->inline_style.end() || image_it->second.empty() || image_it->second == "none") {
        return;  // No background image
    }

    // Parse image URL
    std::string image_path = cssbox_utils::parse_background_image(image_it->second);
    if (image_path.empty()) {
        return;  // Invalid URL
    }

    // Load image (with caching)
    int image = load_background_image(image_path, renderer);
    if (image < 0) {
        printf("Failed to load background image: %s\n", image_path.c_str());
        return;
    }

    // Get image dimensions
    int img_width, img_height;
    nvgImageSize(renderer->vg, image, &img_width, &img_height);

    // Validate image dimensions to prevent divide-by-zero
    if (img_width <= 0 || img_height <= 0) {
        printf("Invalid image dimensions: %dx%d for %s\n", img_width, img_height, image_path.c_str());
        return;
    }

    // Use typed background-size from element->style.background
    BackgroundSize size_mode;
    float size_width = 0, size_height = 0;

    switch (element->style.background.size_mode) {
        case cssbox::Background::Size::AUTO:
            size_mode = BG_SIZE_AUTO;
            break;
        case cssbox::Background::Size::COVER:
            size_mode = BG_SIZE_COVER;
            break;
        case cssbox::Background::Size::CONTAIN:
            size_mode = BG_SIZE_CONTAIN;
            break;
        case cssbox::Background::Size::EXPLICIT:
            size_mode = BG_SIZE_EXPLICIT;
            size_width = element->style.background.size[0].resolve(box.width, 16, renderer->viewport_width);
            size_height = element->style.background.size[1].resolve(box.height, 16, renderer->viewport_height);
            break;
    }

    // Calculate final image dimensions based on size mode
    float final_width = img_width;
    float final_height = img_height;

    switch (size_mode) {
        case BG_SIZE_AUTO:
            // Use intrinsic image size
            final_width = img_width;
            final_height = img_height;
            break;

        case BG_SIZE_COVER: {
            // Scale to cover entire box, maintaining aspect ratio
            float scale_x = box.width / img_width;
            float scale_y = box.height / img_height;
            float scale = std::max(scale_x, scale_y);
            final_width = img_width * scale;
            final_height = img_height * scale;
            break;
        }

        case BG_SIZE_CONTAIN: {
            // Scale to fit inside box, maintaining aspect ratio
            float scale_x = box.width / img_width;
            float scale_y = box.height / img_height;
            float scale = std::min(scale_x, scale_y);
            final_width = img_width * scale;
            final_height = img_height * scale;
            break;
        }

        case BG_SIZE_EXPLICIT:
            // Use explicit dimensions
            final_width = size_width;
            final_height = size_height;
            break;
    }

    // Use typed background-position from element->style.background
    // Position is relative to (box - image) for percentage values
    float pos_x = element->style.background.position[0].resolve(box.width - final_width, 16, renderer->viewport_width);
    float pos_y = element->style.background.position[1].resolve(box.height - final_height, 16, renderer->viewport_height);

    float offset_x = box.x + pos_x;
    float offset_y = box.y + pos_y;

    // Use typed background-repeat from element->style.background
    BackgroundRepeat repeat;
    switch (element->style.background.repeat) {
        case cssbox::Background::Repeat::NO_REPEAT: repeat = BG_NO_REPEAT; break;
        case cssbox::Background::Repeat::REPEAT:    repeat = BG_REPEAT; break;
        case cssbox::Background::Repeat::REPEAT_X:  repeat = BG_REPEAT_X; break;
        case cssbox::Background::Repeat::REPEAT_Y:  repeat = BG_REPEAT_Y; break;
    }

    // Render based on repeat mode
    NVGcontext* vg = renderer->vg;

    // Save context state
    nvgSave(vg);

    // Create scissor to clip to box bounds
    nvgScissor(vg, box.x, box.y, box.width, box.height);

    switch (repeat) {
        case BG_NO_REPEAT: {
            // Single image at position
            NVGpaint img_paint = nvgImagePattern(vg,
                offset_x, offset_y,
                final_width, final_height,
                0.0f, image, 1.0f);

            nvgBeginPath(vg);
            nvgRect(vg, offset_x, offset_y, final_width, final_height);
            nvgFillPaint(vg, img_paint);
            nvgFill(vg);
            break;
        }

        case BG_REPEAT: {
            // Tile in both directions
            // Calculate start positions (align to pattern)
            float start_x = box.x + fmod(offset_x - box.x, final_width);
            float start_y = box.y + fmod(offset_y - box.y, final_height);

            if (start_x > box.x) start_x -= final_width;
            if (start_y > box.y) start_y -= final_height;

            for (float y = start_y; y < box.y + box.height; y += final_height) {
                for (float x = start_x; x < box.x + box.width; x += final_width) {
                    NVGpaint img_paint = nvgImagePattern(vg,
                        x, y, final_width, final_height,
                        0.0f, image, 1.0f);

                    nvgBeginPath(vg);
                    nvgRect(vg, x, y, final_width, final_height);
                    nvgFillPaint(vg, img_paint);
                    nvgFill(vg);
                }
            }
            break;
        }

        case BG_REPEAT_X: {
            // Tile horizontally only
            float start_x = box.x + fmod(offset_x - box.x, final_width);
            if (start_x > box.x) start_x -= final_width;

            for (float x = start_x; x < box.x + box.width; x += final_width) {
                NVGpaint img_paint = nvgImagePattern(vg,
                    x, offset_y, final_width, final_height,
                    0.0f, image, 1.0f);

                nvgBeginPath(vg);
                nvgRect(vg, x, offset_y, final_width, final_height);
                nvgFillPaint(vg, img_paint);
                nvgFill(vg);
            }
            break;
        }

        case BG_REPEAT_Y: {
            // Tile vertically only
            float start_y = box.y + fmod(offset_y - box.y, final_height);
            if (start_y > box.y) start_y -= final_height;

            for (float y = start_y; y < box.y + box.height; y += final_height) {
                NVGpaint img_paint = nvgImagePattern(vg,
                    offset_x, y, final_width, final_height,
                    0.0f, image, 1.0f);

                nvgBeginPath(vg);
                nvgRect(vg, offset_x, y, final_width, final_height);
                nvgFillPaint(vg, img_paint);
                nvgFill(vg);
            }
            break;
        }
    }

    // Restore context state (remove scissor)
    nvgRestore(vg);
}

// ============================================================================
// SVG Clipping Implementation
// ============================================================================

void cssboxPainter::begin_clip_path(const cssboxClipPath& clip_path) {
    nvgSave(vg_);

    // Iterate through clip path shapes
    // Currently only supports rectangular clipping via nvgIntersectScissor
    for (int child_id : clip_path.children_internal_ids) {
        auto it = renderer_->elements.find(child_id);
        if (it == renderer_->elements.end()) continue;
        const auto* child = it->second.get();

        if (child->type == "rect") {
            // Parse rect attributes - support both SVG (x/y) and CSS (left/top) syntax
            float x = 0, y = 0, w = 0, h = 0;

            auto x_it = child->inline_style.find("x");
            auto left_it = child->inline_style.find("left");
            if (x_it != child->inline_style.end()) {
                x = cssbox_utils::parse_length(x_it->second, 0);
            } else if (left_it != child->inline_style.end()) {
                x = cssbox_utils::parse_length(left_it->second, 0);
            }

            auto y_it = child->inline_style.find("y");
            auto top_it = child->inline_style.find("top");
            if (y_it != child->inline_style.end()) {
                y = cssbox_utils::parse_length(y_it->second, 0);
            } else if (top_it != child->inline_style.end()) {
                y = cssbox_utils::parse_length(top_it->second, 0);
            }

            auto w_it = child->inline_style.find("width");
            if (w_it != child->inline_style.end()) {
                w = cssbox_utils::parse_length(w_it->second, 0);
            }

            auto h_it = child->inline_style.find("height");
            if (h_it != child->inline_style.end()) {
                h = cssbox_utils::parse_length(h_it->second, 0);
            }

            nvgIntersectScissor(vg_, x, y, w, h);
        }
    }
}

void cssboxPainter::end_clip_path() {
    nvgRestore(vg_);
}

// ============================================================================
// SVG Pattern Rendering (FBO-based)
// ============================================================================

int cssboxPainter::render_pattern_to_fbo(cssboxPattern& pattern) {
    // Skip if already rendered and not dirty
    if (!pattern.needs_update && pattern.image_handle >= 0) {
        return pattern.image_handle;
    }
    
    // Cleanup old resources
    cleanup_pattern_fbo(pattern);
    
    int w = (int)pattern.width;
    int h = (int)pattern.height;
    
    if (w <= 0 || h <= 0) return -1;
    
    // Create FBO and texture
    glGenFramebuffers(1, &pattern.fbo);
    glGenTextures(1, &pattern.texture);
    
    // Setup texture
    glBindTexture(GL_TEXTURE_2D, pattern.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Attach texture to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, pattern.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pattern.texture, 0);
    
    // Check FBO status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        cleanup_pattern_fbo(pattern);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return -1;
    }
    
    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Set viewport to pattern size
    glViewport(0, 0, w, h);
    
    // Clear pattern area
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    // Render pattern content using NanoVG
    nvgBeginFrame(vg_, w, h, 1.0f);
    
    // Render each child element
    for (int child_id : pattern.children_internal_ids) {
        auto it = renderer_->elements.find(child_id);
        if (it != renderer_->elements.end()) {
            paint_element(it->second.get());
        }
    }
    
    nvgEndFrame(vg_);
    
    // Read pixels from FBO
    std::vector<unsigned char> pixels(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    
    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    
    // Unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Create NanoVG image from pixels
    pattern.image_handle = nvgCreateImageRGBA(vg_, w, h, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY, pixels.data());
    pattern.needs_update = false;
    
    // Cleanup FBO and texture (no longer needed after pixel readback)
    glDeleteTextures(1, &pattern.texture);
    glDeleteFramebuffers(1, &pattern.fbo);
    pattern.texture = 0;
    pattern.fbo = 0;
    
    return pattern.image_handle;
}

void cssboxPainter::cleanup_pattern_fbo(cssboxPattern& pattern) {
    if (pattern.image_handle >= 0) {
        nvgDeleteImage(vg_, pattern.image_handle);
        pattern.image_handle = -1;
    }
    
    if (pattern.texture > 0) {
        glDeleteTextures(1, &pattern.texture);
        pattern.texture = 0;
    }
    
    if (pattern.fbo > 0) {
        glDeleteFramebuffers(1, &pattern.fbo);
        pattern.fbo = 0;
    }
}

void cssboxPainter::prepare_patterns() {
    // Pre-render all dirty patterns before the main NVG frame
    // This is called before nvgBeginFrame() so we can use our own frame
    for (auto& [id, pattern] : renderer_->patterns_) {
        if (pattern.needs_update && !pattern.children_internal_ids.empty()) {
            // Force style computation for pattern children
            for (int child_id : pattern.children_internal_ids) {
                auto it = renderer_->elements.find(child_id);
                if (it != renderer_->elements.end()) {
                    cssboxElement* child = it->second.get();
                    child->style = renderer_->stylesheet->compute_style_typed(
                        child->id,
                        child->type,
                        child->classes,
                        child->attributes,
                        child->pseudo_states,
                        child->inline_style,
                        nullptr,
                        child->child_index,
                        child->total_siblings
                    );

                    // Set up layout for pattern children based on pattern dimensions
                    // Pattern children use the pattern's coordinate system
                    child->layout.x = 0;
                    child->layout.y = 0;
                    child->layout.width = pattern.width;
                    child->layout.height = pattern.height;
                    child->layout.source = cssbox::ResolvedLayout::Source::FLEXBOX;  // Mark as computed
                }
            }

            // Render pattern to FBO
            render_pattern_to_fbo(pattern);
        }
    }
}

bool cssboxPainter::apply_pattern_fill(const std::string& fill_value, const cssboxBox& box) {
    // Check if fill is a pattern reference: url(#pattern-id)
    if (fill_value.find("url(#") == std::string::npos) {
        return false;  // Not a pattern
    }

    // Extract pattern ID
    std::string pattern_id = extract_url_id(fill_value);

    // Find pattern in registry
    auto pattern_it = renderer_->patterns_.find(pattern_id);
    if (pattern_it == renderer_->patterns_.end()) {
        return false;  // Pattern not found
    }

    // Pattern should already be rendered by prepare_patterns()
    // Just use the cached image handle
    int img = pattern_it->second.image_handle;
    if (img < 0) {
        // Fallback: try to render now (shouldn't happen if prepare_patterns was called)
        img = render_pattern_to_fbo(pattern_it->second);
        if (img < 0) {
            return false;  // Failed to render pattern
        }
    }

    // Create image pattern paint
    NVGpaint pattern_paint = nvgImagePattern(
        vg_,
        box.x + pattern_it->second.x,
        box.y + pattern_it->second.y,
        pattern_it->second.width,
        pattern_it->second.height,
        0.0f,  // angle (TODO: support patternTransform)
        img,
        1.0f   // alpha
    );

    // Apply pattern as fill
    nvgFillPaint(vg_, pattern_paint);
    nvgFill(vg_);

    return true;  // Pattern applied successfully
}

// ============================================================================
// SVG Marker Rendering
// ============================================================================

void cssboxPainter::render_marker(const cssboxMarker& marker, float x, float y, float angle) {
    nvgSave(vg_);
    
    // Translate to marker position
    nvgTranslate(vg_, x, y);
    
    // Apply orientation
    if (marker.orient == "auto") {
        nvgRotate(vg_, angle);
    } else if (marker.orient == "auto-start-reverse") {
        nvgRotate(vg_, angle + NVG_PI);  // Reverse direction
    } else if (marker.orient != "0") {
        // Parse angle in degrees and convert to radians
        float orient_angle = std::stof(marker.orient) * NVG_PI / 180.0f;
        nvgRotate(vg_, orient_angle);
    }
    
    // Offset by reference point
    nvgTranslate(vg_, -marker.refX, -marker.refY);
    
    // Scale to marker size (assuming default viewport is 10x10)
    float scaleX = marker.markerWidth / 10.0f;
    float scaleY = marker.markerHeight / 10.0f;
    nvgScale(vg_, scaleX, scaleY);
    
    // Render marker content (children)
    for (int child_id : marker.children_internal_ids) {
        auto it = renderer_->elements.find(child_id);
        if (it != renderer_->elements.end()) {
            paint_element(it->second.get());
        }
    }
    
    nvgRestore(vg_);
}

// ============================================================================
// Text-on-Path Implementation
// ============================================================================

#include "cssbox_painter_textpath.cpp"
