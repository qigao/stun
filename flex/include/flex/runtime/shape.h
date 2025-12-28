/*
 * Flex Engine - Shape Node
 *
 * Geometry node with primitives (Rect, Circle, Path) and paint (Fill, Stroke).
 * Refactored to use unified Geometry variant and Paint types.
 */

#pragma once

#include "flex/runtime/node.h"
#include "flex/runtime/types.h"
#include "flex/runtime/geometry.h"
#include "flex/runtime/allocator.h"
#include <string>
#include <optional>

namespace flex {

struct RectGeometry {
    float width = 0;
    float height = 0;
    float corner_radius = 0;
};

struct CircleGeometry {
    float radius = 0;
};

struct EllipseGeometry {
    float rx = 0;
    float ry = 0;
};

struct PolygonGeometry {
    int sides = 3;
    float radius = 0;
};

struct StarGeometry {
    int points = 5;
    float outer_radius = 0;
    float inner_radius = 0;
};

struct LineGeometry {
    float x2 = 0;
    float y2 = 0;
};

struct RingGeometry {
    float outer_radius = 0;
    float inner_radius = 0;
};

struct TriangleGeometry {
    float width = 0;
    float height = 0;
    Direction direction = Direction::Right;
};

struct PathGeometry {
    std::string d;
};

// ============================================================================
// Paint Types (kept for backward compatibility)
// ============================================================================

enum class FillType : uint8_t {
    Solid,
    LinearGradient,
    RadialGradient,
};

struct Fill {
    FillType type = FillType::Solid;
    Color color = {1, 1, 1, 1};
    flex::LinearGradient linear_gradient;
    flex::RadialGradient radial_gradient;
};

enum class StrokeType : uint8_t {
    Solid,
    LinearGradient,
    RadialGradient,
};

struct Stroke {
    StrokeType type = StrokeType::Solid;
    Color color = {0, 0, 0, 1};
    float width = 1.0f;
    flex::LinearGradient linear_gradient;
    flex::RadialGradient radial_gradient;
};

// ============================================================================
// Shape - Geometry node with fill/stroke
// ============================================================================

class Shape : public Node {
public:
    using Ptr = Shape*;

    Shape() = default;
    ~Shape() override = default;

    static Ptr create(ArenaAllocator& arena) { return arena.create<Shape>(); }

    NodeType type() const override { return NodeType::Shape; }
    const char* type_name() const override { return "Shape"; }

    // -------------------------------------------
    // Geometry
    // -------------------------------------------

    GeometryType geometry_type() const;

    void set_rect(float width, float height, float corner_radius = 0);
    RectGeometry rect() const;  // Returns by value (computed from variant)

    void set_circle(float radius);
    CircleGeometry circle() const;

    void set_ellipse(float rx, float ry);
    EllipseGeometry ellipse() const;

    void set_polygon(int sides, float radius);
    PolygonGeometry polygon() const;

    void set_path(const std::string& d);
    void set_path(const std::string& d, float width, float height, float x = 0.0f, float y = 0.0f);  // With explicit bounds
    void set_path_data(const std::string& d) { set_path(d); }
    PathGeometry path() const;

    void set_star(int points, float outer_radius, float inner_radius = 0);
    StarGeometry star() const;

    void set_line(float x2, float y2);
    LineGeometry line() const;

    void set_ring(float outer_radius, float inner_radius);
    RingGeometry ring() const;

    void set_triangle(float width, float height, Direction direction = Direction::Right);
    TriangleGeometry triangle() const;

    // -------------------------------------------
    // Paint
    // -------------------------------------------

    bool has_fill() const { return fill_.has_value(); }
    Fill fill() const;  // Returns by value (computed from Paint)
    void set_fill(const Color& color);
    void set_fill(uint32_t rgba);
    void set_fill(const flex::LinearGradient& gradient);
    void set_fill(const flex::RadialGradient& gradient);
    void clear_fill() { fill_ = std::nullopt; mark_dirty(DirtyFlags::Content | DirtyFlags::Visual); }

    bool has_stroke() const { return stroke_.has_value(); }
    Stroke stroke() const;  // Returns by value
    void set_stroke(const Color& color, float width = 1.0f);
    void set_stroke(uint32_t rgba, float width = 1.0f);
    void set_stroke(const flex::LinearGradient& gradient, float width = 1.0f);
    void set_stroke(const flex::RadialGradient& gradient, float width = 1.0f);
    void clear_stroke() { stroke_ = std::nullopt; mark_dirty(DirtyFlags::Content | DirtyFlags::Visual); }

    // -------------------------------------------
    // Rough (Hand-drawn) Style
    // -------------------------------------------

    RoughOptions rough() const { return rough_; }
    void set_rough(const RoughOptions& opts) {
        rough_ = opts;
        mark_dirty(DirtyFlags::Content);
        update_cached_path();
    }

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer) override;

    // -------------------------------------------
    // Hit Testing
    // -------------------------------------------

    Bounds compute_bounds() const override;

private:
    // Core data - clean and minimal
    Geometry geometry_;              // variant: ONE geometry type
    std::string cached_path_;        // pre-generated SVG path for outline
    std::string cached_fill_path_;   // pre-generated SVG path for sketchy fill
    std::optional<Paint> fill_;      // optional: has fill or not
    std::optional<Paint> stroke_;    // optional: has stroke or not
    float stroke_width_ = 1.0f;
    RoughOptions rough_ = RoughOptions::disabled();  // Hand-drawn style (disabled by default)

    // Regenerate cached path from geometry
    void update_cached_path();
};

} // namespace flex

namespace flex {
    using PathShape = Shape;
    using RectShape = Shape;
    using CircleShape = Shape;
}
