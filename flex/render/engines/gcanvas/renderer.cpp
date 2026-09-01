#include "flex/render/engines/gcanvas.h"

#include <gcanvas/context.hpp>
#include <gcanvas/backends/opengl.hpp>
#include <gcanvas/svg.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flex::render::engines::gcanvas {
namespace {

constexpr float kTransformEpsilon = 1.0e-5f;
constexpr std::size_t kMaxClipVertices = 64U;

struct ExternalOpenGLState {
  ::gcanvas::opengl::ProcLoader proc_loader = nullptr;
  void* proc_loader_user_data = nullptr;
  int framebuffer_width = 1;
  int framebuffer_height = 1;
};

::gcanvas::opengl::ProcAddress load_external_gl_proc(void* user_data,
                                                     const char* name) {
  auto& state = *static_cast<ExternalOpenGLState*>(user_data);
  return state.proc_loader(state.proc_loader_user_data, name);
}

void external_framebuffer_size(void* user_data, int* width, int* height) {
  const auto& state = *static_cast<const ExternalOpenGLState*>(user_data);
  *width = state.framebuffer_width;
  *height = state.framebuffer_height;
}

float clamp01(float value) {
  return (std::max)(0.0f, (std::min)(1.0f, value));
}

::gcanvas::color to_color(const Color& color, float alpha) {
  return ::gcanvas::color(clamp01(color.r), clamp01(color.g), clamp01(color.b),
                         clamp01(color.a * alpha));
}

::gcanvas::Transform to_transform(const Transform& transform) {
  return ::gcanvas::Transform{transform.data[0], transform.data[3], transform.data[1],
                              transform.data[4], transform.data[2], transform.data[5]};
}

std::vector<::gcanvas::ColorStop> to_stops(const std::vector<ColorStop>& stops,
                                           float alpha) {
  std::vector<::gcanvas::ColorStop> result;
  result.reserve(stops.size());
  for (const ColorStop& stop : stops) {
    result.push_back({stop.offset, to_color(stop.color, alpha)});
  }
  return result;
}

::gcanvas::Paint to_paint(const Paint& paint, float alpha) {
  switch (paint.type) {
  case Paint::Type::None:
    return ::gcanvas::Paint::none();
  case Paint::Type::Solid:
    return ::gcanvas::Paint::solid(to_color(paint.color, alpha));
  case Paint::Type::Linear:
    return ::gcanvas::Paint::linear_gradient(
        paint.linear.x1 * 100.0f, paint.linear.y1 * 100.0f,
        paint.linear.x2 * 100.0f, paint.linear.y2 * 100.0f,
        to_stops(paint.linear.stops, alpha));
  case Paint::Type::Radial:
    return ::gcanvas::Paint::radial_gradient(
        paint.radial.cx * 100.0f, paint.radial.cy * 100.0f, 0.0f,
        paint.radial.radius * 100.0f, to_stops(paint.radial.stops, alpha));
  }
  throw std::logic_error("invalid Flex paint type");
}

Transform make_rotation_transform(float degrees) {
  const float radians = deg_to_rad(degrees);
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  Transform result;
  result.data[0] = cosine;
  result.data[1] = -sine;
  result.data[3] = sine;
  result.data[4] = cosine;
  return result;
}

bool nearly_equal(float lhs, float rhs) {
  return std::fabs(lhs - rhs) <= kTransformEpsilon;
}

bool axis_aligned(const Transform& transform) {
  return nearly_equal(transform.data[1], 0.0f) &&
         nearly_equal(transform.data[3], 0.0f) &&
         nearly_equal(transform.data[6], 0.0f) &&
         nearly_equal(transform.data[7], 0.0f) &&
         nearly_equal(transform.data[8], 1.0f);
}

float cross(const Vec2& first, const Vec2& second, const Vec2& point) {
  return (second.x - first.x) * (point.y - first.y) -
         (second.y - first.y) * (point.x - first.x);
}

float signed_area(const std::vector<Vec2>& polygon) {
  float twice_area = 0.0f;
  for (std::size_t index = 0; index < polygon.size(); ++index) {
    const Vec2& current = polygon[index];
    const Vec2& next = polygon[(index + 1U) % polygon.size()];
    twice_area += current.x * next.y - current.y * next.x;
  }
  return twice_area * 0.5f;
}

Vec2 line_intersection(const Vec2& start, const Vec2& end,
                       const Vec2& clip_start, const Vec2& clip_end) {
  const float start_distance = cross(clip_start, clip_end, start);
  const float end_distance = cross(clip_start, clip_end, end);
  const float denominator = start_distance - end_distance;
  if (nearly_equal(denominator, 0.0f)) {
    throw std::logic_error("gCanvas clip intersection is numerically singular");
  }
  const float amount = start_distance / denominator;
  return start + (end - start) * amount;
}

// Sutherland-Hodgman intersection is O(subject vertices * 4) for the rectangular clip and
// uses O(result vertices) bounded transient storage.
std::vector<Vec2> intersect_with_convex_quad(const std::vector<Vec2>& subject,
                                             const std::vector<Vec2>& clip) {
  if (subject.empty()) {
    return {};
  }
  const float orientation = signed_area(clip);
  if (!std::isfinite(orientation) || nearly_equal(orientation, 0.0f)) {
    return {};
  }

  std::vector<Vec2> input = subject;
  std::vector<Vec2> output;
  input.reserve(kMaxClipVertices);
  output.reserve(kMaxClipVertices);
  for (std::size_t edge = 0; edge < clip.size(); ++edge) {
    output.clear();
    if (input.empty()) {
      break;
    }
    const Vec2& clip_start = clip[edge];
    const Vec2& clip_end = clip[(edge + 1U) % clip.size()];
    Vec2 start = input.back();
    bool start_inside = cross(clip_start, clip_end, start) * orientation >=
                        -kTransformEpsilon;
    for (const Vec2& end : input) {
      const bool end_inside = cross(clip_start, clip_end, end) * orientation >=
                              -kTransformEpsilon;
      if (start_inside != end_inside) {
        output.push_back(line_intersection(start, end, clip_start, clip_end));
      }
      if (end_inside) {
        output.push_back(end);
      }
      if (output.size() > kMaxClipVertices) {
        throw std::length_error("gCanvas renderer clip vertex limit reached");
      }
      start = end;
      start_inside = end_inside;
    }
    input.swap(output);
  }
  if (input.size() < 3U || nearly_equal(signed_area(input), 0.0f)) {
    input.clear();
  }
  return input;
}

[[noreturn]] void unsupported(const char* operation) {
  throw std::logic_error(std::string("gCanvas renderer does not support ") + operation);
}

class GCanvasRenderer final : public Renderer {
public:
  explicit GCanvasRenderer(::gcanvas::Context& context) : context_(context) {}

