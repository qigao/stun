/*
 * Flex Engine - ThorVG Renderer Implementation
 *
 * Simplified implementation using unified Paint API.
 * All geometry is rendered via SVG path strings.
 */

#include "flex/renderer.h"
#include <thorvg.h>
#include <cmath>
#include <vector>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex {

// ============================================================================
// SVG Arc to Bezier Conversion
// ============================================================================

// Convert SVG arc parameters to center parameterization
// Returns: cx, cy, theta1, dtheta
static void arc_to_center(
    float x1, float y1, float x2, float y2,
    float rx, float ry, float phi,
    bool large_arc, bool sweep,
    float* out_cx, float* out_cy, float* out_theta1, float* out_dtheta)
{
    // Handle degenerate cases
    if (rx == 0 || ry == 0) {
        *out_cx = x1; *out_cy = y1;
        *out_theta1 = 0; *out_dtheta = 0;
        return;
    }

    rx = std::abs(rx);
    ry = std::abs(ry);

    float cos_phi = std::cos(phi);
    float sin_phi = std::sin(phi);

    // Step 1: Compute (x1', y1')
    float dx = (x1 - x2) / 2.0f;
    float dy = (y1 - y2) / 2.0f;
    float x1p = cos_phi * dx + sin_phi * dy;
    float y1p = -sin_phi * dx + cos_phi * dy;

    // Step 2: Compute (cx', cy')
    float x1p2 = x1p * x1p;
    float y1p2 = y1p * y1p;
    float rx2 = rx * rx;
    float ry2 = ry * ry;

    // Correct radii if too small
    float lambda = x1p2 / rx2 + y1p2 / ry2;
    if (lambda > 1) {
        float sqrt_lambda = std::sqrt(lambda);
        rx *= sqrt_lambda;
        ry *= sqrt_lambda;
        rx2 = rx * rx;
        ry2 = ry * ry;
    }

    float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    float denom = rx2 * y1p2 + ry2 * x1p2;

    float sq = 0;
    if (denom > 0 && num > 0) {
        sq = std::sqrt(num / denom);
    }
    if (large_arc == sweep) sq = -sq;

    float cxp = sq * rx * y1p / ry;
    float cyp = -sq * ry * x1p / rx;

    // Step 3: Compute (cx, cy)
    float cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0f;
    float cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0f;

    // Step 4: Compute theta1 and dtheta
    auto angle = [](float ux, float uy, float vx, float vy) -> float {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        float ang = (len > 0) ? std::acos(std::clamp(dot / len, -1.0f, 1.0f)) : 0;
        if (ux * vy - uy * vx < 0) ang = -ang;
        return ang;
    };

    float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                         (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    // Adjust dtheta based on sweep flag
    if (!sweep && dtheta > 0) dtheta -= 2 * static_cast<float>(M_PI);
    if (sweep && dtheta < 0) dtheta += 2 * static_cast<float>(M_PI);

    *out_cx = cx;
    *out_cy = cy;
    *out_theta1 = theta1;
    *out_dtheta = dtheta;
}

// Approximate an arc segment (max 90 degrees) with a cubic bezier
static void arc_segment_to_bezier(
    tvg::Shape* shape,
    float cx, float cy, float rx, float ry,
    float cos_phi, float sin_phi,
    float theta1, float dtheta)
{
    // Use standard arc-to-bezier approximation
    float t = std::tan(dtheta / 4.0f);
    float alpha = std::sin(dtheta) * (std::sqrt(4.0f + 3.0f * t * t) - 1.0f) / 3.0f;

    float cos_t1 = std::cos(theta1);
    float sin_t1 = std::sin(theta1);
    float cos_t2 = std::cos(theta1 + dtheta);
    float sin_t2 = std::sin(theta1 + dtheta);

    // Start point (should already be current point)
    float x1 = cx + rx * cos_phi * cos_t1 - ry * sin_phi * sin_t1;
    float y1 = cy + rx * sin_phi * cos_t1 + ry * cos_phi * sin_t1;

    // End point
    float x2 = cx + rx * cos_phi * cos_t2 - ry * sin_phi * sin_t2;
    float y2 = cy + rx * sin_phi * cos_t2 + ry * cos_phi * sin_t2;

    // Control points
    float dx1 = -rx * cos_phi * sin_t1 - ry * sin_phi * cos_t1;
    float dy1 = -rx * sin_phi * sin_t1 + ry * cos_phi * cos_t1;
    float dx2 = -rx * cos_phi * sin_t2 - ry * sin_phi * cos_t2;
    float dy2 = -rx * sin_phi * sin_t2 + ry * cos_phi * cos_t2;

    float cp1x = x1 + alpha * dx1;
    float cp1y = y1 + alpha * dy1;
    float cp2x = x2 - alpha * dx2;
    float cp2y = y2 - alpha * dy2;

    shape->cubicTo(cp1x, cp1y, cp2x, cp2y, x2, y2);
}

// Draw SVG arc using bezier curves
static void draw_arc(
    tvg::Shape* shape,
    float x1, float y1,           // Start point (current point)
    float x2, float y2,           // End point
    float rx, float ry,           // Radii
    float x_axis_rotation,        // In degrees
    bool large_arc, bool sweep)
{
    // Handle degenerate cases
    if (x1 == x2 && y1 == y2) return;
    if (rx == 0 || ry == 0) {
        shape->lineTo(x2, y2);
        return;
    }

    float phi = x_axis_rotation * static_cast<float>(M_PI) / 180.0f;
    float cos_phi = std::cos(phi);
    float sin_phi = std::sin(phi);

    // Get center parameterization
    float cx, cy, theta1, dtheta;
    arc_to_center(x1, y1, x2, y2, rx, ry, phi, large_arc, sweep,
                  &cx, &cy, &theta1, &dtheta);

    // Correct radii (may have been adjusted)
    rx = std::abs(rx);
    ry = std::abs(ry);

    // Split into segments of max 90 degrees
    int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (M_PI / 2.0f)));
    if (segments < 1) segments = 1;

    float segment_dtheta = dtheta / segments;
    float current_theta = theta1;

    for (int i = 0; i < segments; ++i) {
        arc_segment_to_bezier(shape, cx, cy, rx, ry, cos_phi, sin_phi,
                              current_theta, segment_dtheta);
        current_theta += segment_dtheta;
    }
}

