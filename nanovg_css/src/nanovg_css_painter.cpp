/*
 * NanoVG CSS - Painter implementation
 *
 * Translates CSS properties to NanoVG drawing calls.
 */

#include "nanovg_css_internal.h"
#include "nanovg_css_filters.h"
#include <nanovg_css_filters.h>
#include <nanovg_rough.h>
#include <fmtlog.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
 // PHASE 4 SPRINT 4: Helper to create NVGCSSBox from computed layout
  static NVGCSSBox computed_to_box(const NVGCSSComputedLayout& computed) {
      NVGCSSBox box;
      box.x = computed.x;
      box.y = computed.y;
      box.width = computed.width;
      box.height = computed.height;
      for (int i = 0; i < 4; i++) {
          box.padding[i] = computed.padding[i];
          box.margin[i] = computed.margin[i];
          box.border_width[i] = computed.border[i];
          box.border_radius[i] = computed.border_radius[i];
      }
      return box;
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

using StrokeStyle = NVGCSSPainter::StrokeStyle;
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

}  // namespace

// Forward declaration for Sprint 31 background image rendering
static void render_background_image(const NVGCSSElement* element,
                                    const NVGCSSBox& box,
                                    NVGCSSRenderer* renderer);

NVGCSSPainter::NVGCSSPainter(NVGcontext* vg, NVGCSSRenderer* renderer) 
    : vg_(vg), renderer_(renderer), filter_context_(nullptr) {
    // Initialize filter context only if we have a valid NanoVG context
    if (vg_) {
        filter_context_ = nvgcssCreateFilterContext();
    }
}

NVGCSSPainter::~NVGCSSPainter() {
    if (filter_context_) {
        nvgcssDeleteFilterContext(filter_context_);
    }
}