  GCanvasRenderer(std::unique_ptr<ExternalOpenGLState> external_state,
                  std::unique_ptr<::gcanvas::Context> context)
      : external_state_(std::move(external_state)),
        owned_context_(std::move(context)), context_(*owned_context_) {}

  void begin_frame(float width, float height, float pixel_ratio) override {
    if (frame_active_) {
      throw std::logic_error("gCanvas renderer frame is already active");
    }
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(pixel_ratio) ||
        width <= 0.0f || height <= 0.0f || pixel_ratio <= 0.0f) {
      throw std::invalid_argument("gCanvas renderer frame dimensions must be positive");
    }
    if (owned_context_) {
      constexpr float kMaximumDimension =
          static_cast<float>((std::numeric_limits<int>::max)());
      if (width > kMaximumDimension || height > kMaximumDimension ||
          width * pixel_ratio > kMaximumDimension ||
          height * pixel_ratio > kMaximumDimension) {
        throw std::length_error("gCanvas OpenGL framebuffer dimensions exceed int range");
      }
      const int logical_width =
          (std::max)(1, static_cast<int>(std::lround(width)));
      const int logical_height =
          (std::max)(1, static_cast<int>(std::lround(height)));
      external_state_->framebuffer_width =
          (std::max)(1, static_cast<int>(std::lround(width * pixel_ratio)));
      external_state_->framebuffer_height =
          (std::max)(1, static_cast<int>(std::lround(height * pixel_ratio)));
      context_.set_metrics({logical_width, logical_height, 1.0f, 1.0f,
                            0.0f, 0.0f, pixel_ratio});
      context_.resize_context(logical_width, logical_height);
    } else if (!nearly_equal(width, static_cast<float>(context_.metrics().width)) ||
               !nearly_equal(height, static_cast<float>(context_.metrics().height))) {
      throw std::invalid_argument(
          "Flex frame dimensions must match the borrowed gCanvas logical metrics");
    }

    width_ = width;
    height_ = height;
    state_ = State{};
    state_stack_.clear();
    context_.remove_rect_mask();
    frame_active_ = true;
  }

  void end_frame() override {
    require_frame();
    context_.remove_rect_mask();
    context_.draw_frame();
    if (owned_context_) {
      context_.present_frame();
    }
    frame_active_ = false;
  }

  void set_retained_mode(bool enabled) override {
    if (enabled) {
      unsupported("retained mode");
    }
    retained_mode_ = false;
  }

  void save() override {
    require_frame();
    state_stack_.push_back(state_);
  }

  void restore() override {
    require_frame();
    if (state_stack_.empty()) {
      throw std::logic_error("gCanvas renderer state stack underflow");
    }
    state_ = state_stack_.back();
    state_stack_.pop_back();
    apply_clip();
  }

  void reset() override {
    require_frame();
    state_ = State{};
    apply_clip();
  }

  void set_transform(const Transform& transform) override {
    require_frame();
    if (!nearly_equal(transform.data[6], 0.0f) ||
        !nearly_equal(transform.data[7], 0.0f) ||
        !nearly_equal(transform.data[8], 1.0f)) {
      throw std::invalid_argument("gCanvas renderer requires an affine transform");
    }
    state_.transform = transform;
  }

  void translate(float x, float y) override {
    require_frame();
    state_.transform = state_.transform * make_translation(x, y);
  }

  void rotate(float degrees) override {
    require_frame();
    if (!std::isfinite(degrees)) {
      throw std::invalid_argument("gCanvas renderer rotation must be finite");
    }
    state_.transform = state_.transform * make_rotation_transform(degrees);
  }

  void scale(float sx, float sy) override {
    require_frame();
    if (!std::isfinite(sx) || !std::isfinite(sy) || nearly_equal(sx, 0.0f) ||
        nearly_equal(sy, 0.0f)) {
      throw std::invalid_argument("gCanvas renderer scale must be finite and non-zero");
    }
    state_.transform = state_.transform * make_scale(sx, sy);
  }

  void clip_rect(float x, float y, float w, float h) override {
    require_frame();
    require_non_negative_size(w, h, "clip rectangle");
    if (!std::isfinite(x) || !std::isfinite(y)) {
      throw std::invalid_argument("gCanvas clip rectangle position must be finite");
    }
    const std::vector<Vec2> clip = transformed_clip_quad(x, y, w, h);
    if (state_.clip_active) {
      state_.clip = intersect_with_convex_quad(state_.clip, clip);
    } else {
      state_.clip = clip;
    }
    state_.clip_active = true;
    apply_clip();
  }

  void reset_clip() override {
    require_frame();
    state_.clip.clear();
    state_.clip_active = false;
    apply_clip();
  }

