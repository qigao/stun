/*
 * flexUI - Basic Shape Classes
 *
 * Concrete drawable shapes for widget composition.
 * All shapes emit RenderCommandList commands for backend-agnostic drawing.
 */

#ifndef FLEXUI_SHAPES_H
#define FLEXUI_SHAPES_H
 
#include <algorithm>
#include "flexUI/group.h"
#include "flexUI/text_layout.h"
#include "flexUI/text_util.h"
#include <stb_sprintf.h>
#include <string>

namespace flexUI {

/**
 * RectShape - Rectangle with optional rounded corners
 */
class RectShape : public Drawable {
public:
    RectShape() = default;
    RectShape(float x, float y, float w, float h, float radius = 0)
        : width_(w), height_(h), radius_(radius) {
        x_ = x;
        y_ = y;
    }

    // Dimensions
    float width() const { return width_; }
    float height() const { return height_; }
    void set_size(float w, float h) { width_ = w; height_ = h; }

    // Corner radius
    float radius() const { return radius_; }
    void set_radius(float r) { radius_ = r; }

    // Fill
    void set_fill(const Color& c) { fill_ = Paint::solid(c); }
    void set_fill(const Paint& p) { fill_ = p; }
    const Paint& fill() const { return fill_; }

    // Stroke
    void set_stroke(const Color& c, float width = 1.0f) {
        stroke_ = Paint::solid(c);
        stroke_width_ = width;
    }
    void set_stroke(const Paint& p, float width = 1.0f) {
        stroke_ = p;
        stroke_width_ = width;
    }
    const Paint& stroke() const { return stroke_; }
    float stroke_width() const { return stroke_width_; }

    // Drawable interface
    void draw(RenderCommandList& commands, const Transform& parent, float alpha) override {
        if (!visible_ || (fill_.type == Paint::Type::None && stroke_.type == Paint::Type::None))
            return;

        Transform world = parent * flex::make_translation(x_, y_);

        // Apply alpha to fill/stroke
        Paint fill_with_alpha = fill_;
        Paint stroke_with_alpha = stroke_;

        if (fill_with_alpha.type == Paint::Type::Solid) {
            fill_with_alpha.color.a *= alpha;
        }
        if (stroke_with_alpha.type == Paint::Type::Solid) {
            stroke_with_alpha.color.a *= alpha;
        }

        commands.save();
        commands.set_transform(world);
        commands.draw_rect(0, 0, width_, height_, radius_, fill_with_alpha, stroke_with_alpha, stroke_width_);
        commands.restore();
    }

    Bounds local_bounds() const override {
        float half_stroke = stroke_width_ / 2.0f;
        return Bounds{x_ - half_stroke, y_ - half_stroke,
                      width_ + stroke_width_, height_ + stroke_width_};
    }

private:
    float width_ = 0;
    float height_ = 0;
    float radius_ = 0;
    Paint fill_ = Paint::none();
    Paint stroke_ = Paint::none();
    float stroke_width_ = 0;
};

/**
 * CircleShape - Circle or ellipse
 */
class CircleShape : public Drawable {
public:
    CircleShape() = default;
    CircleShape(float cx, float cy, float r)
        : radius_x_(r), radius_y_(r) {
        x_ = cx;
        y_ = cy;
    }
    CircleShape(float cx, float cy, float rx, float ry)
        : radius_x_(rx), radius_y_(ry) {
        x_ = cx;
        y_ = cy;
    }

    // Radius
    float radius() const { return radius_x_; }
    float radius_x() const { return radius_x_; }
    float radius_y() const { return radius_y_; }
    void set_radius(float r) { radius_x_ = radius_y_ = r; }
    void set_radius(float rx, float ry) { radius_x_ = rx; radius_y_ = ry; }

    // Fill
    void set_fill(const Color& c) { fill_ = Paint::solid(c); }
    void set_fill(const Paint& p) { fill_ = p; }