// ============================================================================
// SVG Path Parser
// ============================================================================

static void parse_svg_path(tvg::Shape* shape, const std::string& d) {
    if (d.empty()) return;

    float cx = 0, cy = 0;  // Current point
    float sx = 0, sy = 0;  // Start of subpath (for Z command)
    size_t i = 0;
    char cmd = 0;

    auto skip_ws = [&]() {
        while (i < d.size() && (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ','))
            ++i;
    };

    auto parse_num = [&]() -> float {
        skip_ws();
        size_t start = i;
        if (i < d.size() && (d[i] == '-' || d[i] == '+')) ++i;
        while (i < d.size() && ((d[i] >= '0' && d[i] <= '9') || d[i] == '.')) ++i;
        if (start == i) return 0;
        return std::stof(d.substr(start, i - start));
    };

    while (i < d.size()) {
        skip_ws();
        if (i >= d.size()) break;

        char c = d[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            cmd = c;
            ++i;
        }

        bool relative = (cmd >= 'a' && cmd <= 'z');
        char ucmd = relative ? (cmd - 32) : cmd;

        switch (ucmd) {
            case 'M': {
                float x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }
                shape->moveTo(x, y);
                cx = sx = x; cy = sy = y;
                cmd = relative ? 'l' : 'L';
                break;
            }
            case 'L': {
                float x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }
                shape->lineTo(x, y);
                cx = x; cy = y;
                break;
            }
            case 'H': {
                float x = parse_num();
                if (relative) x += cx;
                shape->lineTo(x, cy);
                cx = x;
                break;
            }
            case 'V': {
                float y = parse_num();
                if (relative) y += cy;
                shape->lineTo(cx, y);
                cy = y;
                break;
            }
            case 'C': {
                float x1 = parse_num(), y1 = parse_num();
                float x2 = parse_num(), y2 = parse_num();
                float x = parse_num(), y = parse_num();
                if (relative) {
                    x1 += cx; y1 += cy; x2 += cx; y2 += cy; x += cx; y += cy;
                }
                shape->cubicTo(x1, y1, x2, y2, x, y);
                cx = x; cy = y;
                break;
            }
            case 'Q': {
                float x1 = parse_num(), y1 = parse_num();
                float x = parse_num(), y = parse_num();
                if (relative) { x1 += cx; y1 += cy; x += cx; y += cy; }
                float cx1 = cx + 2.0f/3.0f * (x1 - cx);
                float cy1 = cy + 2.0f/3.0f * (y1 - cy);
                float cx2 = x + 2.0f/3.0f * (x1 - x);
                float cy2 = y + 2.0f/3.0f * (y1 - y);
                shape->cubicTo(cx1, cy1, cx2, cy2, x, y);
                cx = x; cy = y;
                break;
            }
            case 'A': {
                float rx = parse_num(), ry = parse_num();
                float rotation = parse_num();
                float large_arc_flag = parse_num();
                float sweep_flag = parse_num();
                float x = parse_num(), y = parse_num();
                if (relative) { x += cx; y += cy; }

                draw_arc(shape, cx, cy, x, y, rx, ry, rotation,
                         large_arc_flag != 0, sweep_flag != 0);
                cx = x; cy = y;
                break;
            }
            case 'Z': {
                shape->close();
                cx = sx; cy = sy;
                break;
            }
            default:
                ++i;
                break;
        }
    }
}