  void set_global_alpha(float alpha) override {
    require_frame();
    if (!std::isfinite(alpha)) {
      throw std::invalid_argument("gCanvas renderer alpha must be finite");
    }
    const float base = state_stack_.empty() ? 1.0f : state_stack_.back().alpha;
    state_.alpha = clamp01(base * alpha);
  }

  void set_shadow(const Shadow& shadow) override {
    require_frame();
    if (!std::isfinite(shadow.offset_x) || !std::isfinite(shadow.offset_y) ||
        !std::isfinite(shadow.blur) || !std::isfinite(shadow.spread) ||
        !std::isfinite(shadow.color.r) || !std::isfinite(shadow.color.g) ||
        !std::isfinite(shadow.color.b) || !std::isfinite(shadow.color.a) ||
        shadow.blur < 0.0f) {
      throw std::invalid_argument(
          "gCanvas renderer shadow values must be finite with non-negative blur");
    }
    state_.shadow = shadow;
  }

  void clear_shadow() override {
    require_frame();
    state_.shadow = Shadow{};
  }

  void set_blur(const BlurFilter& blur) override {
    require_frame();
    if (!std::isfinite(blur.radius) || blur.radius < 0.0f) {
      throw std::invalid_argument(
          "gCanvas renderer blur radius must be finite and non-negative");
    }
    state_.blur = blur;
  }

  void clear_blur() override {
    require_frame();
    state_.blur = BlurFilter{};
  }

  void fill_path(const std::string& data, const Paint& paint) override {
    require_frame();
    if (paint.type == Paint::Type::None || data.empty()) {
      return;
    }
    const ::gcanvas::Path path = ::gcanvas::Path::from_svg(data);
    validate_sampled_shadow();
    draw_sampled_path_outer_shadow(path, 0.0f);
    draw_path_content(path, paint, 0.0f);
    draw_sampled_path_inset_shadow(path, 0.0f);
  }

  void stroke_path(const std::string& data, const Paint& paint, float width) override {
    require_frame();
    if (paint.type == Paint::Type::None || data.empty() || width <= 0.0f) {
      return;
    }
    const ::gcanvas::Path path = ::gcanvas::Path::from_svg(data);
    validate_sampled_shadow();
    draw_sampled_path_outer_shadow(path, width);
    draw_path_content(path, paint, width);
    draw_sampled_path_inset_shadow(path, width);
  }

  void draw_line(float x1, float y1, float x2, float y2, const Paint& paint,
                 float width) override {
    require_frame();
    if (paint.type == Paint::Type::None || width <= 0.0f) {
      return;
    }
    ::gcanvas::Path path;
    path.move_to(x1, y1).line_to(x2, y2);
    validate_sampled_shadow();
    draw_sampled_path_outer_shadow(path, width);
    draw_path_content(path, paint, width);
    draw_sampled_path_inset_shadow(path, width);
  }