void NVGCSSPainter::apply_filters(const NVGCSSElement* element, float& opacity) {
    if (element->style.filters.empty()) return;
    
    // Separate opacity from other filters
    bool has_advanced_filters = false;
    for (const auto& filter : element->style.filters) {
        if (filter.type == nvgcss::FilterType::OPACITY) {
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

void NVGCSSPainter::render_with_filters(const NVGCSSElement* element, 
                                        const NVGCSSBox& box,
                                        std::function<void()> render_fn) {
    if (element->style.filters.empty() || !filter_context_ || !vg_) {
        render_fn();
        return;
    }
    
    // Check if we have any advanced filters (non-opacity)
    bool has_advanced = false;
    for (const auto& filter : element->style.filters) {
        if (filter.type != nvgcss::FilterType::OPACITY) {
            has_advanced = true;
            break;
        }
    }
    
    if (!has_advanced) {
        render_fn();
        return;
    }
    
    // Convert filters to C API format
    std::vector<NVGCSSFilter> c_filters;
    for (const auto& filter : element->style.filters) {
        NVGCSSFilterType type;
        switch (filter.type) {
            case nvgcss::FilterType::BLUR:
                type = NVGCSS_FILTER_BLUR;
                break;
            case nvgcss::FilterType::BRIGHTNESS:
                type = NVGCSS_FILTER_BRIGHTNESS;
                break;
            case nvgcss::FilterType::CONTRAST:
                type = NVGCSS_FILTER_CONTRAST;
                break;
            case nvgcss::FilterType::GRAYSCALE:
                type = NVGCSS_FILTER_GRAYSCALE;
                break;
            case nvgcss::FilterType::HUE_ROTATE:
                type = NVGCSS_FILTER_HUE_ROTATE;
                break;
            case nvgcss::FilterType::INVERT:
                type = NVGCSS_FILTER_INVERT;
                break;
            case nvgcss::FilterType::SATURATE:
                type = NVGCSS_FILTER_SATURATE;
                break;
            case nvgcss::FilterType::SEPIA:
                type = NVGCSS_FILTER_SEPIA;
                break;
            case nvgcss::FilterType::OPACITY:
                type = NVGCSS_FILTER_OPACITY;
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
    
    nvgcssApplyFilters(filter_context_, vg_, box.x, box.y, box.width, box.height,
                       c_filters.data(), (int)c_filters.size(), callback, &render_fn);
}

// Sprint 33: Helper function to render a single border side with style
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

// Sprint 34: Helper to parse per-side border color with fallback
static NVGcolor parse_border_color(const std::string& explicit_color,
                                    const NVGCSSElement* element)
{
    // Use explicit per-side color if set
    if (!explicit_color.empty()) {
        return nvgcss_utils::parse_color(explicit_color);
    }

    // TODO: border-color not yet in typed system - reading from inline_style
    auto it = element->inline_style.find("border-color");
    if (it != element->inline_style.end()) {
        return nvgcss_utils::parse_color(it->second);
    }

    // Default: black
    return nvgRGBA(0, 0, 0, 255);
}

// Sprint 33: Render borders with per-side styles
// Sprint 34: Enhanced with per-side colors
static void render_styled_borders(NVGcontext* vg, const NVGCSSElement* element,
                                  const NVGCSSBox& box)
{
    // Get border properties from element
    float top_width = box.border_width[0];
    float right_width = box.border_width[1];
    float bottom_width = box.border_width[2];
    float left_width = box.border_width[3];

    // Sprint 34: Parse per-side colors (fallback to general border-color from typed style)
    NVGcolor top_color = parse_border_color(element->explicit_style.border_top_color, element);
    NVGcolor right_color = parse_border_color(element->explicit_style.border_right_color, element);
    NVGcolor bottom_color = parse_border_color(element->explicit_style.border_bottom_color, element);
    NVGcolor left_color = parse_border_color(element->explicit_style.border_left_color, element);

    // Get per-side styles
    const std::string& top_style = element->explicit_style.border_top_style;
    const std::string& right_style = element->explicit_style.border_right_style;
    const std::string& bottom_style = element->explicit_style.border_bottom_style;
    const std::string& left_style = element->explicit_style.border_left_style;

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
void NVGCSSPainter::paint_element(const NVGCSSElement* element) {
    // DEBUG: Log what we're trying to paint
    logi("[PAINTER] paint_element id='{}' type='{}' display={} visible={} bg_type={}",
         element->id, element->type, (int)element->style.display, element->visible, 
         (int)element->style.background.type);
    
    // Check visibility from typed property
    if (element->style.display == nvgcss::Display::NONE) {
        logi("[PAINTER] Skipping '{}' - display is NONE", element->id);
        return;
    }
    if (!element->visible) {
        logi("[PAINTER] Skipping '{}' - not visible", element->id);
        return;
    }

    // Save state
    nvgSave(vg_);

    // Apply transform
    nvgTransform(vg_,
                 element->transform[0], element->transform[1],
                 element->transform[2], element->transform[3],
                 element->transform[4], element->transform[5]);

    // Apply opacity from typed property
    float combined_opacity = element->style.opacity;
    
    // Apply filter effects
    apply_filters(element, combined_opacity);
    
    nvgGlobalAlpha(vg_, combined_opacity);

    // Draw based on element type
    // HTML elements (div, button, span with text) are treated as boxes
    bool is_box_element = (element->type == "rect" || element->type == "group" ||
                          element->type == "div" || element->type == "button" ||
                          element->type == "input" || element->type == "panel" ||
                          element->type == "screen" || element->type == "window" ||
                          element->type == "widget");

    if (!element->stroke_points.empty()) {
        paint_freehand_path(element);
    }
    else if (is_box_element) {
        // Use computed layout for dimensions (PHASE 4 SPRINT 4)
        const auto& box = computed_to_box(element->computed);

        // DEBUG: Trace element rendering
        logi("[PAINTER] Rendering type='{}' id='{}' class='{}' at ({:.1f}, {:.1f}) size ({:.1f} x {:.1f})",
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

        // Create path (rounded rectangle if border-radius specified)
        create_rounded_rect_path(box);

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
        if (!element->text_content.empty()) {
            // Get text color from typed property
            NVGcolor text_color = element->style.color;

            // Font size from typed property (already in pixels)
            float font_size = element->style.font_size;

            // Font face (with font-weight and font-style)
            std::string font_face = compute_font_face(element);

            // Text alignment from typed enum
            int align = compute_text_align(element);

            // TODO: text-transform not yet in typed system
            std::string text_content = element->text_content;

            // Apply typography settings to NanoVG
            nvgFontSize(vg_, font_size);
            nvgFontFace(vg_, font_face.c_str());
            nvgTextAlign(vg_, align);
            nvgFillColor(vg_, text_color);

            // Calculate text position based on alignment
            // IMPORTANT: Use content box (account for padding), not border box!
            float padding_left = element->computed.padding[3];   // left
            float padding_top = element->computed.padding[0];    // top
            float padding_right = element->computed.padding[1];  // right
            float padding_bottom = element->computed.padding[2]; // bottom

            float content_x = box.x + padding_left;
            float content_y = box.y + padding_top;
            float content_width = box.width - padding_left - padding_right;
            float content_height = box.height - padding_top - padding_bottom;

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

            // Sprint 28: Render text shadows FIRST (so they appear behind text)
            if (!element->text_shadows.empty()) {
                // Render each shadow in reverse order (last shadow first, so first shadow is on top)
                for (auto it = element->text_shadows.rbegin(); it != element->text_shadows.rend(); ++it) {
                    const TextShadow& shadow = *it;

                    // Calculate shadow position
                    float shadow_x = text_x + shadow.offset_x;
                    float shadow_y = text_y + shadow.offset_y;

                    // If blur is specified, render multiple times with decreasing alpha
                    if (shadow.blur_radius > 0) {
                        // Simple blur approximation: render text multiple times with decreasing alpha
                        int blur_samples = std::min(5, (int)shadow.blur_radius);
                        for (int i = 0; i < blur_samples; i++) {
                            float offset = (i - blur_samples / 2.0f) * (shadow.blur_radius / blur_samples);
                            float alpha_multiplier = 1.0f - (std::abs(i - blur_samples / 2.0f) / (blur_samples / 2.0f));

                            NVGcolor blurred_color = shadow.color;
                            blurred_color.a *= alpha_multiplier * 0.3f;  // Reduce alpha for blur

                            nvgFillColor(vg_, blurred_color);
                            nvgText(vg_, shadow_x + offset, shadow_y + offset, text_content.c_str(), nullptr);
                        }
                    } else {
                        // No blur - just render once
                        nvgFillColor(vg_, shadow.color);
                        nvgText(vg_, shadow_x, shadow_y, text_content.c_str(), nullptr);
                    }
                }
            }

            // Render actual text on top
            nvgFillColor(vg_, text_color);
            nvgText(vg_, text_x, text_y, text_content.c_str(), nullptr);

            // Apply text decoration (underline, line-through, overline)
            apply_text_decoration(element, text_x, text_y, text_color);
        }
    }
    else if (element->type == "text" || element->type == "span") {
        // Phase 3: Text rendering with Sprint 11 typography support
        const auto& box = computed_to_box(element->computed);

        // Get text color from typed property
        NVGcolor text_color = element->style.color;

        // Font size from typed property (already in pixels)
        float font_size = element->style.font_size;

        // Font face (with font-weight and font-style)
        std::string font_face = compute_font_face(element);

        // Text alignment from typed enum
        int align = compute_text_align(element);

        // TODO: text-transform not yet in typed system
        std::string text_content = element->text_content;

        // Apply typography settings to NanoVG
        nvgFontSize(vg_, font_size);
        nvgFontFace(vg_, font_face.c_str());
        nvgTextAlign(vg_, align);
        nvgFillColor(vg_, text_color);

        // Calculate text position based on alignment
        float text_x = box.x;
        float text_y = box.y;

        if (align & NVG_ALIGN_CENTER) {
            text_x += box.width / 2.0f;
        } else if (align & NVG_ALIGN_RIGHT) {
            text_x += box.width;
        }

        if (align & NVG_ALIGN_MIDDLE) {
            text_y += box.height / 2.0f;
        } else if (align & NVG_ALIGN_BOTTOM) {
            text_y += box.height;
        }

        // Sprint 28: Render text shadows FIRST (so they appear behind text)
        if (!element->text_shadows.empty()) {
            // Render each shadow in reverse order (last shadow first, so first shadow is on top)
            for (auto it = element->text_shadows.rbegin(); it != element->text_shadows.rend(); ++it) {
                const TextShadow& shadow = *it;

                // Calculate shadow position
                float shadow_x = text_x + shadow.offset_x;
                float shadow_y = text_y + shadow.offset_y;

                // If blur is specified, render multiple times with decreasing alpha
                if (shadow.blur_radius > 0) {
                    // Simple blur approximation: render text multiple times with decreasing alpha
                    int blur_samples = std::min(5, (int)shadow.blur_radius);
                    for (int i = 0; i < blur_samples; i++) {
                        float offset = (i - blur_samples / 2.0f) * (shadow.blur_radius / blur_samples);
                        float alpha_multiplier = 1.0f - (std::abs(i - blur_samples / 2.0f) / (blur_samples / 2.0f));

                        NVGcolor blurred_color = shadow.color;
                        blurred_color.a *= alpha_multiplier * 0.3f;  // Reduce alpha for blur

                        nvgFillColor(vg_, blurred_color);
                        nvgText(vg_, shadow_x + offset, shadow_y + offset, text_content.c_str(), nullptr);
                    }
                } else {
                    // No blur - just render once
                    nvgFillColor(vg_, shadow.color);
                    nvgText(vg_, shadow_x, shadow_y, text_content.c_str(), nullptr);
                }
            }
        }

        // Render actual text on top
        nvgFillColor(vg_, text_color);
        nvgText(vg_, text_x, text_y, text_content.c_str(), nullptr);

        // Apply text decoration (underline, line-through, overline)
        apply_text_decoration(element, text_x, text_y, text_color);
    }
    else if (element->type == "line") {
        const auto& box = computed_to_box(element->computed);
        paint_line_shape(element, box);
    }
    else if (element->type == "circle" || element->type == "ellipse") {
        const auto& box = computed_to_box(element->computed);
        paint_circle_shape(element, box);
    }
    else if (element->type == "path") {
        const auto& box = computed_to_box(element->computed);
        paint_svg_path(element, box);
    }
    else if (element->type == "polygon" || element->type == "polyline") {
        const auto& box = computed_to_box(element->computed);
        paint_svg_path(element, box);
    }

    // Call custom paint callback if provided
    if (element->custom_paint) {
        // TODO: custom_paint signature should be updated to not need computed_style
        element->custom_paint(vg_, element, element->inline_style);
    }

    // Restore state
    nvgRestore(vg_);
}

void NVGCSSPainter::apply_background(const NVGCSSElement* element,
                                      const NVGCSSBox& box) {
    // 60fps: Read from TYPED property (not inline_style!)
    const auto& bg = element->style.background;

    logi("[PAINTER] apply_background id='{}' bg_type={} gradient_css='{}'",
         element->id, (int)bg.type, bg.gradient_css);

    // Handle different background types
    if (bg.type == nvgcss::BackgroundType::COLOR) {
        NVGcolor bg_color = bg.color;

        // Check if background color is transparent (alpha == 0)
        if (bg_color.a <= 0.001f) {
            return;  // Skip transparent backgrounds
        }

        nvgFillColor(vg_, bg_color);
        nvgFill(vg_);
    }
    else if (bg.type == nvgcss::BackgroundType::GRADIENT) {
        // Parse and render gradient from CSS string
        if (!bg.gradient_css.empty()) {
            logi("[PAINTER] Creating gradient for box ({}, {}) size {}x{}", 
                 box.x, box.y, box.width, box.height);
            NVGpaint gradient_paint = create_gradient(bg.gradient_css, box);
            logi("[PAINTER] Gradient paint created, applying...");
            nvgFillPaint(vg_, gradient_paint);
            nvgFill(vg_);
            logi("[PAINTER] Gradient filled");
        } else {
            logi("[PAINTER] ERROR: gradient_css is EMPTY!");
        }
    }
    else if (bg.type == nvgcss::BackgroundType::IMAGE) {
        // Background image rendering implemented via render_background_image()
        // bg.image_handle contains the NanoVG image handle
    }
    // If type == NONE, skip (transparent background)
}

void NVGCSSPainter::apply_border(const std::map<std::string, std::string>& style,
                                  const NVGCSSBox& box) {
    // Check for border properties
    auto color_it = style.find("border-color");
    auto width_it = style.find("border-width");
    auto style_it = style.find("border-style");

    // Also check shorthand
    if (color_it == style.end()) color_it = style.find("border");
    if (width_it == style.end() && style.count("border")) {
        // Parse width from shorthand (e.g., "2px solid red")
        // For simplicity, assume format: width style color
    }

    // Default values
    float border_width = 1.0f;
    NVGcolor border_color = nvgRGBA(0, 0, 0, 255);
    std::string border_style = "solid";

    if (width_it != style.end()) {
        border_width = nvgcss_utils::parse_length(width_it->second, box.width);
    }

    if (color_it != style.end()) {
        border_color = nvgcss_utils::parse_color(color_it->second);
    }

    if (style_it != style.end()) {
        border_style = style_it->second;
    }

    // Only draw if border width > 0
    if (border_width > 0 && border_style != "none") {
        // Recreate path for stroke
        create_rounded_rect_path(box);

        nvgStrokeWidth(vg_, border_width);
        nvgStrokeColor(vg_, border_color);

        // Handle dashed/dotted styles
        if (border_style == "dashed") {
            // NanoVG doesn't have native dashed lines, but we can approximate
            // For now, just use solid
        } else if (border_style == "dotted") {
            // Same - use solid for now
        }

        nvgStroke(vg_);
    }
}

// Sprint 27: Render box-shadows using parsed BoxShadow data structure
// Sprint 39: Added inset shadow support
void NVGCSSPainter::paint_box_shadows(const NVGCSSElement* element,
                                       const NVGCSSBox& box) {
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

// DEPRECATED: Old apply_shadow that parsed on every render
void NVGCSSPainter::apply_shadow(const std::map<std::string, std::string>& style,
                                  const NVGCSSBox& box) {
    // Sprint 27: This function is deprecated - use paint_box_shadows() instead
    // Kept for backward compatibility only
    // The new implementation uses element->box_shadows which is parsed during layout
}

void NVGCSSPainter::apply_opacity(const std::map<std::string, std::string>& style) {
    // Already handled in paint_element
}

NVGpaint NVGCSSPainter::create_gradient(const std::string& gradient_css,
                                        const NVGCSSBox& box) {
    // Phase 3: Parse gradient and dispatch to appropriate handler
    GradientData gradient = parse_gradient(gradient_css);

    if (gradient.type == GradientData::LINEAR) {
        return create_linear_gradient(gradient, box);
    } else {
        return create_radial_gradient(gradient, box);
    }
}

GradientData NVGCSSPainter::parse_gradient(const std::string& gradient_css) {
    GradientData result;

    // Determine gradient type
    if (gradient_css.find("radial-gradient") != std::string::npos) {
        result.type = GradientData::RADIAL;
    } else {
        result.type = GradientData::LINEAR;
    }

    std::string gradient = gradient_css;

    // Remove function name and parentheses
    size_t start_pos = gradient.find('(');
    size_t end_pos = gradient.rfind(')');
    if (start_pos != std::string::npos && end_pos != std::string::npos) {
        gradient = gradient.substr(start_pos + 1, end_pos - start_pos - 1);
    }

    // Split by commas (but be careful with rgba commas)
    std::vector<std::string> parts;
    std::string current;
    int paren_depth = 0;

    for (char c : gradient) {
        if (c == '(') paren_depth++;
        else if (c == ')') paren_depth--;
        else if (c == ',' && paren_depth == 0) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.empty()) parts.push_back(current);

    // Trim all parts
    for (auto& part : parts) {
        part.erase(0, part.find_first_not_of(" \t"));
        part.erase(part.find_last_not_of(" \t") + 1);
    }

    if (parts.empty()) return result;

    size_t color_start_idx = 0;

    // Parse direction/position (first part might not be a color)
    if (!parts.empty()) {
        const std::string& first = parts[0];

        if (result.type == GradientData::LINEAR) {
            // Linear gradient direction
            if (first.find("to ") == 0) {
                // Named direction
                if (first == "to bottom") result.angle = 180.0f;
                else if (first == "to top") result.angle = 0.0f;
                else if (first == "to right") result.angle = 90.0f;
                else if (first == "to left") result.angle = 270.0f;
                else if (first == "to bottom right") result.angle = 135.0f;
                else if (first == "to bottom left") result.angle = 225.0f;
                else if (first == "to top right") result.angle = 45.0f;
                else if (first == "to top left") result.angle = 315.0f;
                color_start_idx = 1;
            } else if (first.find("deg") != std::string::npos) {
                // Angle in degrees
                result.angle = safe_stof(first);
                color_start_idx = 1;
            }
        } else {
            // Radial gradient shape/position
            if (first.find("circle") != std::string::npos) {
                result.shape = "circle";
                // Check for position
                if (first.find(" at ") != std::string::npos) {
                    size_t at_pos = first.find(" at ");
                    result.position = first.substr(at_pos + 4);
                }
                color_start_idx = 1;
            } else if (first.find("ellipse") != std::string::npos) {
                result.shape = "ellipse";
                if (first.find(" at ") != std::string::npos) {
                    size_t at_pos = first.find(" at ");
                    result.position = first.substr(at_pos + 4);
                }
                color_start_idx = 1;
            } else if (first.find("at ") != std::string::npos) {
                result.position = first.substr(3);  // Remove "at "
                color_start_idx = 1;
            }
        }
    }

    // Parse color stops
    for (size_t i = color_start_idx; i < parts.size(); ++i) {
        std::string stop_str = parts[i];

        GradientStop stop;
        stop.position = static_cast<float>(i - color_start_idx) /
                       static_cast<float>(parts.size() - color_start_idx - 1);

        // Check if stop has explicit position (e.g., "red 50%")
        size_t space_pos = stop_str.find_last_of(' ');
        if (space_pos != std::string::npos) {
            std::string pos_str = stop_str.substr(space_pos + 1);
            if (pos_str.find('%') != std::string::npos) {
                // Percentage position
                stop.position = safe_stof(pos_str) / 100.0f;
                stop_str = stop_str.substr(0, space_pos);
                stop_str.erase(stop_str.find_last_not_of(" \t") + 1);
            } else if (pos_str.find("px") != std::string::npos) {
                // Pixel position - convert to percentage (needs box size, skip for now)
                stop_str = stop_str.substr(0, space_pos);
                stop_str.erase(stop_str.find_last_not_of(" \t") + 1);
            }
        }

        stop.color = nvgcss_utils::parse_color(stop_str);
        result.stops.push_back(stop);
    }

    // Ensure we have at least 2 stops
    if (result.stops.size() < 2) {
        if (result.stops.empty()) {
            result.stops.push_back({0.0f, nvgRGB(0, 0, 0)});
        }
        result.stops.push_back({1.0f, nvgRGB(255, 255, 255)});
    }

    return result;
}

NVGpaint NVGCSSPainter::create_linear_gradient(const GradientData& gradient,
                                                const NVGCSSBox& box) {
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

NVGpaint NVGCSSPainter::create_radial_gradient(const GradientData& gradient,
                                                const NVGCSSBox& box) {
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

void NVGCSSPainter::create_rounded_rect_path(const NVGCSSBox& box) {
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

std::string NVGCSSPainter::compute_font_face(const NVGCSSElement* element) {
    // Get base font family from typed property
    std::string family = element->style.font_family;

    // font_weight is typed enum - check if bold
    bool is_bold = (element->style.font_weight >= nvgcss::FontWeight::BOLD);  // 700+

    // font_style is typed enum
    bool is_italic = (element->style.font_style == nvgcss::FontStyle::ITALIC ||
                     element->style.font_style == nvgcss::FontStyle::OBLIQUE);

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

float NVGCSSPainter::parse_font_size(const std::map<std::string, std::string>& style,
                                      float parent_size) {
    auto it = style.find("font-size");
    if (it == style.end()) return parent_size;

    const std::string& value = it->second;

    // Keywords
    static const std::map<std::string, float> keywords = {
        {"xx-small", 9.0f},
        {"x-small", 10.0f},
        {"small", 13.0f},
        {"medium", 16.0f},  // Default
        {"large", 18.0f},
        {"x-large", 24.0f},
        {"xx-large", 32.0f}
    };

    if (keywords.count(value)) {
        return keywords.at(value);
    }

    // Parse length with units
    if (value.find("em") != std::string::npos) {
        float multiplier = safe_stof(value, 1.f);
        return parent_size * multiplier;
    }

    if (value.find("rem") != std::string::npos) {
        float multiplier = safe_stof(value, 1.f);
        // Root font size is typically 16px
        return 16.0f * multiplier;
    }

    if (value.find("%") != std::string::npos) {
        float percent = safe_stof(value, 100.f);
        return parent_size * (percent / 100.0f);
    }

    // Default: px
    return nvgcss_utils::parse_length(value, parent_size);
}

int NVGCSSPainter::compute_text_align(const NVGCSSElement* element) {
    int h_align = NVG_ALIGN_LEFT;
    int v_align = NVG_ALIGN_TOP;

    // Horizontal alignment from typed enum
    switch (element->style.text_align) {
        case nvgcss::TextAlign::LEFT:   h_align = NVG_ALIGN_LEFT; break;
        case nvgcss::TextAlign::CENTER: h_align = NVG_ALIGN_CENTER; break;
        case nvgcss::TextAlign::RIGHT:  h_align = NVG_ALIGN_RIGHT; break;
        case nvgcss::TextAlign::JUSTIFY: h_align = NVG_ALIGN_LEFT; break;  // NanoVG doesn't support justify
    }

    // TODO: vertical-align not yet in typed system - reading from inline_style
    auto valign_it = element->inline_style.find("vertical-align");
    if (valign_it != element->inline_style.end()) {
        const std::string& valign = valign_it->second;
        if (valign == "top") v_align = NVG_ALIGN_TOP;
        else if (valign == "middle") v_align = NVG_ALIGN_MIDDLE;
        else if (valign == "bottom") v_align = NVG_ALIGN_BOTTOM;
        else if (valign == "baseline") v_align = NVG_ALIGN_BASELINE;
    }

    return h_align | v_align;
}

std::string NVGCSSPainter::apply_text_transform(const std::string& text,
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

void NVGCSSPainter::apply_text_decoration(const NVGCSSElement* element,
                                           float text_x, float text_y,
                                           NVGcolor text_color) {
    // TODO: text-decoration not yet in typed system - reading from inline_style
    auto decoration_it = element->inline_style.find("text-decoration");
    if (decoration_it == element->inline_style.end()) {
        return;  // No text decoration
    }

    const std::string& decoration = decoration_it->second;
    if (decoration == "none") return;

    // Get text bounds
    float bounds[4];
    nvgTextBounds(vg_, text_x, text_y, element->text_content.c_str(), nullptr, bounds);

    float line_y;
    if (decoration == "underline") {
        line_y = bounds[3] + 2;  // Below text
    } else if (decoration == "line-through") {
        line_y = (bounds[1] + bounds[3]) / 2.0f;  // Middle
    } else if (decoration == "overline") {
        line_y = bounds[1] - 2;  // Above text
    } else {
        return;  // Unrecognized decoration
    }

    // Draw decoration line
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, bounds[0], line_y);
    nvgLineTo(vg_, bounds[2], line_y);
    nvgStrokeColor(vg_, text_color);
    nvgStrokeWidth(vg_, 1.0f);
    nvgStroke(vg_);
}

NVGCSSPainter::StrokeStyle NVGCSSPainter::resolve_stroke_style(
    const NVGCSSElement* element,
    const NVGCSSBox* box,
    float absolute_hint) const {

    StrokeStyle stroke;
    float reference = absolute_hint;
    if (box) {
        reference = std::max(box->width, box->height);
    }

    // TODO: stroke properties not yet in typed system
    // Reading from inline_style temporarily until we add them to ComputedStyle

    bool stroke_forced_none = false;
    auto stroke_it = element->inline_style.find("stroke");
    if (stroke_it != element->inline_style.end()) {
        const std::string& value = stroke_it->second;
        logi("[PAINTER] resolve_stroke_style id='{}' stroke='{}'", element->id, value);
        if (value != "none" && value != "transparent") {
            stroke.color = nvgcss_utils::parse_color(value);
            stroke.enabled = true;
            logi("[PAINTER] Stroke enabled: r={} g={} b={} a={}", stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a);
        } else {
            stroke_forced_none = true;
            stroke.enabled = false;
        }
    } else {
        logi("[PAINTER] resolve_stroke_style id='{}' - NO stroke property found", element->id);
    }

    auto width_it = element->inline_style.find("stroke-width");
    if (width_it != element->inline_style.end()) {
        float parsed_width = nvgcss_utils::parse_length(width_it->second, reference);
        if (parsed_width > 0.0f) {
            stroke.width = parsed_width;
            stroke.enabled = true;
        }
    }

    auto cap_it = element->inline_style.find("stroke-linecap");
    if (cap_it != element->inline_style.end()) {
        const std::string& cap = cap_it->second;
        if (cap == "round") stroke.line_cap = NVG_ROUND;
        else if (cap == "square") stroke.line_cap = NVG_SQUARE;
        else stroke.line_cap = NVG_BUTT;
    }

    auto join_it = element->inline_style.find("stroke-linejoin");
    if (join_it != element->inline_style.end()) {
        const std::string& join = join_it->second;
        if (join == "round") stroke.line_join = NVG_ROUND;
        else if (join == "bevel") stroke.line_join = NVG_BEVEL;
        else stroke.line_join = NVG_MITER;
    }

    auto miter_it = element->inline_style.find("stroke-miterlimit");
    if (miter_it != element->inline_style.end()) {
        float limit = safe_stof(miter_it->second, stroke.miter_limit);
        if (limit > 0.0f) stroke.miter_limit = limit;
    }

    auto dash_it = element->inline_style.find("stroke-dasharray");
    if (dash_it != element->inline_style.end()) {
        std::string value = dash_it->second;
        stroke.dash_array.clear();
        if (!value.empty() && value != "none") {
            std::string normalized = value;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');
            std::stringstream ss(normalized);
            std::string token;
            while (ss >> token) {
                float dash_value = nvgcss_utils::parse_length(token, stroke.width);
                if (dash_value > 0.0f) {
                    stroke.dash_array.push_back(dash_value);
                }
            }
        }
    }

    auto offset_it = element->inline_style.find("stroke-dashoffset");
    if (offset_it != element->inline_style.end()) {
        stroke.dash_offset = nvgcss_utils::parse_length(offset_it->second, stroke.width);
    }

    if (stroke_forced_none) {
        stroke.enabled = false;
    }
    return stroke;
}

void NVGCSSPainter::paint_rect_stroke(const NVGCSSElement* element,
                                      const NVGCSSBox& box) {
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

static bool extract_coordinate(const NVGCSSElement* element,
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
    out_value = origin + nvgcss_utils::parse_length(it->second, context);
    return true;
}

void NVGCSSPainter::paint_line_shape(const NVGCSSElement* element,
                                     const NVGCSSBox& box) {
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
    bool use_rough = (element->style.svg_stroke.rendering == nvgcss::StrokeRendering::ROUGH);
    
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
}

void NVGCSSPainter::paint_circle_shape(const NVGCSSElement* element,
                                       const NVGCSSBox& box) {
    float cx = box.x + box.width * 0.5f;
    float cy = box.y + box.height * 0.5f;
    float rx = box.width * 0.5f;
    float ry = box.height * 0.5f;

    if (element->circle_geometry.defined) {
        cx = element->circle_geometry.cx;
        cy = element->circle_geometry.cy;
        rx = element->circle_geometry.rx > 0 ? element->circle_geometry.rx : rx;
        ry = element->circle_geometry.ry > 0 ? element->circle_geometry.ry : ry;
    }

    extract_coordinate(element, "cx", box.width, box.x, cx);
    extract_coordinate(element, "cy", box.height, box.y, cy);

    auto r_it = element->inline_style.find("r");
    if (r_it != element->inline_style.end()) {
        float r_value = nvgcss_utils::parse_length(r_it->second, std::max(box.width, box.height));
        if (r_value > 0.0f) {
            rx = ry = r_value;
        }
    }
    auto rx_it = element->inline_style.find("rx");
    if (rx_it != element->inline_style.end()) {
        rx = nvgcss_utils::parse_length(rx_it->second, box.width);
    }
    auto ry_it = element->inline_style.find("ry");
    if (ry_it != element->inline_style.end()) {
        ry = nvgcss_utils::parse_length(ry_it->second, box.height);
    }

    // Check if rough rendering is enabled
    printf("[PAINTER] paint_circle_shape BEFORE CHECK: id='%s' ptr=%p rendering=%d\n",
           element->id.c_str(), (void*)element, (int)element->style.svg_stroke.rendering);
    
    bool use_rough = (element->style.svg_stroke.rendering == nvgcss::StrokeRendering::ROUGH);
    
    printf("[PAINTER] paint_circle_shape AFTER CHECK: id='%s' rendering=%d (AUTO=%d, ROUGH=%d) use_rough=%d roughness=%.2f\n",
           element->id.c_str(), 
           (int)element->style.svg_stroke.rendering,
           (int)nvgcss::StrokeRendering::AUTO,
           (int)nvgcss::StrokeRendering::ROUGH,
           use_rough, 
           element->style.svg_stroke.roughness);

    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second != "none") {
        NVGcolor fill_color = nvgcss_utils::parse_color(fill_it->second);
        
        if (use_rough) {
            // Use rough rendering for fill
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
            // Standard smooth rendering
            nvgBeginPath(vg_);
            if (std::abs(rx - ry) < 0.001f) {
                nvgCircle(vg_, cx, cy, rx);
            } else {
                nvgEllipse(vg_, cx, cy, rx, ry);
            }
            nvgFillColor(vg_, fill_color);
            nvgFill(vg_);
        }
    }

    auto stroke = resolve_stroke_style(element, &box, std::max(rx, ry));
    
    printf("[PAINTER] Stroke resolved: enabled=%d width=%.2f use_rough=%d\n", 
           stroke.enabled, stroke.width, use_rough);
    
    if (!stroke.enabled || stroke.width <= 0.0f) {
        printf("[PAINTER] Skipping stroke - not enabled or zero width\n");
        return;
    }
    
    if (use_rough) {
        printf("[PAINTER] *** USING ROUGH RENDERING ***\n");
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

void NVGCSSPainter::paint_svg_path(const NVGCSSElement* element, const NVGCSSBox& box) {
    auto d_it = element->inline_style.find("d");
    if (d_it == element->inline_style.end() || d_it->second.empty()) return;

    const std::string& path_data = d_it->second;
    nvgBeginPath(vg_);

    // Parse SVG path data
    float x = 0, y = 0;  // Current position
    float start_x = 0, start_y = 0;  // Subpath start
    size_t i = 0;
    
    while (i < path_data.length()) {
        while (i < path_data.length() && std::isspace(path_data[i])) i++;
        if (i >= path_data.length()) break;
        
        char cmd = path_data[i++];
        std::vector<float> args;
        
        // Parse numbers
        while (i < path_data.length()) {
            while (i < path_data.length() && (std::isspace(path_data[i]) || path_data[i] == ',')) i++;
            if (i >= path_data.length() || std::isalpha(path_data[i])) break;
            
            size_t end;
            args.push_back(std::stof(path_data.substr(i), &end));
            i += end;
        }
        
        // Execute command
        switch (cmd) {
            case 'M': if (args.size() >= 2) { x = args[0]; y = args[1]; start_x = x; start_y = y; nvgMoveTo(vg_, x, y); } break;
            case 'm': if (args.size() >= 2) { x += args[0]; y += args[1]; start_x = x; start_y = y; nvgMoveTo(vg_, x, y); } break;
            case 'L': if (args.size() >= 2) { x = args[0]; y = args[1]; nvgLineTo(vg_, x, y); } break;
            case 'l': if (args.size() >= 2) { x += args[0]; y += args[1]; nvgLineTo(vg_, x, y); } break;
            case 'H': if (args.size() >= 1) { x = args[0]; nvgLineTo(vg_, x, y); } break;
            case 'h': if (args.size() >= 1) { x += args[0]; nvgLineTo(vg_, x, y); } break;
            case 'V': if (args.size() >= 1) { y = args[0]; nvgLineTo(vg_, x, y); } break;
            case 'v': if (args.size() >= 1) { y += args[0]; nvgLineTo(vg_, x, y); } break;
            case 'C': if (args.size() >= 6) { nvgBezierTo(vg_, args[0], args[1], args[2], args[3], args[4], args[5]); x = args[4]; y = args[5]; } break;
            case 'c': if (args.size() >= 6) { nvgBezierTo(vg_, x+args[0], y+args[1], x+args[2], y+args[3], x+args[4], y+args[5]); x += args[4]; y += args[5]; } break;
            case 'Q': if (args.size() >= 4) { nvgQuadTo(vg_, args[0], args[1], args[2], args[3]); x = args[2]; y = args[3]; } break;
            case 'q': if (args.size() >= 4) { nvgQuadTo(vg_, x+args[0], y+args[1], x+args[2], y+args[3]); x += args[2]; y += args[3]; } break;
            case 'Z': case 'z': nvgLineTo(vg_, start_x, start_y); x = start_x; y = start_y; break;
        }
    }

    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && fill_it->second != "none") {
        nvgFillColor(vg_, nvgcss_utils::parse_color(fill_it->second));
        nvgFill(vg_);
    }

    auto stroke = resolve_stroke_style(element, &box, std::max(box.width, box.height));
    if (stroke.enabled && stroke.width > 0.0f) {
        apply_stroke_state(vg_, stroke);
        nvgStroke(vg_);
    }
}

void NVGCSSPainter::paint_freehand_path(const NVGCSSElement* element) {
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
static int load_background_image(const std::string& path, NVGCSSRenderer* renderer) {
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
static void render_background_image(const NVGCSSElement* element,
                                    const NVGCSSBox& box,
                                    NVGCSSRenderer* renderer) {
    // Get background-image from element's inline style (not yet in typed system)
    // TODO: Add background_image to typed ComputedStyle
    auto image_it = element->inline_style.find("background-image");
    if (image_it == element->inline_style.end() || image_it->second.empty() || image_it->second == "none") {
        return;  // No background image
    }

    // Parse image URL
    std::string image_path = nvgcss_utils::parse_background_image(image_it->second);
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

    // Parse background-size from element's explicit style (not yet in typed system)
    // TODO: Add background_size to typed ComputedStyle
    BackgroundSize size_mode;
    float size_width, size_height;
    std::string size_str = element->explicit_style.background_size;
    nvgcss_utils::parse_background_size(size_str, size_mode, size_width, size_height);

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

    // Parse background-position from element's explicit style (not yet in typed system)
    // TODO: Add background_position to typed ComputedStyle
    std::string pos_str = element->explicit_style.background_position;
    BackgroundPosition position = nvgcss_utils::parse_background_position(pos_str);

    // Calculate position offset
    float offset_x, offset_y;

    if (position.x_is_percent) {
        offset_x = box.x + (box.width - final_width) * position.x;
    } else {
        offset_x = box.x + position.x;
    }

    if (position.y_is_percent) {
        offset_y = box.y + (box.height - final_height) * position.y;
    } else {
        offset_y = box.y + position.y;
    }

    // Parse background-repeat from element's explicit style (not yet in typed system)
    // TODO: Add background_repeat to typed ComputedStyle
    std::string repeat_str = element->explicit_style.background_repeat;
    BackgroundRepeat repeat = nvgcss_utils::parse_background_repeat(repeat_str);

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