// ============================================================================
// ThorVG Renderer Implementation
// ============================================================================

class ThorVGRenderer : public Renderer {
public:
    explicit ThorVGRenderer(tvg::Canvas* canvas) : canvas_(canvas) {
        state_stack_.reserve(32);
    }

    // Frame Control
    void begin_frame(float width, float height, float pixel_ratio) override {
        // Clear all paints from previous frame
        canvas_->remove();

        width_ = width;
        height_ = height;
        pixel_ratio_ = pixel_ratio;
        global_alpha_ = 1.0f;
        current_transform_ = Transform{};
        current_shadow_ = Shadow{};
        current_blur_ = BlurFilter{};
        state_stack_.clear();
    }

    void end_frame() override {
        canvas_->draw(true);  // true = clear buffer before drawing
        canvas_->sync();
    }

    // Transform Stack
    void save() override {
        state_stack_.push_back({current_transform_, global_alpha_, current_shadow_, current_blur_});
    }

    void restore() override {
        if (!state_stack_.empty()) {
            current_transform_ = state_stack_.back().transform;
            global_alpha_ = state_stack_.back().alpha;
            current_shadow_ = state_stack_.back().shadow;
            current_blur_ = state_stack_.back().blur;
            state_stack_.pop_back();
        }
    }

    void reset() override {
        current_transform_ = Transform{};
        global_alpha_ = 1.0f;
        current_shadow_ = Shadow{};
        current_blur_ = BlurFilter{};
    }

    void translate(float x, float y) override {
        // Translation should be affected by current scale
        // This ensures proper transform composition: T' = T + S * delta
        current_transform_.tx += x * current_transform_.sx;
        current_transform_.ty += y * current_transform_.sy;
    }

    void rotate(float degrees) override {
        current_transform_.rotation += degrees;
    }

    void scale(float sx, float sy) override {
        current_transform_.sx *= sx;
        current_transform_.sy *= sy;
    }