  void draw_rect(float x, float y, float w, float h, float radius,
                 const Paint& fill, const Paint& stroke, float stroke_width) override {
    require_frame();
    require_non_negative_size(w, h, "rectangle");
    if (!std::isfinite(radius) || radius < 0.0f) {
      throw std::invalid_argument("gCanvas rectangle radius must be finite and non-negative");
    }
    const bool analytic_outer = has_shadow() && !state_.shadow.inset &&
                                axis_aligned(state_.transform) && has_uniform_scale();
    if (analytic_outer) {
      draw_rect_shadow(x, y, w, h, radius, fill, stroke, stroke_width);
    }
    if (requires_path(fill, false) || requires_path(stroke, true) ||
        !axis_aligned(state_.transform) || !has_uniform_scale() ||
        has_blur() ||
        (has_shadow() && state_.shadow.inset && fill.type == Paint::Type::None)) {
      ::gcanvas::Path path;
      if (radius > 0.0f) {
        path.rounded_rect(x, y, w, h, radius);
      } else {
        path.rect(x, y, w, h);
      }
      if (state_.shadow.inset &&
          (fill.type == Paint::Type::None || !axis_aligned(state_.transform) ||
           !has_uniform_scale())) {
        validate_sampled_shadow();
      }
      if (has_shadow() && !state_.shadow.inset && !analytic_outer) {
        draw_sampled_path_outer_shadow(
            path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
      }
      if (fill.type != Paint::Type::None) {
        draw_path_content(path, fill, 0.0f);
      }
      if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
        draw_path_content(path, stroke, stroke_width);
      }
      if (state_.shadow.inset) {
        if (fill.type != Paint::Type::None && axis_aligned(state_.transform) &&
            has_uniform_scale()) {
          draw_rect_shadow(x, y, w, h, radius, fill, stroke, stroke_width);
        } else {
          draw_sampled_path_inset_shadow(
              path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
        }
      }
      return;
    }
    const Bounds bounds = transformed_rect(x, y, w, h);
    const float scale = stroke_scale();
    const float transformed_radius = (std::max)(0.0f, radius * scale);

    if (prepare_paint(fill, false)) {
      if (transformed_radius > 0.0f) {
        context_.fill_rounded_rect(bounds.x, bounds.y, bounds.width, bounds.height,
                                   transformed_radius);
      } else {
        context_.fill_rect(bounds.x, bounds.y, bounds.width, bounds.height);
      }
    }
    if (stroke_width > 0.0f && prepare_paint(stroke, true)) {
      context_.set_line_width(stroke_width * scale);
      if (transformed_radius > 0.0f) {
        context_.stroke_rounded_rect(bounds.x, bounds.y, bounds.width, bounds.height,
                                     transformed_radius);
      } else {
        context_.stroke_rect(bounds.x, bounds.y, bounds.width, bounds.height);
      }
    }
    if (state_.shadow.inset) {
      draw_rect_shadow(x, y, w, h, radius, fill, stroke, stroke_width);
    }
  }

  void draw_circle(float cx, float cy, float radius, const Paint& fill,
                   const Paint& stroke, float stroke_width) override {
    require_frame();
    if (!std::isfinite(radius) || radius < 0.0f) {
      throw std::invalid_argument("gCanvas circle radius must be non-negative");
    }
    const bool analytic_outer = has_shadow() && !state_.shadow.inset &&
                                axis_aligned(state_.transform) && has_uniform_scale();
    if (analytic_outer) {
      draw_circle_shadow(cx, cy, radius, fill, stroke, stroke_width);
    }
    if (requires_path(fill, false) || requires_path(stroke, true) ||
        !axis_aligned(state_.transform) || !has_uniform_scale() ||
        has_blur() ||
        (has_shadow() && state_.shadow.inset && fill.type == Paint::Type::None)) {
      ::gcanvas::Path path;
      path.ellipse(cx, cy, radius, radius);
      if (state_.shadow.inset &&
          (fill.type == Paint::Type::None || !axis_aligned(state_.transform) ||
           !has_uniform_scale())) {
        validate_sampled_shadow();
      }
      if (has_shadow() && !state_.shadow.inset && !analytic_outer) {
        draw_sampled_path_outer_shadow(
            path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
      }
      if (fill.type != Paint::Type::None) {
        draw_path_content(path, fill, 0.0f);
      }
      if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
        draw_path_content(path, stroke, stroke_width);
      }
      if (state_.shadow.inset) {
        if (fill.type != Paint::Type::None && axis_aligned(state_.transform) &&
            has_uniform_scale()) {
          draw_circle_shadow(cx, cy, radius, fill, stroke, stroke_width);
        } else {
          draw_sampled_path_inset_shadow(
              path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
        }
      }
      return;
    }
    const Vec2 center = state_.transform * Vec2{cx, cy};
    const float scale = std::fabs(state_.transform.data[0]);
    const float transformed_radius = radius * scale;
    if (prepare_paint(fill, false)) {
      context_.fill_circle(center.x, center.y, transformed_radius);
    }
    if (stroke_width > 0.0f && prepare_paint(stroke, true)) {
      context_.set_line_width(stroke_width * scale);
      context_.stroke_circle(center.x, center.y, transformed_radius);
    }
    if (state_.shadow.inset) {
      draw_circle_shadow(cx, cy, radius, fill, stroke, stroke_width);
    }
  }

  void draw_ellipse(float cx, float cy, float rx, float ry, const Paint& fill,
                    const Paint& stroke, float stroke_width) override {
    require_frame();
    if (!std::isfinite(rx) || !std::isfinite(ry) || rx < 0.0f || ry < 0.0f) {
      throw std::invalid_argument("gCanvas ellipse radii must be finite and non-negative");
    }
    if (rx == 0.0f || ry == 0.0f) {
      return;
    }
    const bool analytic = !has_shadow() && !has_blur() && axis_aligned(state_.transform) &&
                          (fill.type == Paint::Type::None ||
                           fill.type == Paint::Type::Solid) &&
                          (stroke.type == Paint::Type::None ||
                           stroke.type == Paint::Type::Solid);
    if (analytic) {
      const Vec2 center = state_.transform * Vec2{cx, cy};
      const float transformed_rx = rx * std::fabs(state_.transform.data[0]);
      const float transformed_ry = ry * std::fabs(state_.transform.data[4]);
      if (prepare_paint(fill, false)) {
        context_.fill_ellipse(center.x, center.y, transformed_rx, transformed_ry);
      }
      if (stroke_width > 0.0f && prepare_paint(stroke, true)) {
        const float scale = (std::max)(std::fabs(state_.transform.data[0]),
                                       std::fabs(state_.transform.data[4]));
        context_.set_line_width(stroke_width * scale);
        context_.stroke_ellipse(center.x, center.y, transformed_rx, transformed_ry);
      }
      return;
    }
    ::gcanvas::Path path;
    path.ellipse(cx, cy, rx, ry);
    const bool inset_shadow = has_shadow() && state_.shadow.inset;
    if (inset_shadow &&
        (fill.type == Paint::Type::None || !axis_aligned(state_.transform))) {
      validate_sampled_shadow();
    }
    if (!inset_shadow)
      draw_sampled_path_outer_shadow(
          path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
    if (fill.type != Paint::Type::None) {
      draw_path_content(path, fill, 0.0f);
    }
    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      draw_path_content(path, stroke, stroke_width);
    }
    if (inset_shadow) {
      if (fill.type != Paint::Type::None && axis_aligned(state_.transform)) {
        const Vec2 center = state_.transform * Vec2{cx, cy};
        const Vec2 offset = transformed_shadow_offset();
        const float scale = shadow_effect_scale();
        context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
        context_.draw_ellipse_inset_shadow(
            center.x, center.y, rx * std::fabs(state_.transform.data[0]),
            ry * std::fabs(state_.transform.data[4]), offset.x, offset.y,
            state_.shadow.blur * scale, state_.shadow.spread * scale);
      } else {
        draw_sampled_path_inset_shadow(
            path, fill.type != Paint::Type::None ? 0.0f : stroke_width);
      }
    }
  }

  void draw_text(const std::string& text, float x, float y,
                 const std::string& font_family, float font_size, bool bold,
                 const Color& color) override {
    require_frame();
    if (text.empty()) {
      return;
    }
    if (!std::isfinite(font_size) || font_size <= 0.0f) {
      throw std::invalid_argument("gCanvas font size must be positive");
    }
    if (font_size > static_cast<float>((std::numeric_limits<int>::max)())) {
      throw std::length_error("gCanvas font size exceeds the context limit");
    }
    validate_sampled_shadow();
    ::gcanvas::Font* font = resolve_font(font_family, bold);
    if (font != nullptr) {
      context_.set_font(*font);
    } else {
      context_.use_default_font();
    }
    context_.set_font_size((std::max)(1, static_cast<int>(std::lround(font_size))));
    if (has_shadow() && !state_.shadow.inset) {
      prepare_sampled_shadow();
      context_.draw_text_shadow(x, y, text, state_.shadow.blur * shadow_effect_scale(),
                                sampled_shadow_spread(), shadow_transform());
    }
    context_.set_fill_color(to_color(color, state_.alpha));
    if (has_blur()) {
      context_.draw_text_blur(x, y, text, transformed_blur_radius(),
                              to_transform(state_.transform));
    } else {
      context_.draw_text(x, y, text, to_transform(state_.transform));
    }
    if (has_shadow() && state_.shadow.inset) {
      prepare_sampled_shadow();
      const Vec2 offset = transformed_shadow_offset();
      context_.draw_text_inset_shadow(
          x, y, text, offset.x, offset.y,
          state_.shadow.blur * shadow_effect_scale(), sampled_shadow_spread(),
          to_transform(state_.transform));
    }
  }

  bool register_font(const std::string& family, const std::string& path) override {
    if (family.empty() || path.empty()) {
      return false;
    }
    ::gcanvas::Font& font = context_.create_font(path);
    fonts_.insert_or_assign(family, &font);
    return true;
  }

  void unregister_font(const std::string& family) override {
    fonts_.erase(family);
  }

  void draw_image(const std::string& src, float x, float y, float width,
                  float height) override {
    require_frame();
    if (src.empty()) {
      throw std::invalid_argument("gCanvas image source must not be empty");
    }
    require_non_negative_size(width, height, "image");
    validate_sampled_shadow();
    auto found = images_.find(src);
    if (found == images_.end()) {
      ::gcanvas::Image& image = context_.create_image(src);
      found = images_.emplace(src, &image).first;
    }
    if (has_shadow() && !state_.shadow.inset) {
      prepare_sampled_shadow();
      context_.draw_image_shadow(x, y, width, height, *found->second,
                                 state_.shadow.blur * shadow_effect_scale(),
                                 sampled_shadow_spread(), shadow_transform());
    }
    context_.set_fill_color(to_color(Color::White, state_.alpha));
    if (has_blur()) {
      context_.draw_image_blur(x, y, width, height, *found->second,
                               transformed_blur_radius(),
                               to_transform(state_.transform));
    } else {
      context_.draw_image(x, y, width, height, *found->second,
                          to_transform(state_.transform), true);
    }
    if (has_shadow() && state_.shadow.inset) {
      prepare_sampled_shadow();
      const Vec2 offset = transformed_shadow_offset();
      context_.draw_image_inset_shadow(
          x, y, width, height, *found->second, offset.x, offset.y,
          state_.shadow.blur * shadow_effect_scale(), sampled_shadow_spread(),
          to_transform(state_.transform));
    }
  }

  void draw_svg(const std::string& src, float x, float y, float width, float height) override {
    require_frame();
    if (src.empty()) {
      throw std::invalid_argument("gCanvas SVG source must not be empty");
    }
    draw_svg_impl("file:" + src, x, y, width, height,
                  [&src] { return ::gcanvas::SvgDocument::load_file(src); });
  }

  void draw_svg_data(const std::string& data, float x, float y, float width,
                     float height) override {
    require_frame();
    if (data.empty()) {
      throw std::invalid_argument("gCanvas inline SVG data must not be empty");
    }
    const std::string key = "data:" + std::to_string(std::hash<std::string>{}(data)) +
                            ":" + std::to_string(data.size());
    draw_svg_impl(key, x, y, width, height,
                  [&data] { return ::gcanvas::SvgDocument::load_data(data); });
  }

  void clear(const Color& color) override {
    require_frame();
    context_.set_clear_color(to_color(color, 1.0f));
  }

  Bounds viewport() const override {
    return Bounds{0.0f, 0.0f, width_, height_};
  }

  RendererCapabilities capabilities() const override {
    RendererCapabilities caps;
    caps.retained_mode = false;
    caps.surface_recreation = false;
    caps.path_drawing = true;
    caps.raster_images = true;
    caps.svg_images = true;
    caps.rotation = true;
    caps.scaling = true;
    caps.shadow = true;
    caps.blur = true;
    return caps;
  }

  bool supports_retained_mode() const override { return false; }

  void remove_cached(PaintHandle) override { unsupported("cached paint removal"); }

  PaintHandle push_rect(float, float, float, float, float, const Paint&, const Paint&,
                        float, const Transform&, float) override {
    unsupported("retained rectangles");
  }

  PaintHandle push_circle(float, float, float, const Paint&, const Paint&, float,
                          const Transform&, float) override {
    unsupported("retained circles");
  }

  PaintHandle push_ellipse(float, float, float, float, const Paint&, const Paint&, float,
                           const Transform&, float) override {
    unsupported("retained ellipses");
  }

  PaintHandle push_polygon(int, float, const Paint&, const Paint&, float,
                           const Transform&, float) override {
    unsupported("retained polygons");
  }

  PaintHandle push_star(int, float, float, const Paint&, const Paint&, float,
                        const Transform&, float) override {
    unsupported("retained stars");
  }

  PaintHandle push_path(const std::string&, const Paint&, const Paint&, float,
                        const Transform&, float) override {
    unsupported("retained paths");
  }

  PaintHandle push_text(const std::string&, const std::string&, float, bool, const Color&,
                        const Transform&, float) override {
    unsupported("retained text");
  }

  PaintHandle push_image(const std::string&, float, float, const Transform&,
                         float) override {
    unsupported("retained images");
  }

  PaintHandle push_svg(const std::string&, float, float, const Transform&,
                       float) override {
    unsupported("retained SVG images");
  }

  PaintHandle push_svg_data(const std::string&, float, float, const Transform&,
                            float) override {
    unsupported("retained inline SVG images");
  }

  void update_transform(PaintHandle, const Transform&) override {
    unsupported("cached transform updates");
  }

private:
  struct State {
    Transform transform;
    float alpha = 1.0f;
    Shadow shadow;
    BlurFilter blur;
    std::vector<Vec2> clip;
    bool clip_active = false;
  };

  void require_frame() const {
    if (!frame_active_) {
      throw std::logic_error("gCanvas renderer operation requires an active frame");
    }
  }

  static void require_axis_aligned(const Transform& transform) {
    if (!axis_aligned(transform)) {
      unsupported("rotated or sheared transforms");
    }
  }

  static void require_non_negative_size(float width, float height, const char* operation) {
    if (!std::isfinite(width) || !std::isfinite(height) || width < 0.0f || height < 0.0f) {
      throw std::invalid_argument(std::string("gCanvas ") + operation +
                                  " size must be finite and non-negative");
    }
  }

  Bounds transformed_rect(float x, float y, float width, float height) const {
    require_axis_aligned(state_.transform);
    const Vec2 first = state_.transform * Vec2{x, y};
    const Vec2 second = state_.transform * Vec2{x + width, y + height};
    return Bounds{(std::min)(first.x, second.x), (std::min)(first.y, second.y),
                  std::fabs(second.x - first.x), std::fabs(second.y - first.y)};
  }

  std::vector<Vec2> transformed_clip_quad(float x, float y, float width,
                                          float height) const {
    const float right = x + width;
    const float bottom = y + height;
    if (!std::isfinite(right) || !std::isfinite(bottom)) {
      throw std::invalid_argument("gCanvas clip rectangle bounds overflow");
    }
    std::vector<Vec2> result = {
        state_.transform * Vec2{x, y}, state_.transform * Vec2{right, y},
        state_.transform * Vec2{right, bottom}, state_.transform * Vec2{x, bottom}};
    for (const Vec2& point : result) {
      if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument("gCanvas clip transform overflow");
      }
    }
    if (width == 0.0f || height == 0.0f || nearly_equal(signed_area(result), 0.0f)) {
      result.clear();
    }
    return result;
  }

  float stroke_scale() const {
    require_axis_aligned(state_.transform);
    return (std::min)(std::fabs(state_.transform.data[0]),
                      std::fabs(state_.transform.data[4]));
  }

  bool has_uniform_scale() const {
    return axis_aligned(state_.transform) &&
           nearly_equal(std::fabs(state_.transform.data[0]),
                        std::fabs(state_.transform.data[4]));
  }

  bool requires_path(const Paint& paint, bool) const {
    if (paint.type == Paint::Type::None) {
      return false;
    }
    if (paint.type != Paint::Type::Solid) {
      return true;
    }
    return false;
  }

  bool prepare_paint(const Paint& paint, bool stroke) {
    if (paint.type == Paint::Type::None) {
      return false;
    }
    if (paint.type != Paint::Type::Solid) {
      unsupported("gradient paints");
    }
    if (stroke) {
      context_.set_stroke_color(to_color(paint.color, state_.alpha));
    } else {
      context_.set_fill_color(to_color(paint.color, state_.alpha));
    }
    return true;
  }

  bool has_shadow() const {
    return !state_.shadow.is_none() && state_.shadow.color.a > 0.0f;
  }

  bool has_blur() const {
    return !state_.blur.is_none();
  }

  float transformed_blur_radius() const {
    const float radius = state_.blur.radius * shadow_effect_scale();
    if (!std::isfinite(radius)) {
      throw std::invalid_argument(
          "gCanvas transformed blur radius must remain finite");
    }
    return radius;
  }

  void draw_path_content(const ::gcanvas::Path& path, const Paint& paint,
                         float line_width) {
    const ::gcanvas::Paint source = to_paint(paint, state_.alpha);
    const ::gcanvas::Transform transform = to_transform(state_.transform);
    if (has_blur()) {
      context_.draw_path_blur(path, source, line_width,
                              transformed_blur_radius(), transform);
    } else if (line_width > 0.0f) {
      context_.stroke_path(path, source, line_width, transform);
    } else {
      context_.fill_path(path, source, transform);
    }
  }

  float shadow_scale() const {
    if (!has_uniform_scale()) {
      unsupported("primitive shadows with rotated, sheared, or non-uniform transforms");
    }
    return std::fabs(state_.transform.data[0]);
  }

  float shadow_effect_scale() const {
    const float x_scale = std::hypot(state_.transform.data[0], state_.transform.data[3]);
    const float y_scale = std::hypot(state_.transform.data[1], state_.transform.data[4]);
    const float scale = (std::max)(x_scale, y_scale);
    if (!std::isfinite(scale)) {
      throw std::invalid_argument("gCanvas shadow transform scale must be finite");
    }
    return scale;
  }

  Vec2 transformed_shadow_offset() const {
    return Vec2{state_.transform.data[0] * state_.shadow.offset_x +
                    state_.transform.data[1] * state_.shadow.offset_y,
                state_.transform.data[3] * state_.shadow.offset_x +
                    state_.transform.data[4] * state_.shadow.offset_y};
  }

  float sampled_shadow_spread() const {
    return state_.shadow.spread * shadow_effect_scale();
  }

  void validate_sampled_shadow() const {
    if (has_shadow()) {
      const float scale = shadow_effect_scale();
      const float blur = state_.shadow.blur * scale;
      const float spread = state_.shadow.spread * scale;
      const Vec2 offset = transformed_shadow_offset();
      if (!std::isfinite(blur) || !std::isfinite(spread) ||
          !std::isfinite(offset.x) || !std::isfinite(offset.y)) {
        throw std::invalid_argument("gCanvas transformed shadow values must remain finite");
      }
    }
  }

  ::gcanvas::Transform shadow_transform() const {
    ::gcanvas::Transform transform = to_transform(state_.transform);
    const Vec2 offset = transformed_shadow_offset();
    transform.e += offset.x;
    transform.f += offset.y;
    return transform;
  }

  void prepare_sampled_shadow() {
    context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
  }

  void draw_sampled_path_outer_shadow(const ::gcanvas::Path& path, float line_width) {
    if (!has_shadow() || state_.shadow.inset) {
      return;
    }
    prepare_sampled_shadow();
    context_.draw_path_shadow(path, line_width, state_.shadow.blur * shadow_effect_scale(),
                              sampled_shadow_spread(), shadow_transform());
  }

  void draw_sampled_path_inset_shadow(const ::gcanvas::Path& path, float line_width) {
    if (!has_shadow() || !state_.shadow.inset) {
      return;
    }
    prepare_sampled_shadow();
    const Vec2 offset = transformed_shadow_offset();
    context_.draw_path_inset_shadow(
        path, line_width, offset.x, offset.y,
        state_.shadow.blur * shadow_effect_scale(), sampled_shadow_spread(),
        to_transform(state_.transform));
  }

  static bool has_visible_geometry(const Paint& fill, const Paint& stroke,
                                   float stroke_width) {
    return fill.type != Paint::Type::None ||
           (stroke.type != Paint::Type::None && stroke_width > 0.0f);
  }

  void draw_rect_shadow(float x, float y, float width, float height, float radius,
                        const Paint& fill, const Paint& stroke, float stroke_width) {
    if (!has_shadow() || !has_visible_geometry(fill, stroke, stroke_width) ||
        width == 0.0f || height == 0.0f) {
      return;
    }
    const float scale = shadow_scale();
    const Bounds bounds = transformed_rect(x, y, width, height);
    const Vec2 offset = transformed_shadow_offset();
    if (state_.shadow.inset) {
      if (fill.type == Paint::Type::None) {
        unsupported("inset shadows for stroke-only rectangles");
      }
      context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
      if (radius > 0.0f) {
        context_.draw_rounded_rect_inset_shadow(
            bounds.x, bounds.y, bounds.width, bounds.height, radius * scale,
            offset.x, offset.y, state_.shadow.blur * scale,
            state_.shadow.spread * scale);
      } else {
        context_.draw_rect_inset_shadow(
            bounds.x, bounds.y, bounds.width, bounds.height, offset.x, offset.y,
            state_.shadow.blur * scale, state_.shadow.spread * scale);
      }
      return;
    }
    const float stroke_outset =
        stroke.type != Paint::Type::None && stroke_width > 0.0f
            ? stroke_width * scale * 0.5f
            : 0.0f;
    const float spread = state_.shadow.spread * scale + stroke_outset;
    const float shadow_width = bounds.width + spread * 2.0f;
    const float shadow_height = bounds.height + spread * 2.0f;
    if (shadow_width <= 0.0f || shadow_height <= 0.0f) {
      return;
    }

    context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
    const float shadow_x = bounds.x + offset.x - spread;
    const float shadow_y = bounds.y + offset.y - spread;
    const float shadow_radius = (std::max)(0.0f, radius * scale + spread);
    const float blur = state_.shadow.blur * scale;
    if (shadow_radius > 0.0f) {
      context_.draw_rounded_rect_shadow(shadow_x, shadow_y, shadow_width,
                                        shadow_height, shadow_radius, blur);
    } else {
      context_.draw_rect_shadow(shadow_x, shadow_y, shadow_width, shadow_height, blur);
    }
  }

  void draw_circle_shadow(float cx, float cy, float radius, const Paint& fill,
                          const Paint& stroke, float stroke_width) {
    if (!has_shadow() || !has_visible_geometry(fill, stroke, stroke_width) || radius == 0.0f) {
      return;
    }
    const float scale = shadow_scale();
    const Vec2 center = state_.transform * Vec2{cx, cy};
    const Vec2 offset = transformed_shadow_offset();
    if (state_.shadow.inset) {
      if (fill.type == Paint::Type::None) {
        unsupported("inset shadows for stroke-only circles");
      }
      context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
      context_.draw_circle_inset_shadow(
          center.x, center.y, radius * scale, offset.x, offset.y,
          state_.shadow.blur * scale, state_.shadow.spread * scale);
      return;
    }
    const float stroke_outset =
        stroke.type != Paint::Type::None && stroke_width > 0.0f
            ? stroke_width * scale * 0.5f
            : 0.0f;
    const float shadow_radius =
        radius * scale + state_.shadow.spread * scale + stroke_outset;
    if (shadow_radius <= 0.0f) {
      return;
    }
    context_.set_fill_color(to_color(state_.shadow.color, state_.alpha));
    context_.draw_circle_shadow(center.x + offset.x, center.y + offset.y,
                                shadow_radius, state_.shadow.blur * scale);
  }

  void apply_clip() {
    if (!state_.clip_active) {
      context_.remove_rect_mask();
      return;
    }
    if (state_.clip.empty()) {
      context_.set_convex_mask({});
      return;
    }

    bool axis_aligned_clip = true;
    float left = state_.clip.front().x;
    float top = state_.clip.front().y;
    float right = left;
    float bottom = top;
    for (std::size_t index = 0; index < state_.clip.size(); ++index) {
      const Vec2& current = state_.clip[index];
      const Vec2& next = state_.clip[(index + 1U) % state_.clip.size()];
      axis_aligned_clip = axis_aligned_clip &&
                          (nearly_equal(current.x, next.x) ||
                           nearly_equal(current.y, next.y));
      left = (std::min)(left, current.x);
      top = (std::min)(top, current.y);
      right = (std::max)(right, current.x);
      bottom = (std::max)(bottom, current.y);
    }
    if (axis_aligned_clip) {
      context_.set_rect_mask(left, top, right - left, bottom - top);
      return;
    }

    std::vector<::gcanvas::vec2> vertices;
    vertices.reserve(state_.clip.size());
    for (const Vec2& point : state_.clip) {
      vertices.emplace_back(point.x, point.y);
    }
    context_.set_convex_mask(vertices);
  }

  ::gcanvas::Font* resolve_font(const std::string& family, bool bold) const {
    const auto find_font = [this](const std::string& name) -> ::gcanvas::Font* {
      const auto found = fonts_.find(name);
      return found == fonts_.end() ? nullptr : found->second;
    };
    if (bold && !family.empty()) {
      if (::gcanvas::Font* font = find_font(family + "-bold")) {
        return font;
      }
    }
    if (!family.empty()) {
      if (::gcanvas::Font* font = find_font(family)) {
        return font;
      }
    }
    if (bold) {
      if (::gcanvas::Font* font = find_font("sans-serif-bold")) {
        return font;
      }
    }
    return find_font("sans-serif");
  }

  template <typename Loader>
  void draw_svg_impl(const std::string& source_key, float x, float y, float width,
                     float height, Loader&& loader) {
    require_non_negative_size(width, height, "SVG");
    validate_sampled_shadow();
    const std::string cache_key = source_key + "|" + std::to_string(width) + "|" +
                                  std::to_string(height);
    auto found = svg_images_.find(cache_key);
    int image_width = 0;
    int image_height = 0;

    if (found == svg_images_.end()) {
      auto document = loader();
      if (!document) {
        return;
      }

      constexpr float max_dimension = static_cast<float>((std::numeric_limits<int>::max)());
      if (width > max_dimension || height > max_dimension) {
        throw std::length_error("gCanvas SVG dimensions exceed PlutoSVG limits");
      }
      const int target_width = width > 0.0f ? (std::max)(1, static_cast<int>(std::lround(width))) : -1;
      const int target_height =
          height > 0.0f ? (std::max)(1, static_cast<int>(std::lround(height))) : -1;
      ::gcanvas::SvgBitmap bitmap = document->rasterize(target_width, target_height);
      if (bitmap.empty()) {
        return;
      }

      image_width = bitmap.width;
      image_height = bitmap.height;
      ::gcanvas::Image& image = context_.create_image(
          bitmap.width, bitmap.height, 4, bitmap.rgba.data(), bitmap.rgba.size());
      found = svg_images_.emplace(cache_key, &image).first;
    } else {
      image_width = found->second->get_width();
      image_height = found->second->get_height();
    }

    const float draw_width = width > 0.0f ? width : static_cast<float>(image_width);
    const float draw_height = height > 0.0f ? height : static_cast<float>(image_height);
    if (has_shadow() && !state_.shadow.inset) {
      prepare_sampled_shadow();
      context_.draw_image_shadow(x, y, draw_width, draw_height, *found->second,
                                 state_.shadow.blur * shadow_effect_scale(),
                                 sampled_shadow_spread(), shadow_transform());
    }
    context_.set_fill_color(to_color(Color::White, state_.alpha));
    if (has_blur()) {
      context_.draw_image_blur(x, y, draw_width, draw_height, *found->second,
                               transformed_blur_radius(),
                               to_transform(state_.transform));
    } else {
      context_.draw_image(x, y, draw_width, draw_height, *found->second,
                          to_transform(state_.transform), true);
    }
    if (has_shadow() && state_.shadow.inset) {
      prepare_sampled_shadow();
      const Vec2 offset = transformed_shadow_offset();
      context_.draw_image_inset_shadow(
          x, y, draw_width, draw_height, *found->second, offset.x, offset.y,
          state_.shadow.blur * shadow_effect_scale(), sampled_shadow_spread(),
          to_transform(state_.transform));
    }
  }

  std::unique_ptr<ExternalOpenGLState> external_state_;
  std::unique_ptr<::gcanvas::Context> owned_context_;
  ::gcanvas::Context& context_;
  State state_;
  std::vector<State> state_stack_;
  std::unordered_map<std::string, ::gcanvas::Image*> images_;
  std::unordered_map<std::string, ::gcanvas::Image*> svg_images_;
  std::unordered_map<std::string, ::gcanvas::Font*> fonts_;
  float width_ = 0.0f;
  float height_ = 0.0f;
  bool frame_active_ = false;
};

} // namespace

