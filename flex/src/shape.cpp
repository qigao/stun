/*
 * Flex Engine - Shape Implementation
 *
 * Clean implementation using Geometry variant and Paint.
 * No legacy sync code, no redundant storage.
 */

#include "flex/shape.h"
#include "flex/renderer.h"
#include <algorithm>

namespace flex {

// ============================================================================
// Path Cache
// ============================================================================

void Shape::update_cached_path() {
    cached_path_ = geometry_to_path(geometry_);
}

// ============================================================================
// Geometry Type Query
// ============================================================================

GeometryType Shape::geometry_type() const {
    return std::visit([](auto&& g) -> GeometryType {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, std::monostate>) return GeometryType::None;
        else if constexpr (std::is_same_v<T, RectData>) return GeometryType::Rect;
        else if constexpr (std::is_same_v<T, CircleData>) return GeometryType::Circle;
        else if constexpr (std::is_same_v<T, EllipseData>) return GeometryType::Ellipse;
        else if constexpr (std::is_same_v<T, PolygonData>) return GeometryType::Polygon;
        else if constexpr (std::is_same_v<T, PathData>) return GeometryType::Path;
        else if constexpr (std::is_same_v<T, StarData>) return GeometryType::Star;
        else if constexpr (std::is_same_v<T, LineData>) return GeometryType::Line;
        else if constexpr (std::is_same_v<T, RingData>) return GeometryType::Ring;
        else return GeometryType::None;
    }, geometry_);
}

// ============================================================================
// Geometry Setters
// ============================================================================