    // Stroke
    void set_stroke(const Color& c, float width = 1.0f) {
        stroke_ = Paint::solid(c);
        stroke_width_ = width;
    }

    // Drawable interface
    void draw(RenderCommandList& commands, const Transform& parent, float alpha) override {
        if (!visible_) return;

        Transform world = parent * flex::make_translation(x_, y_);

        Paint fill_with_alpha = fill_;
        Paint stroke_with_alpha = stroke_;

        if (fill_with_alpha.type == Paint::Type::Solid) {
            fill_with_alpha.color.a *= alpha;
        }
        if (stroke_with_alpha.type == Paint::Type::Solid) {
            stroke_with_alpha.color.a *= alpha;
        }

        commands.save();
        commands.set_transform(world);
        if (radius_x_ == radius_y_) {
            commands.draw_circle(0, 0, radius_x_, fill_with_alpha, stroke_with_alpha, stroke_width_);
        } else {
            commands.draw_ellipse(0, 0, radius_x_, radius_y_, fill_with_alpha, stroke_with_alpha, stroke_width_);
        }
        commands.restore();
    }

    Bounds local_bounds() const override {
        float half_stroke = stroke_width_ / 2.0f;
        return Bounds{x_ - radius_x_ - half_stroke, y_ - radius_y_ - half_stroke,
                      (radius_x_ + half_stroke) * 2, (radius_y_ + half_stroke) * 2};
    }

private:
    float radius_x_ = 0;
    float radius_y_ = 0;
    Paint fill_ = Paint::none();
    Paint stroke_ = Paint::none();
    float stroke_width_ = 0;
};

/**
 * TextShape - Text rendering
 */
class TextShape : public Drawable {
public:
    TextShape() = default;
    TextShape(float x, float y, const std::string& text)
        : text_(text) {
        x_ = x;
        y_ = y;
    }

    // Text content
    const std::string& text() const { return text_; }
    void set_text(const std::string& t) { text_ = t; }

    // Font
    const std::string& font_family() const { return font_family_; }
    void set_font_family(const std::string& f) { font_family_ = f; }

    float font_size() const { return font_size_; }
    void set_font_size(float s) { font_size_ = s; }

    bool bold() const { return bold_; }
    void set_bold(bool b) { bold_ = b; }

    // Color
    const Color& color() const { return color_; }
    void set_color(const Color& c) { color_ = c; }

    // Drawable interface
    void draw(RenderCommandList& commands, const Transform& parent, float alpha) override {
        if (!visible_ || text_.empty()) return;

        Transform world = parent * flex::make_translation(x_, y_);

        Color color_with_alpha = color_;
        color_with_alpha.a *= alpha;

        commands.save();
        commands.set_transform(world);
        float current_x = 0.0f;
        for (const auto& segment : segment_text(text_)) {
            const std::string font_name =
                segment.type == TextSegmentType::Emoji ? get_emoji_font_name()
                                                       : resolved_font_family();
            commands.draw_text(segment.text, current_x, 0, font_name, font_size_, bold_,
                               color_with_alpha);
            current_x += segment_width(segment);
        }
        commands.restore();
    }

    Bounds local_bounds() const override {
        float approx_width = 0.0f;
        for (const auto& segment : segment_text(text_)) {
            approx_width += segment_width(segment);
        }
        float approx_height = font_size_;
        return Bounds{x_, y_ - approx_height, approx_width, approx_height};
    }

private:
    std::string resolved_font_family() const {
        return font_family_.empty() ? "Arial" : font_family_;
    }

    float segment_width(const TextSegment& segment) const {
        if (segment.type == TextSegmentType::Emoji) {
            return static_cast<float>(utf8_scalar_count(segment.text)) * font_size_;
        }

        ComputedStyle measure_style;
        measure_style.font_family = resolved_font_family();
        measure_style.font_size = font_size_;
        measure_style.font_weight = bold_ ? FontWeight::Bold : FontWeight::Normal;
        return approximate_text_width(&measure_style, segment.text);
    }

