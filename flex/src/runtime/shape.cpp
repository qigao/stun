/*
 * Flex Engine - Shape Implementation
 *
 * Clean implementation using Geometry variant and Paint.
 */
#include "flex/runtime/shape.h"
#include "flex/runtime/renderer.h"
#include <algorithm>
#include <thorvg.h>

namespace flex {

// ============================================================================
// Path Cache
// ============================================================================

void Shape::update_cached_path() {
  cached_path_ = geometry_to_path(geometry_, rough_);
  cached_fill_path_ = geometry_to_fill_path(geometry_, rough_);
}

// ============================================================================
// Geometry Type Query
// ============================================================================

GeometryType Shape::geometry_type() const {
  return std::visit(
      [](auto &&g) -> GeometryType {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return GeometryType::None;
        else if constexpr (std::is_same_v<T, RectData>)
          return GeometryType::Rect;
        else if constexpr (std::is_same_v<T, CircleData>)
          return GeometryType::Circle;
        else if constexpr (std::is_same_v<T, EllipseData>)
          return GeometryType::Ellipse;
        else if constexpr (std::is_same_v<T, PolygonData>)
          return GeometryType::Polygon;
        else if constexpr (std::is_same_v<T, PathData>)
          return GeometryType::Path;
        else if constexpr (std::is_same_v<T, StarData>)
          return GeometryType::Star;
        else if constexpr (std::is_same_v<T, LineData>)
          return GeometryType::Line;
        else if constexpr (std::is_same_v<T, RingData>)
          return GeometryType::Ring;
        else if constexpr (std::is_same_v<T, TriangleData>)
          return GeometryType::Triangle;
        else
          return GeometryType::None;
      },
      geometry_);
}

// ============================================================================
// Geometry Setters
// ============================================================================

void Shape::set_rect(float width, float height, float corner_radius) {
  geometry_ = RectData{(std::max)(0.f, width), (std::max)(0.f, height), (std::max)(0.f, corner_radius)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_circle(float radius) {
  geometry_ = CircleData{(std::max)(0.f, radius)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_ellipse(float rx, float ry) {
  geometry_ = EllipseData{(std::max)(0.f, rx), (std::max)(0.f, ry)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_polygon(int sides, float radius) {
  geometry_ = PolygonData{(std::max)(3, sides), (std::max)(0.f, radius)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_path(const std::string &d) {
  geometry_ = PathData{d, 0, 0}; // No bounds specified
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_path(const std::string &d, float width, float height, float x, float y) {
  geometry_ = PathData{d, (std::max)(0.f, width), (std::max)(0.f, height), x, y};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_star(int points, float outer_radius, float inner_radius) {
  geometry_ =
      StarData{(std::max)(3, points), (std::max)(0.f, outer_radius), (std::max)(0.f, inner_radius)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_line(float x2, float y2) {
  geometry_ = LineData{x2, y2};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_ring(float outer_radius, float inner_radius) {
  geometry_ = RingData{(std::max)(0.f, outer_radius), (std::max)(0.f, inner_radius)};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

void Shape::set_triangle(float width, float height, Direction direction) {
  geometry_ = TriangleData{(std::max)(0.f, width), (std::max)(0.f, height), direction};
  update_cached_path();
  mark_dirty(DirtyFlags::Content | DirtyFlags::Bounds);
}

// ============================================================================
// Geometry Getters (compute from variant)
// ============================================================================

RectGeometry Shape::rect() const {
  if (auto *r = std::get_if<RectData>(&geometry_)) {
    return {r->width, r->height, r->corner_radius};
  }
  return {};
}

CircleGeometry Shape::circle() const {
  if (auto *c = std::get_if<CircleData>(&geometry_)) {
    return {c->radius};
  }
  return {};
}

EllipseGeometry Shape::ellipse() const {
  if (auto *e = std::get_if<EllipseData>(&geometry_)) {
    return {e->rx, e->ry};
  }
  return {};
}

PolygonGeometry Shape::polygon() const {
  if (auto *p = std::get_if<PolygonData>(&geometry_)) {
    return {p->sides, p->radius};
  }
  return {};
}

PathGeometry Shape::path() const {
  if (auto *p = std::get_if<PathData>(&geometry_)) {
    return {p->d};
  }
  return {};
}

StarGeometry Shape::star() const {
  if (auto *s = std::get_if<StarData>(&geometry_)) {
    return {s->points, s->outer_radius, s->inner_radius};
  }
  return {};
}

LineGeometry Shape::line() const {
  if (auto *l = std::get_if<LineData>(&geometry_)) {
    return {l->x2, l->y2};
  }
  return {};
}

RingGeometry Shape::ring() const {
  if (auto *r = std::get_if<RingData>(&geometry_)) {
    return {r->outer_radius, r->inner_radius};
  }
  return {};
}

TriangleGeometry Shape::triangle() const {
  if (auto *t = std::get_if<TriangleData>(&geometry_)) {
    return {t->width, t->height, t->direction};
  }
  return {};
}

// ============================================================================
// Paint Setters
// ============================================================================

void Shape::set_fill(const Color &color) {
  fill_ = Paint(color);
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(uint32_t rgba) {
  fill_ = Paint(Color::from_rgba32(rgba));
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(const LinearGradient &gradient) {
  fill_ = Paint(gradient);
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_fill(const RadialGradient &gradient) {
  fill_ = Paint(gradient);
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const Color &color, float width) {
  stroke_ = Paint(color);
  stroke_width_ = width;
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(uint32_t rgba, float width) {
  stroke_ = Paint(Color::from_rgba32(rgba));
  stroke_width_ = width;
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const LinearGradient &gradient, float width) {
  stroke_ = Paint(gradient);
  stroke_width_ = width;
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

void Shape::set_stroke(const RadialGradient &gradient, float width) {
  stroke_ = Paint(gradient);
  stroke_width_ = width;
  mark_dirty(DirtyFlags::Content | DirtyFlags::Visual);
}

// ============================================================================
// Paint Getters (compute from Paint)
// ============================================================================

Fill Shape::fill() const {
  Fill f;
  if (!fill_)
    return f;

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
  if (!stroke_)
    return s;

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

// Helper to render a rough path with "salted" (randomized) segment thicknesses
static void split_and_render_rough(flex::Renderer &r, const std::string &path,
                                   const flex::Paint &paint, float base_width, float salt,
                                   unsigned int seed) {
  if (path.empty())
    return;
  if (salt <= 0.0f) {
    r.stroke_path(path, paint, base_width);
    return;
  }

  flex::detail::RoughRandom rng(seed + 123);

  // Find each 'M' (MoveTo) and render segments independently with jittered width
  size_t start = 0;
  while (start < path.length()) {
    size_t next_m = path.find('M', start + 1);
    std::string segment =
        path.substr(start, (next_m == std::string::npos) ? std::string::npos : (next_m - start));

    // Randomize width: base_width * (1.0 +/- salt)
    float jitter = rng.range(-salt, salt);
    float w = (std::max)(0.1f, base_width * (1.0f + jitter));

    r.stroke_path(segment, paint, w);
    if (next_m == std::string::npos)
      break;
    start = next_m;
  }
}

void Shape::render(Renderer &r) {
  if (!visible_)
    return;

  bool is_rough = (rough_.roughness > 0);

  // -------------------------------------------
  // RETAINED MODE: Use cached paint objects
  // -------------------------------------------
  if (!is_rough && r.supports_retained_mode()) {
    // Already cached and no changes? Skip entirely!
    if (cached_paint() && !is_dirty(DirtyFlags::Content)) {
      if (is_dirty(DirtyFlags::Transform)) {
        r.update_transform(cached_paint(), world_transform());
        clear_dirty(DirtyFlags::Transform);
      }
      return;  // Cached object still valid
    }

    // Need to rebuild
    Paint f = fill_ ? *fill_ : Paint();
    Paint s = stroke_ ? *stroke_ : Paint();
    float sw = stroke_width_;
    float alpha = opacity_;

    // Remove old cached object if exists
    if (cached_paint()) {
      r.remove_cached(cached_paint());
      invalidate_cache();
    }

    // Create new cached object
    PaintHandle new_paint = std::visit([&](auto&& g) -> PaintHandle {
      using T = std::decay_t<decltype(g)>;

      if constexpr (std::is_same_v<T, RectData>) {
        return r.push_rect(0, 0, g.width, g.height, g.corner_radius, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, CircleData>) {
        return r.push_circle(0, 0, g.radius, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, EllipseData>) {
        return r.push_ellipse(0, 0, g.rx, g.ry, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, PolygonData>) {
        return r.push_polygon(g.sides, g.radius, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, StarData>) {
        return r.push_star(g.points, g.outer_radius, g.inner_radius, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, PathData>) {
        return r.push_path(g.d, f, s, sw, world_transform(), alpha);
      } else if constexpr (std::is_same_v<T, LineData>) {
        // Line not yet supported in retained mode
        return nullptr;
      } else if constexpr (std::is_same_v<T, RingData>) {
        // Ring not yet supported in retained mode
        return nullptr;
      }
      return nullptr;
    }, geometry_);

    if (new_paint) {
      set_cached_paint(new_paint);
      clear_dirty(DirtyFlags::Content | DirtyFlags::Transform);
      return;
    }
    // Fall through to immediate mode for unsupported geometry
  }

  // -------------------------------------------
  // IMMEDIATE MODE: Fallback for rough/complex shapes
  // -------------------------------------------

  // Helper to apply local transform
  auto apply_local_transform = [&]() {
      r.translate(x_, y_);
      if (rotation_ != 0.0f) {
          r.rotate(rotation_);
      }
      if (scale_x_ != 1.0f || scale_y_ != 1.0f) {
          r.scale(scale_x_, scale_y_);
      }
  };

  // OPTIMIZATION: Use direct primitive rendering if not rough
  if (!is_rough) {
      r.save();
      apply_local_transform();
      if (opacity_ < 1.0f) r.set_global_alpha(opacity_);

      bool handled = std::visit([&](auto&& g) -> bool {
          using T = std::decay_t<decltype(g)>;

          Paint f = fill_ ? *fill_ : Paint();
          Paint s = stroke_ ? *stroke_ : Paint();
          float sw = stroke_width_;

          if constexpr (std::is_same_v<T, RectData>) {
              r.draw_rect(0, 0, g.width, g.height, g.corner_radius, f, s, sw);
              return true;
          } else if constexpr (std::is_same_v<T, CircleData>) {
              r.draw_circle(0, 0, g.radius, f, s, sw);
              return true;
          } else if constexpr (std::is_same_v<T, EllipseData>) {
              r.draw_ellipse(0, 0, g.rx, g.ry, f, s, sw);
              return true;
          }
          return false;
      }, geometry_);

      r.restore();
      if (handled) return;
  }

  // Fallback to path rendering for rough shapes or complex geometry
  if (cached_path_.empty()) {
    // Debug: Why is path empty?
    fprintf(stderr, "Warning: Shape '%s' has empty cached_path_\n", id_.c_str());
    return;
  }

  r.save();

  // Apply local transform (relative to parent, accumulated by renderer stack)
  apply_local_transform();

  if (opacity_ < 1.0f)
    r.set_global_alpha(opacity_);

  // 1. Draw sketchy fill (if any)
  if (fill_ && !cached_fill_path_.empty()) {
    split_and_render_rough(r, cached_fill_path_, *fill_, rough_.fill_weight,
                           rough_.stroke_width_randomness * 0.5f, rough_.seed + 777);
  }
  // Otherwise draw solid fill (only if no sketchy fill is generated)
  else if (fill_) {
    r.fill_path(cached_path_, *fill_);
  }

  // 2. Draw outline
  if (stroke_) {
    split_and_render_rough(r, cached_path_, *stroke_, stroke_width_, rough_.stroke_width_randomness,
                           rough_.seed);
  }

  r.restore();
}

// ============================================================================
// Property Access
// ============================================================================

// ============================================================================
// Bounds - Using variant visitor
// ============================================================================

Bounds Shape::compute_bounds() const {
  Bounds b;
  b.x = 0;
  b.y = 0;

  std::visit(
      [&](auto &&g) {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          b.width = 0;
          b.height = 0;
        } else if constexpr (std::is_same_v<T, RectData>) {
          b.width = g.width;
          b.height = g.height;
        } else if constexpr (std::is_same_v<T, CircleData>) {
          b.width = g.radius * 2;
          b.height = g.radius * 2;
          b.x -= g.radius;
          b.y -= g.radius;
        } else if constexpr (std::is_same_v<T, EllipseData>) {
          b.width = g.rx * 2;
          b.height = g.ry * 2;
          b.x -= g.rx;
          b.y -= g.ry;
        } else if constexpr (std::is_same_v<T, PolygonData>) {
          // Polygon is centered by default in our generator
          b.width = g.radius * 2;
          b.height = g.radius * 2;
          b.x -= g.radius;
          b.y -= g.radius;
        } else if constexpr (std::is_same_v<T, PathData>) {
          b.width = (g.width > 0 ? g.width : 10);
          b.height = (g.height > 0 ? g.height : 10);
          b.x = g.x;
          b.y = g.y;
        } else if constexpr (std::is_same_v<T, StarData>) {
          b.width = g.outer_radius * 2;
          b.height = g.outer_radius * 2;
          b.x -= g.outer_radius;
          b.y -= g.outer_radius;
        } else if constexpr (std::is_same_v<T, LineData>) {
          b.width = std::abs(g.x2);
          b.height = std::abs(g.y2);
          if (g.x2 < 0) b.x += g.x2;
          if (g.y2 < 0) b.y += g.y2;
        } else if constexpr (std::is_same_v<T, RingData>) {
          b.width = g.outer_radius * 2;
          b.height = g.outer_radius * 2;
          b.x -= g.outer_radius;
          b.y -= g.outer_radius;
        } else if constexpr (std::is_same_v<T, TriangleData>) {
          // Triangle is centered at origin
          b.width = g.width;
          b.height = g.height;
          b.x -= g.width / 2;
          b.y -= g.height / 2;
        }
      },
      geometry_);

  return b;
}

// ============================================================================
// Animation Property Dispatch
// ============================================================================

static Color get_color_value(const AnimValue& value) {
    if (auto* c = std::get_if<Color>(&value)) {
        return *c;
    } else if (auto* s = std::get_if<std::string>(&value)) {
        return Color::from_hex(s->c_str());
    }
    return Color::Black;
}

bool Shape::set_animated_property(PropertyID pid, const AnimValue& value) {
    // Try base class first
    if (Node::set_animated_property(pid, value)) return true;

    switch (pid) {
    case PropertyID::Width:
        if (auto* f = std::get_if<float>(&value)) {
            auto r = rect();
            if (r.width != *f) set_rect(*f, r.height, r.corner_radius);
            return true;
        }
        break;

    case PropertyID::Height:
        if (auto* f = std::get_if<float>(&value)) {
            auto r = rect();
            if (r.height != *f) set_rect(r.width, *f, r.corner_radius);
            return true;
        }
        break;

    case PropertyID::Radius:
        if (auto* f = std::get_if<float>(&value)) {
            auto c = circle();
            if (c.radius != *f) set_circle(*f);
            return true;
        }
        break;

    case PropertyID::Fill:
    case PropertyID::Color: {
        Color new_color = get_color_value(value);
        auto f = fill();
        if (f.color != new_color) set_fill(new_color);
        return true;
    }

    case PropertyID::FillOpacity:
        if (auto* f = std::get_if<float>(&value)) {
            auto current = fill();
            if (current.color.a != *f) {
                Color c = current.color;
                c.a = *f;
                set_fill(c);
            }
            return true;
        }
        break;

    case PropertyID::Stroke: {
        Color new_color = get_color_value(value);
        auto s = stroke();
        if (s.color != new_color) set_stroke(new_color, s.width);
        return true;
    }

    case PropertyID::StrokeWidth:
        if (auto* f = std::get_if<float>(&value)) {
            auto s = stroke();
            if (s.width != *f) set_stroke(s.color, *f);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

} // namespace flex