std::unique_ptr<Renderer> create_renderer(::gcanvas::Context& context) {
  return std::make_unique<GCanvasRenderer>(context);
}

std::unique_ptr<Renderer> create_external_opengl_renderer(
    ::gcanvas::opengl::ProcLoader proc_loader, void* proc_loader_user_data) {
#if defined(FLEX_GCANVAS_HAS_OPENGL)
  if (proc_loader == nullptr) {
    throw std::invalid_argument("gCanvas external OpenGL renderer requires a proc loader");
  }
  auto state = std::make_unique<ExternalOpenGLState>();
  state->proc_loader = proc_loader;
  state->proc_loader_user_data = proc_loader_user_data;

  ::gcanvas::opengl::CreateInfo create_info;
  create_info.host.user_data = state.get();
  create_info.host.get_proc_address = load_external_gl_proc;
  create_info.host.framebuffer_size = external_framebuffer_size;
  create_info.presentation = ::gcanvas::opengl::PresentationMode::External;
  auto context = ::gcanvas::opengl::create_context(create_info);
  return std::make_unique<GCanvasRenderer>(std::move(state), std::move(context));
#else
  (void)proc_loader;
  (void)proc_loader_user_data;
  throw std::logic_error("Flex gCanvas engine was built without OpenGL support");
#endif
}

} // namespace flex::render::engines::gcanvas