    // Clipping
    void clip_rect(float x, float y, float w, float h) override {
        (void)x; (void)y; (void)w; (void)h;
    }

    void reset_clip() override {}

    // Opacity
    void set_global_alpha(float alpha) override {
        global_alpha_ = alpha;
    }

    // Effects (Shadow and Blur)
    void set_shadow(const Shadow& shadow) override {
        current_shadow_ = shadow;
    }

    void clear_shadow() override {
        current_shadow_ = Shadow{};
    }

    void set_blur(const BlurFilter& blur) override {
        current_blur_ = blur;
    }

    void clear_blur() override {
        current_blur_ = BlurFilter{};
    }

    // Core Drawing
    void fill_path(const std::string& d, const Paint& paint) override {
        auto shape = tvg::Shape::gen();
        parse_svg_path(shape, d);
        apply_paint_fill(shape, paint);
        apply_transform(shape);
        canvas_->push(shape);
    }

    void stroke_path(const std::string& d, const Paint& paint, float width) override {
        auto shape = tvg::Shape::gen();
        parse_svg_path(shape, d);
        apply_paint_stroke(shape, paint, width);
        apply_transform(shape);
        canvas_->push(shape);
    }

    // Text
    void draw_text(const std::string& text, float x, float y,
                  const std::string& font_family, float font_size,
                  bool bold, const Color& color) override {
        auto tvg_text = tvg::Text::gen();
        tvg_text->font(font_family.c_str());
        tvg_text->size(font_size);
        tvg_text->text(text.c_str());
        tvg_text->translate(current_transform_.tx + x, current_transform_.ty + y);
        tvg_text->fill(to_u8(color.r), to_u8(color.g), to_u8(color.b));
        tvg_text->opacity(static_cast<uint8_t>(color.a * global_alpha_ * 255));
        canvas_->push(tvg_text);
        (void)bold;
    }

    // Image
    void draw_image(const std::string& src, float x, float y,
                   float width, float height) override {
        auto picture = tvg::Picture::gen();
        if (picture->load(src.c_str()) != tvg::Result::Success) return;

        if (width > 0 && height > 0) {
            float pw, ph;
            picture->size(&pw, &ph);
            if (pw > 0 && ph > 0) picture->size(width, height);
        }
        picture->translate(current_transform_.tx + x, current_transform_.ty + y);
        picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
        canvas_->push(picture);
    }

    // SVG
    void draw_svg(const std::string& src, float x, float y,
                 float width, float height) override {
        auto picture = tvg::Picture::gen();
        if (picture->load(src.c_str()) != tvg::Result::Success) return;

        if (width > 0 && height > 0) {
            float pw, ph;
            picture->size(&pw, &ph);
            if (pw > 0 && ph > 0) picture->size(width, height);
        }
        picture->translate(current_transform_.tx + x, current_transform_.ty + y);
        picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
        canvas_->push(picture);
    }

    void draw_svg_data(const std::string& data, float x, float y,
                      float width, float height) override {
        auto picture = tvg::Picture::gen();
        if (picture->load(data.c_str(), static_cast<uint32_t>(data.size()),
                         "svg+xml", nullptr, false) != tvg::Result::Success) return;

        if (width > 0 && height > 0) {
            float pw, ph;
            picture->size(&pw, &ph);
            if (pw > 0 && ph > 0) picture->size(width, height);
        }
        picture->translate(current_transform_.tx + x, current_transform_.ty + y);
        picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
        canvas_->push(picture);
    }

    // Clear
    void clear(const Color& color) override {
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, width_, height_, 0, 0);
        shape->fill(to_u8(color.r), to_u8(color.g), to_u8(color.b), to_u8(color.a));
        canvas_->push(shape);
    }