    std::string text_;
    std::string font_family_ = "Arial";
    float font_size_ = 14.0f;
    bool bold_ = false;
    Color color_{0.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * LineShape - Simple line between two points
 */
class LineShape : public Drawable {
public:
    LineShape() = default;
    LineShape(float x1, float y1, float x2, float y2, float width = 1.0f) {
        x_ = x1;
        y_ = y1;
        x2_ = x2;
        y2_ = y2;
        stroke_width_ = width;
    }

    // End point (start point is x_, y_)
    float x2() const { return x2_; }
    float y2() const { return y2_; }
    float stroke_width() const { return stroke_width_; }
    void set_end(float x2, float y2) { x2_ = x2; y2_ = y2; }

    // Stroke
    void set_stroke(const Color& c, float width = 1.0f) {
        stroke_ = Paint::solid(c);
        stroke_width_ = width;
    }

    // Drawable interface
    void draw(RenderCommandList& commands, const Transform& parent, float alpha) override {
        if (!visible_) return;

        Paint stroke_with_alpha = stroke_;
        if (stroke_with_alpha.type == Paint::Type::Solid) {
            stroke_with_alpha.color.a *= alpha;
        }

        commands.save();
        commands.set_transform(parent);
        commands.draw_line(x_, y_, x2_, y2_, stroke_with_alpha, stroke_width_);
        commands.restore();
    }

    Bounds local_bounds() const override {
        float half_stroke = stroke_width_ / 2.0f;
        float min_x = std::min(x_, x2_) - half_stroke;
        float min_y = std::min(y_, y2_) - half_stroke;
        float max_x = std::max(x_, x2_) + half_stroke;
        float max_y = std::max(y_, y2_) + half_stroke;
        return Bounds{min_x, min_y, max_x - min_x, max_y - min_y};
    }

private:
    float x2_ = 0;
    float y2_ = 0;
    Paint stroke_{Color{0.0f, 0.0f, 0.0f, 1.0f}};
    float stroke_width_ = 1.0f;
};

/**
 * PathShape - Custom SVG path
 */
class PathShape : public Drawable {
public:
    PathShape() = default;
    PathShape(const std::string& path_data) : path_data_(path_data) {}

    // Path data
    const std::string& path_data() const { return path_data_; }
    void set_path_data(const std::string& d) { path_data_ = d; }

    // Fill
    void set_fill(const Color& c) { fill_ = Paint::solid(c); }
    void set_fill(const Paint& p) { fill_ = p; }

    // Stroke
    void set_stroke(const Color& c, float width = 1.0f) {
        stroke_ = Paint::solid(c);
        stroke_width_ = width;
    }

    // Drawable interface
    void draw(RenderCommandList& commands, const Transform& parent, float alpha) override {
        if (!visible_ || path_data_.empty()) return;

        Transform world = parent * flex::make_translation(x_, y_);

        Paint fill_with_alpha = fill_;
        Paint stroke_with_alpha = stroke_;

        if (fill_with_alpha.type == Paint::Type::Solid) {
            fill_with_alpha.color.a *= alpha;
        }
        if (stroke_with_alpha.type == Paint::Type::Solid) {
            stroke_with_alpha.color.a *= alpha;
        }

        commands.save();
        commands.set_transform(world);
        if (fill_.type != Paint::Type::None) {
            commands.fill_path(path_data_, fill_with_alpha);
        }
        if (stroke_.type != Paint::Type::None) {
            commands.stroke_path(path_data_, stroke_with_alpha, stroke_width_);
        }
        commands.restore();
    }

    Bounds local_bounds() const override {
        // Approximate - would need path parsing for accurate bounds
        return Bounds{x_, y_, 0, 0};
    }

private:
    std::string path_data_;
    Paint fill_ = Paint::none();
    Paint stroke_ = Paint::none();
    float stroke_width_ = 1.0f;
};

} // namespace flexUI

#endif // FLEXUI_SHAPES_H