void Shape::set_rect(float width, float height, float corner_radius) {
    geometry_ = RectData{
        std::max(0.f, width),
        std::max(0.f, height),
        std::max(0.f, corner_radius)
    };
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_circle(float radius) {
    geometry_ = CircleData{std::max(0.f, radius)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_ellipse(float rx, float ry) {
    geometry_ = EllipseData{std::max(0.f, rx), std::max(0.f, ry)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_polygon(int sides, float radius) {
    geometry_ = PolygonData{std::max(3, sides), std::max(0.f, radius)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_path(const std::string& d) {
    geometry_ = PathData{d, 0, 0};  // No bounds specified
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_path(const std::string& d, float width, float height) {
    geometry_ = PathData{d, std::max(0.f, width), std::max(0.f, height)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_star(int points, float outer_radius, float inner_radius) {
    geometry_ = StarData{std::max(3, points), std::max(0.f, outer_radius), std::max(0.f, inner_radius)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_line(float x2, float y2) {
    geometry_ = LineData{x2, y2};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_ring(float outer_radius, float inner_radius) {
    geometry_ = RingData{std::max(0.f, outer_radius), std::max(0.f, inner_radius)};
    update_cached_path();
    mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

// ============================================================================
// Geometry Getters (compute from variant)
// ============================================================================

RectGeometry Shape::rect() const {
    if (auto* r = std::get_if<RectData>(&geometry_)) {
        return {r->width, r->height, r->corner_radius};
    }
    return {};
}

CircleGeometry Shape::circle() const {
    if (auto* c = std::get_if<CircleData>(&geometry_)) {
        return {c->radius};
    }
    return {};
}

EllipseGeometry Shape::ellipse() const {
    if (auto* e = std::get_if<EllipseData>(&geometry_)) {
        return {e->rx, e->ry};
    }
    return {};
}

PolygonGeometry Shape::polygon() const {
    if (auto* p = std::get_if<PolygonData>(&geometry_)) {
        return {p->sides, p->radius};
    }
    return {};
}

PathGeometry Shape::path() const {
    if (auto* p = std::get_if<PathData>(&geometry_)) {
        return {p->d};
    }
    return {};
}

StarGeometry Shape::star() const {
    if (auto* s = std::get_if<StarData>(&geometry_)) {
        return {s->points, s->outer_radius, s->inner_radius};
    }
    return {};
}

LineGeometry Shape::line() const {
    if (auto* l = std::get_if<LineData>(&geometry_)) {
        return {l->x2, l->y2};
    }
    return {};
}

RingGeometry Shape::ring() const {
    if (auto* r = std::get_if<RingData>(&geometry_)) {
        return {r->outer_radius, r->inner_radius};
    }
    return {};
}

// ============================================================================
// Paint Setters
// ============================================================================

void Shape::set_fill(const Color& color) {
    fill_ = Paint(color);
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(uint32_t rgba) {
    fill_ = Paint(Color::from_rgba32(rgba));
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(const LinearGradient& gradient) {
    fill_ = Paint(gradient);
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(const RadialGradient& gradient) {
    fill_ = Paint(gradient);
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const Color& color, float width) {
    stroke_ = Paint(color);
    stroke_width_ = width;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(uint32_t rgba, float width) {
    stroke_ = Paint(Color::from_rgba32(rgba));
    stroke_width_ = width;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const LinearGradient& gradient, float width) {
    stroke_ = Paint(gradient);
    stroke_width_ = width;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const RadialGradient& gradient, float width) {
    stroke_ = Paint(gradient);
    stroke_width_ = width;
    mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

// ============================================================================
// Paint Getters (compute from Paint)
// ============================================================================

Fill Shape::fill() const {
    Fill f;
    if (!fill_) return f;

    switch (fill_->type) {
        case Paint::Type::Solid:
            f.type = FillType::Solid;
            f.color = fill_->color;
            break;
        case Paint::Type::Linear:
            f.type = FillType::LinearGradient;
            f.linear_gradient = fill_->linear;
            break;
        case Paint::Type::Radial:
            f.type = FillType::RadialGradient;
            f.radial_gradient = fill_->radial;
            break;
    }
    return f;
}

Stroke Shape::stroke() const {
    Stroke s;
    s.width = stroke_width_;
    if (!stroke_) return s;

    switch (stroke_->type) {
        case Paint::Type::Solid:
            s.type = StrokeType::Solid;
            s.color = stroke_->color;
            break;
        case Paint::Type::Linear:
            s.type = StrokeType::LinearGradient;
            s.linear_gradient = stroke_->linear;
            break;
        case Paint::Type::Radial:
            s.type = StrokeType::RadialGradient;
            s.radial_gradient = stroke_->radial;
            break;
    }
    return s;
}

// ============================================================================
// Rendering - Clean and Simple
// ============================================================================

void Shape::render(Renderer& r) {
    if (!visible_) return;
    if (cached_path_.empty()) return;

    r.save();

    r.translate(x_, y_);
    if (rotation_ != 0) r.rotate(rotation_);
    if (scale_x_ != 1 || scale_y_ != 1) r.scale(scale_x_, scale_y_);
    if (opacity_ < 1.0f) r.set_global_alpha(opacity_);

    if (fill_) r.fill_path(cached_path_, *fill_);
    if (stroke_) r.stroke_path(cached_path_, *stroke_, stroke_width_);

    r.restore();
}

// ============================================================================
// Property Access
// ============================================================================


// ============================================================================
// Bounds - Using variant visitor
// ============================================================================

Bounds Shape::bounds() const {
    Bounds b;
    b.x = x_;
    b.y = y_;

    std::visit([&](auto&& g) {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            b.width = 0;
            b.height = 0;
        }
        else if constexpr (std::is_same_v<T, RectData>) {
            b.width = g.width * scale_x_;
            b.height = g.height * scale_y_;
        }
        else if constexpr (std::is_same_v<T, CircleData>) {
            b.width = g.radius * 2 * scale_x_;
            b.height = g.radius * 2 * scale_y_;
            b.x -= g.radius * scale_x_;
            b.y -= g.radius * scale_y_;
        }
        else if constexpr (std::is_same_v<T, EllipseData>) {
            b.width = g.rx * 2 * scale_x_;
            b.height = g.ry * 2 * scale_y_;
            b.x -= g.rx * scale_x_;
            b.y -= g.ry * scale_y_;
        }
        else if constexpr (std::is_same_v<T, PolygonData>) {
            b.width = g.radius * 2 * scale_x_;
            b.height = g.radius * 2 * scale_y_;
            b.x -= g.radius * scale_x_;
            b.y -= g.radius * scale_y_;
        }
        else if constexpr (std::is_same_v<T, PathData>) {
            // Use stored bounds if available, otherwise minimal default
            b.width = (g.width > 0 ? g.width : 10) * scale_x_;
            b.height = (g.height > 0 ? g.height : 10) * scale_y_;
        }
        else if constexpr (std::is_same_v<T, StarData>) {
            b.width = g.outer_radius * 2 * scale_x_;
            b.height = g.outer_radius * 2 * scale_y_;
            b.x -= g.outer_radius * scale_x_;
            b.y -= g.outer_radius * scale_y_;
        }
        else if constexpr (std::is_same_v<T, LineData>) {
            b.width = std::abs(g.x2) * scale_x_;
            b.height = std::abs(g.y2) * scale_y_;
            if (g.x2 < 0) b.x += g.x2 * scale_x_;
            if (g.y2 < 0) b.y += g.y2 * scale_y_;
        }
        else if constexpr (std::is_same_v<T, RingData>) {
            b.width = g.outer_radius * 2 * scale_x_;
            b.height = g.outer_radius * 2 * scale_y_;
            b.x -= g.outer_radius * scale_x_;
            b.y -= g.outer_radius * scale_y_;
        }
    }, geometry_);

    return b;
}

} // namespace flex