private:
    tvg::Canvas* canvas_;
    float width_ = 0, height_ = 0, pixel_ratio_ = 1.0f;
    float global_alpha_ = 1.0f;
    Shadow current_shadow_;
    BlurFilter current_blur_;

    struct Transform {
        float tx = 0, ty = 0;
        float sx = 1, sy = 1;
        float rotation = 0;
    };
    struct SavedState {
        Transform transform;
        float alpha = 1.0f;
        Shadow shadow;
        BlurFilter blur;
    };
    std::vector<SavedState> state_stack_;
    Transform current_transform_;

    static uint8_t to_u8(float f) {
        return static_cast<uint8_t>(f * 255.0f);
    }

    void apply_transform(tvg::Shape* shape) {
        if (current_transform_.tx != 0 || current_transform_.ty != 0)
            shape->translate(current_transform_.tx, current_transform_.ty);
        if (current_transform_.rotation != 0)
            shape->rotate(current_transform_.rotation);
        if (current_transform_.sx != 1)
            shape->scale(current_transform_.sx);
    }

    void apply_paint_fill(tvg::Shape* shape, const Paint& paint) {
        switch (paint.type) {
            case Paint::Type::Solid: {
                uint8_t a = to_u8(paint.color.a * global_alpha_);
                shape->fill(to_u8(paint.color.r), to_u8(paint.color.g),
                           to_u8(paint.color.b), a);
                break;
            }
            case Paint::Type::Linear: {
                auto* fill = tvg::LinearGradient::gen();
                fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100,
                            paint.linear.x2 * 100, paint.linear.y2 * 100);
                apply_gradient_stops(fill, paint.linear.stops);
                shape->fill(fill);
                break;
            }
            case Paint::Type::Radial: {
                auto* fill = tvg::RadialGradient::gen();
                fill->radial(paint.radial.cx * 100, paint.radial.cy * 100,
                            paint.radial.radius * 100,
                            paint.radial.cx * 100, paint.radial.cy * 100, 0);
                apply_gradient_stops(fill, paint.radial.stops);
                shape->fill(fill);
                break;
            }
        }
    }

    void apply_paint_stroke(tvg::Shape* shape, const Paint& paint, float width) {
        shape->strokeWidth(width);
        switch (paint.type) {
            case Paint::Type::Solid: {
                uint8_t a = to_u8(paint.color.a * global_alpha_);
                shape->strokeFill(to_u8(paint.color.r), to_u8(paint.color.g),
                                 to_u8(paint.color.b), a);
                break;
            }
            case Paint::Type::Linear: {
                auto* fill = tvg::LinearGradient::gen();
                fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100,
                            paint.linear.x2 * 100, paint.linear.y2 * 100);
                apply_gradient_stops(fill, paint.linear.stops);
                shape->strokeFill(fill);
                break;
            }
            case Paint::Type::Radial: {
                auto* fill = tvg::RadialGradient::gen();
                fill->radial(paint.radial.cx * 100, paint.radial.cy * 100,
                            paint.radial.radius * 100,
                            paint.radial.cx * 100, paint.radial.cy * 100, 0);
                apply_gradient_stops(fill, paint.radial.stops);
                shape->strokeFill(fill);
                break;
            }
        }
    }

    template<typename GradientT>
    void apply_gradient_stops(GradientT* fill, const std::vector<ColorStop>& stops) {
        if (stops.empty()) return;
        std::vector<tvg::Fill::ColorStop> tvg_stops;
        tvg_stops.reserve(stops.size());
        for (const auto& stop : stops) {
            tvg::Fill::ColorStop cs;
            cs.offset = stop.offset;
            cs.r = to_u8(stop.color.r);
            cs.g = to_u8(stop.color.g);
            cs.b = to_u8(stop.color.b);
            cs.a = to_u8(stop.color.a * global_alpha_);
            tvg_stops.push_back(cs);
        }
        fill->colorStops(tvg_stops.data(), static_cast<uint32_t>(tvg_stops.size()));
    }
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Renderer> create_thorvg_renderer(tvg::Canvas* canvas) {
    return std::make_unique<ThorVGRenderer>(canvas);
}

} // namespace flex
