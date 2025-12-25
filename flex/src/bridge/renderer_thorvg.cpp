/*
 * Flex Engine - ThorVG Renderer Implementation
 *
 * Simplified implementation using unified Paint API.
 * All geometry is rendered via SVG path strings.
 */

#include "flex/bridge/renderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stb_sprintf.h>
#include <thorvg.h>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace flex {

// ============================================================================
// SVG Arc to Bezier Conversion
// ============================================================================

// Convert SVG arc parameters to center parameterization
// Returns: cx, cy, theta1, dtheta
static void arc_to_center(float x1, float y1, float x2, float y2, float rx, float ry, float phi,
                          bool large_arc, bool sweep, float *out_cx, float *out_cy,
                          float *out_theta1, float *out_dtheta) {
  // Handle degenerate cases
  if (rx == 0 || ry == 0) {
    *out_cx = x1;
    *out_cy = y1;
    *out_theta1 = 0;
    *out_dtheta = 0;
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
  if (large_arc == sweep)
    sq = -sq;

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
    if (ux * vy - uy * vx < 0)
      ang = -ang;
    return ang;
  };

  float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
  float dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);

  // Adjust dtheta based on sweep flag
  if (!sweep && dtheta > 0)
    dtheta -= 2 * static_cast<float>(M_PI);
  if (sweep && dtheta < 0)
    dtheta += 2 * static_cast<float>(M_PI);

  *out_cx = cx;
  *out_cy = cy;
  *out_theta1 = theta1;
  *out_dtheta = dtheta;
}

// Approximate an arc segment (max 90 degrees) with a cubic bezier
static void arc_segment_to_bezier(tvg::Shape *shape, float cx, float cy, float rx, float ry,
                                  float cos_phi, float sin_phi, float theta1, float dtheta) {
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
static void draw_arc(tvg::Shape *shape, float x1, float y1, // Start point (current point)
                     float x2, float y2,                    // End point
                     float rx, float ry,                    // Radii
                     float x_axis_rotation,                 // In degrees
                     bool large_arc, bool sweep) {
  // Handle degenerate cases
  if (x1 == x2 && y1 == y2)
    return;
  if (rx == 0 || ry == 0) {
    shape->lineTo(x2, y2);
    return;
  }

  float phi = x_axis_rotation * static_cast<float>(M_PI) / 180.0f;
  float cos_phi = std::cos(phi);
  float sin_phi = std::sin(phi);

  // Get center parameterization
  float cx, cy, theta1, dtheta;
  arc_to_center(x1, y1, x2, y2, rx, ry, phi, large_arc, sweep, &cx, &cy, &theta1, &dtheta);

  // Correct radii (may have been adjusted)
  rx = std::abs(rx);
  ry = std::abs(ry);

  // Split into segments of max 90 degrees
  int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (M_PI / 2.0f)));
  if (segments < 1)
    segments = 1;

  float segment_dtheta = dtheta / segments;
  float current_theta = theta1;

  for (int i = 0; i < segments; ++i) {
    arc_segment_to_bezier(shape, cx, cy, rx, ry, cos_phi, sin_phi, current_theta, segment_dtheta);
    current_theta += segment_dtheta;
  }
}

// ============================================================================
// SVG Path Parser
// ============================================================================

static void parse_svg_path(tvg::Shape *shape, const std::string &d) {
  if (d.empty())
    return;

  float cx = 0, cy = 0; // Current point
  float sx = 0, sy = 0; // Start of subpath (for Z command)
  size_t i = 0;
  char cmd = 0;

  auto skip_ws = [&]() {
    while (i < d.size() &&
           (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ','))
      ++i;
  };

  auto parse_num = [&]() -> float {
    skip_ws();
    size_t start = i;
    if (i < d.size() && (d[i] == '-' || d[i] == '+'))
      ++i;
    while (i < d.size() && ((d[i] >= '0' && d[i] <= '9') || d[i] == '.'))
      ++i;
    if (start == i)
      return 0;
    return std::stof(d.substr(start, i - start));
  };

  while (i < d.size()) {
    skip_ws();
    if (i >= d.size())
      break;

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
      if (relative) {
        x += cx;
        y += cy;
      }
      shape->moveTo(x, y);
      cx = sx = x;
      cy = sy = y;
      cmd = relative ? 'l' : 'L';
      break;
    }
    case 'L': {
      float x = parse_num(), y = parse_num();
      if (relative) {
        x += cx;
        y += cy;
      }
      shape->lineTo(x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'H': {
      float x = parse_num();
      if (relative)
        x += cx;
      shape->lineTo(x, cy);
      cx = x;
      break;
    }
    case 'V': {
      float y = parse_num();
      if (relative)
        y += cy;
      shape->lineTo(cx, y);
      cy = y;
      break;
    }
    case 'C': {
      float x1 = parse_num(), y1 = parse_num();
      float x2 = parse_num(), y2 = parse_num();
      float x = parse_num(), y = parse_num();
      if (relative) {
        x1 += cx;
        y1 += cy;
        x2 += cx;
        y2 += cy;
        x += cx;
        y += cy;
      }
      shape->cubicTo(x1, y1, x2, y2, x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'Q': {
      float x1 = parse_num(), y1 = parse_num();
      float x = parse_num(), y = parse_num();
      if (relative) {
        x1 += cx;
        y1 += cy;
        x += cx;
        y += cy;
      }
      float cx1 = cx + 2.0f / 3.0f * (x1 - cx);
      float cy1 = cy + 2.0f / 3.0f * (y1 - cy);
      float cx2 = x + 2.0f / 3.0f * (x1 - x);
      float cy2 = y + 2.0f / 3.0f * (y1 - y);
      shape->cubicTo(cx1, cy1, cx2, cy2, x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'A': {
      float rx = parse_num(), ry = parse_num();
      float rotation = parse_num();
      float large_arc_flag = parse_num();
      float sweep_flag = parse_num();
      float x = parse_num(), y = parse_num();
      if (relative) {
        x += cx;
        y += cy;
      }

      draw_arc(shape, cx, cy, x, y, rx, ry, rotation, large_arc_flag != 0, sweep_flag != 0);
      cx = x;
      cy = y;
      break;
    }
    case 'Z': {
      shape->close();
      cx = sx;
      cy = sy;
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
  explicit ThorVGRenderer(tvg::Canvas *canvas) : canvas_(canvas) { state_stack_.reserve(32); }

  // Retained mode control
  void set_retained_mode(bool enabled) override {
    retained_mode_ = enabled;
  }

  // Frame Control
  void begin_frame(float width, float height, float pixel_ratio) override {
    frame_start_ = std::chrono::high_resolution_clock::now();

    // Only clear canvas in immediate mode
    if (!retained_mode_) {
      canvas_->remove();
      background_shape_ = nullptr;
    }

    remove_time_ = std::chrono::high_resolution_clock::now();

    width_ = width;
    height_ = height;
    pixel_ratio_ = pixel_ratio;
    global_alpha_ = 1.0f;
    current_transform_ = Transform::Identity();
    current_shadow_ = Shadow{};
    current_blur_ = BlurFilter{};
    current_clip_ = ClipRect{};
    state_stack_.clear();

    push_count_ = 0;
    update_count_ = 0;
    clip_count_ = 0;
    path_parse_count_ = 0;
  }

  void end_frame() override {
    auto push_done = std::chrono::high_resolution_clock::now();

    // Always use draw(true) - ThorVG partial rendering requires dirty region tracking
    // which we don't implement yet. Full redraw is more predictable.
    canvas_->draw(true);
    auto draw_done = std::chrono::high_resolution_clock::now();

    canvas_->sync();
    auto sync_done = std::chrono::high_resolution_clock::now();

    // frame_count_++;
    // if (frame_count_ % 100 == 0) {
    //   double remove_ms = std::chrono::duration<double, std::milli>(remove_time_ -
    //   frame_start_).count(); double push_ms = std::chrono::duration<double, std::milli>(push_done
    //   - remove_time_).count(); double draw_ms = std::chrono::duration<double,
    //   std::milli>(draw_done - push_done).count(); double sync_ms = std::chrono::duration<double,
    //   std::milli>(sync_done - draw_done).count(); double total_ms = std::chrono::duration<double,
    //   std::milli>(sync_done - frame_start_).count();

    //   std::cout << "[Profile] Frame " << frame_count_
    //             << " | remove: " << remove_ms << "ms"
    //             << " | push(" << push_count_ << "): " << push_ms << "ms"
    //             << " | draw: " << draw_ms << "ms"
    //             << " | sync: " << sync_ms << "ms"
    //             << " | total: " << total_ms << "ms"
    //             << std::endl;
    // }
  }

  // Transform Stack
  void save() override {
    state_stack_.push_back(
        {current_transform_, global_alpha_, current_shadow_, current_blur_, current_clip_});
  }

  void restore() override {
    if (!state_stack_.empty()) {
      current_transform_ = state_stack_.back().transform;
      global_alpha_ = state_stack_.back().alpha;
      current_shadow_ = state_stack_.back().shadow;
      current_blur_ = state_stack_.back().blur;
      current_clip_ = state_stack_.back().clip;
      state_stack_.pop_back();
    }
  }

  void reset() override {
    current_transform_ = Transform::Identity();
    global_alpha_ = 1.0f;
    current_shadow_ = Shadow{};
    current_blur_ = BlurFilter{};
    current_clip_ = ClipRect{};
  }

  void set_transform(const Transform &transform) override { current_transform_ = transform; }

  void translate(float x, float y) override { current_transform_.translate(x, y); }

  void rotate(float degrees) override {
    if (degrees == 0.0f)
      return;
    float rad = degrees * (3.14159265f / 180.0f);
    current_transform_.rotate(rad);
  }

  void scale(float sx, float sy) override { current_transform_.scale(sx, sy); }

  // Clipping
  void clip_rect(float x, float y, float w, float h) override {
    current_clip_ = {x, y, w, h, true};
  }

  void reset_clip() override { current_clip_ = ClipRect{}; }

  // Opacity
  void set_global_alpha(float alpha) override {
    // Cumulative alpha: multiply new alpha by parent state's alpha
    float base_alpha = state_stack_.empty() ? 1.0f : state_stack_.back().alpha;
    global_alpha_ = base_alpha * alpha;
  }

  // Effects (Shadow and Blur)
  void set_shadow(const Shadow &shadow) override { current_shadow_ = shadow; }

  void clear_shadow() override { current_shadow_ = Shadow{}; }

  void set_blur(const BlurFilter &blur) override { current_blur_ = blur; }

  void clear_blur() override { current_blur_ = BlurFilter{}; }

  // Core Drawing
  void fill_path(const std::string &d, const Paint &paint) override {
    auto shape = tvg::Shape::gen();
    if (!try_fast_geometry(shape, d)) {
      path_parse_count_++;
      parse_svg_path(shape, d);
    }
    apply_paint_fill(shape, paint);
    apply_transform(shape);
    apply_clip(shape);
    push_to_canvas(shape);
  }

  void stroke_path(const std::string &d, const Paint &paint, float width) override {
    auto shape = tvg::Shape::gen();
    if (!try_fast_geometry(shape, d)) {
      path_parse_count_++;
      parse_svg_path(shape, d);
    }
    apply_paint_stroke(shape, paint, width);
    apply_transform(shape);
    apply_clip(shape);
    push_to_canvas(shape);
  }

  // Optimized primitive drawing
  void draw_rect(float x, float y, float w, float h, float r, const Paint &fill,
                 const Paint &stroke, float stroke_width) override {
    auto shape = tvg::Shape::gen();
    shape->appendRect(x, y, w, h, r, r);
    if (fill.type != Paint::Type::None)
      apply_paint_fill(shape, fill);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke(shape, stroke, stroke_width);
    apply_transform(shape);
    apply_clip(shape);
    push_to_canvas(shape);
  }

  void draw_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                   float stroke_width) override {
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, r, r);
    if (fill.type != Paint::Type::None)
      apply_paint_fill(shape, fill);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke(shape, stroke, stroke_width);
    apply_transform(shape);
    apply_clip(shape);
    push_to_canvas(shape);
  }

  void draw_ellipse(float cx, float cy, float rx, float ry, const Paint &fill, const Paint &stroke,
                    float stroke_width) override {
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, rx, ry);
    if (fill.type != Paint::Type::None)
      apply_paint_fill(shape, fill);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke(shape, stroke, stroke_width);
    apply_transform(shape);
    apply_clip(shape);
    push_to_canvas(shape);
  }

  // Text
  void draw_text(const std::string &text, float x, float y, const std::string &font_family,
                 float font_size, bool bold, const Color &color) override {
    auto tvg_text = tvg::Text::gen();
    tvg_text->font(font_family.c_str());
    tvg_text->size(font_size);
    tvg_text->text(text.c_str());
    tvg_text->transform(build_matrix_with_offset(x, y));
    tvg_text->fill(to_u8(color.r), to_u8(color.g), to_u8(color.b));
    tvg_text->opacity(static_cast<uint8_t>(color.a * global_alpha_ * 255));
    apply_clip(tvg_text);
    push_to_canvas(tvg_text);
    (void)bold;
  }

  // Image
  void draw_image(const std::string &src, float x, float y, float width, float height) override {
    auto picture = tvg::Picture::gen();
    if (picture->load(src.c_str()) != tvg::Result::Success)
      return;
    if (width > 0 && height > 0) {
      float pw, ph;
      picture->size(&pw, &ph);
      if (pw > 0 && ph > 0)
        picture->size(width, height);
    }
    picture->transform(build_matrix_with_offset(x, y));
    picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
    apply_clip(picture);
    push_to_canvas(picture);
  }

  // SVG
  void draw_svg(const std::string &src, float x, float y, float width, float height) override {
    auto picture = tvg::Picture::gen();
    if (picture->load(src.c_str()) != tvg::Result::Success)
      return;
    if (width > 0 && height > 0) {
      float pw, ph;
      picture->size(&pw, &ph);
      if (pw > 0 && ph > 0)
        picture->size(width, height);
    }
    picture->transform(build_matrix_with_offset(x, y));
    picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
    apply_clip(picture);
    push_to_canvas(picture);
  }

  void draw_svg_data(const std::string &data, float x, float y, float width,
                     float height) override {
    auto picture = tvg::Picture::gen();
    if (picture->load(data.c_str(), static_cast<uint32_t>(data.size()), "svg+xml", nullptr,
                      false) != tvg::Result::Success)
      return;
    if (width > 0 && height > 0) {
      float pw, ph;
      picture->size(&pw, &ph);
      if (pw > 0 && ph > 0)
        picture->size(width, height);
    }
    picture->transform(build_matrix_with_offset(x, y));
    picture->opacity(static_cast<uint8_t>(global_alpha_ * 255));
    apply_clip(picture);
    push_to_canvas(picture);
  }

  // Clear
  void clear(const Color &color) override {
    // In retained mode, skip clear - background is already drawn
    // ThorVG will redraw all objects on top of existing buffer
    if (retained_mode_ && background_shape_) {
      // Background already exists, no need to add again
      return;
    }

    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, width_, height_, 0, 0);
    shape->fill(to_u8(color.r), to_u8(color.g), to_u8(color.b), to_u8(color.a));

    if (retained_mode_) {
      background_shape_ = shape;
    }
    push_to_canvas(shape);
  }

  // Viewport Query (Phase 3.2: For culling optimization)
  Bounds viewport() const override { return Bounds{0, 0, width_, height_}; }

  // -------------------------------------------
  // Retained Mode API
  // -------------------------------------------

  bool supports_retained_mode() const override { return retained_mode_; }

  void remove_cached(PaintHandle paint) override {
    if (paint) {
      canvas_->remove(static_cast<tvg::Paint*>(paint));
    }
  }

  PaintHandle push_rect(float x, float y, float w, float h, float r, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    auto shape = tvg::Shape::gen();
    shape->appendRect(x, y, w, h, r, r);
    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  PaintHandle push_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                          float stroke_width, const Transform &transform, float alpha) override {
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, r, r);
    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  PaintHandle push_ellipse(float cx, float cy, float rx, float ry, const Paint &fill,
                           const Paint &stroke, float stroke_width, const Transform &transform,
                           float alpha) override {
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, rx, ry);
    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  PaintHandle push_polygon(int sides, float radius, const Paint &fill, const Paint &stroke,
                           float stroke_width, const Transform &transform, float alpha) override {
    if (sides < 3 || radius <= 0)
      return nullptr;

    auto shape = tvg::Shape::gen();

    // Build polygon directly using ThorVG API (no SVG path parsing!)
    const float pi = 3.14159265358979f;
    const float angle_step = 2.0f * pi / sides;
    // Flat-top orientation (top edge is horizontal)
    const float start_angle = -pi / 2.0f + angle_step / 2.0f;

    for (int i = 0; i < sides; ++i) {
      float angle = start_angle + i * angle_step;
      float x = radius * std::cos(angle);
      float y = radius * std::sin(angle);

      if (i == 0) {
        shape->moveTo(x, y);
      } else {
        shape->lineTo(x, y);
      }
    }
    shape->close();

    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  PaintHandle push_star(int points, float outer_radius, float inner_radius, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    if (points < 3 || outer_radius <= 0)
      return nullptr;
    if (inner_radius <= 0)
      inner_radius = outer_radius * 0.4f;

    auto shape = tvg::Shape::gen();

    // Build star directly using ThorVG API (no SVG path parsing!)
    const float pi = 3.14159265358979f;
    const float angle_step = pi / points; // Half step between outer and inner
    const float start_angle = -pi / 2.0f; // Start at top

    for (int i = 0; i < points * 2; ++i) {
      float angle = start_angle + i * angle_step;
      float r = (i % 2 == 0) ? outer_radius : inner_radius;
      float x = r * std::cos(angle);
      float y = r * std::sin(angle);

      if (i == 0) {
        shape->moveTo(x, y);
      } else {
        shape->lineTo(x, y);
      }
    }
    shape->close();

    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  PaintHandle push_path(const std::string &d, const Paint &fill, const Paint &stroke,
                        float stroke_width, const Transform &transform, float alpha) override {
    if (d.empty())
      return nullptr;

    auto shape = tvg::Shape::gen();
    parse_svg_path(shape, d);

    if (fill.type != Paint::Type::None)
      apply_paint_fill_with_alpha(shape, fill, alpha);
    if (stroke.type != Paint::Type::None)
      apply_paint_stroke_with_alpha(shape, stroke, stroke_width, alpha);
    apply_matrix(shape, transform);

    auto *raw_ptr = shape;
    push_count_++;
    canvas_->push(std::move(shape));
    return static_cast<PaintHandle>(raw_ptr);
  }

  void update_transform(PaintHandle paint, const Transform &transform) override {
    if (!paint)
      return;
    apply_matrix(static_cast<tvg::Paint*>(paint), transform);
    update_count_++;
  }

private:
  tvg::Canvas *canvas_;
  float width_ = 0, height_ = 0, pixel_ratio_ = 1.0f;
  float global_alpha_ = 1.0f;
  Transform current_transform_ = Transform::Identity();
  Shadow current_shadow_;
  BlurFilter current_blur_;
  tvg::Shape *background_shape_ = nullptr; // Cached background for retained mode

  struct ClipRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool active = false;
  };

  struct SavedState {
    Transform transform;
    float alpha = 1.0f;
    Shadow shadow;
    BlurFilter blur;
    ClipRect clip;
  };
  std::vector<SavedState> state_stack_;
  ClipRect current_clip_;

  // Profiling
  std::chrono::high_resolution_clock::time_point frame_start_;
  std::chrono::high_resolution_clock::time_point remove_time_;
  int frame_count_ = 0;
  int push_count_ = 0;
  int update_count_ = 0;
  int clip_count_ = 0;
  int path_parse_count_ = 0;
  bool retained_mode_ = false;

  static uint8_t to_u8(float f) { return static_cast<uint8_t>(f * 255.0f); }

  template <typename T> void push_to_canvas(T &paint) {
    push_count_++;
    canvas_->push(std::move(paint));
  }

  // Fast geometry detection - returns true if geometry was recognized and applied
  bool try_fast_geometry(tvg::Shape *shape, const std::string &d) {
    if (d.empty())
      return false;

    // Circle detection: "M r 0 A r r 0 1 1 -r 0 A r r 0 1 1 r 0 Z"
    // Pattern: starts with "M", contains exactly 2 arcs with same radii
    if (d[0] == 'M' && d.find('A') != std::string::npos) {
      float r = 0;
      int num_count = sscanf(d.c_str(), "M %g 0 A %*g %*g", &r);
      if (num_count == 1 && r > 0) {
        // Verify it's a full circle (two identical arcs)
        char buf[256];
        stbsp_snprintf(buf, sizeof(buf),
                       "M %.4g 0 A %.4g %.4g 0 1 1 %.4g 0 A %.4g %.4g 0 1 1 %.4g 0 Z", r, r, r, -r,
                       r, r, r);

        if (d == buf) {
          shape->appendCircle(0, 0, r, r);
          return true;
        }
      }
    }

    // Rectangle detection: "M 0 0 H w V h H 0 Z"
    if (d[0] == 'M' && d.find("M 0 0 H") == 0) {
      float w = 0, h = 0;
      int num_count = sscanf(d.c_str(), "M 0 0 H %g V %g H 0 Z", &w, &h);
      if (num_count == 2 && w > 0 && h > 0) {
        shape->appendRect(0, 0, w, h, 0, 0);
        return true;
      }
    }

    // Ellipse detection: "M rx 0 A rx ry 0 1 1 -rx 0 A rx ry 0 1 1 rx 0 Z"
    if (d[0] == 'M' && d.find('A') != std::string::npos) {
      float rx = 0, ry = 0;
      int num_count = sscanf(d.c_str(), "M %g 0 A %*g %g", &rx, &ry);
      if (num_count == 2 && rx > 0 && ry > 0 && rx != ry) {
        // Verify it's a full ellipse
        char buf[256];
        stbsp_snprintf(buf, sizeof(buf),
                       "M %.4g 0 A %.4g %.4g 0 1 1 %.4g 0 A %.4g %.4g 0 1 1 %.4g 0 Z", rx, rx, ry,
                       -rx, rx, ry, rx);

        if (d == buf) {
          shape->appendCircle(0, 0, rx, ry);
          return true;
        }
      }
    }

    // No fast path matched - use full parser
    return false;
  }

  // Convert flex::Transform to tvg::Matrix
  static tvg::Matrix to_tvg_matrix(const Transform& t) {
    tvg::Matrix m;
    m.e11 = t.m[0]; m.e12 = t.m[1]; m.e13 = t.m[2];
    m.e21 = t.m[3]; m.e22 = t.m[4]; m.e23 = t.m[5];
    m.e31 = 0.0f;   m.e32 = 0.0f;   m.e33 = 1.0f;
    return m;
  }

  tvg::Matrix build_matrix() {
    return to_tvg_matrix(current_transform_);
  }

  tvg::Matrix build_matrix_with_offset(float x, float y) {
    // Fast path: translation-only transform
    if (current_transform_.is_translation_only()) {
      tvg::Matrix m;
      m.e11 = 1.0f; m.e12 = 0.0f; m.e13 = x + current_transform_.tx();
      m.e21 = 0.0f; m.e22 = 1.0f; m.e23 = y + current_transform_.ty();
      m.e31 = 0.0f; m.e32 = 0.0f; m.e33 = 1.0f;
      return m;
    }
    // General case: transform the offset point
    Vec2 world_pos = current_transform_ * Vec2(x, y);
    tvg::Matrix m;
    m.e11 = current_transform_.m[0]; m.e12 = current_transform_.m[1]; m.e13 = world_pos.x();
    m.e21 = current_transform_.m[3]; m.e22 = current_transform_.m[4]; m.e23 = world_pos.y();
    m.e31 = 0.0f; m.e32 = 0.0f; m.e33 = 1.0f;
    return m;
  }

  void apply_transform(tvg::Shape *shape) {
    // Skip if identity - no transform needed
    if (!current_transform_.is_identity()) {
      shape->transform(build_matrix());
    }
  }

  void apply_clip(tvg::Paint *paint) {
    if (!current_clip_.active)
      return;
    clip_count_++;
    auto clipper = tvg::Shape::gen();
    clipper->appendRect(current_clip_.x, current_clip_.y, current_clip_.w, current_clip_.h, 0, 0);
    clipper->transform(build_matrix());
    paint->clip(std::move(clipper));
  }

  void apply_paint_fill(tvg::Shape *shape, const Paint &paint) {
    switch (paint.type) {
    case Paint::Type::Solid: {
      uint8_t a = to_u8(paint.color.a * global_alpha_);
      shape->fill(to_u8(paint.color.r), to_u8(paint.color.g), to_u8(paint.color.b), a);
      break;
    }
    case Paint::Type::Linear: {
      auto *fill = tvg::LinearGradient::gen();
      fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100, paint.linear.x2 * 100,
                   paint.linear.y2 * 100);
      apply_gradient_stops(fill, paint.linear.stops);
      shape->fill(fill);
      break;
    }
    case Paint::Type::Radial: {
      auto *fill = tvg::RadialGradient::gen();
      fill->radial(paint.radial.cx * 100, paint.radial.cy * 100, paint.radial.radius * 100,
                   paint.radial.cx * 100, paint.radial.cy * 100, 0);
      apply_gradient_stops(fill, paint.radial.stops);
      shape->fill(fill);
      break;
    }
    }
  }

  void apply_paint_stroke(tvg::Shape *shape, const Paint &paint, float width) {
    shape->strokeWidth(width);
    switch (paint.type) {
    case Paint::Type::Solid: {
      uint8_t a = to_u8(paint.color.a * global_alpha_);
      shape->strokeFill(to_u8(paint.color.r), to_u8(paint.color.g), to_u8(paint.color.b), a);
      break;
    }
    case Paint::Type::Linear: {
      auto *fill = tvg::LinearGradient::gen();
      fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100, paint.linear.x2 * 100,
                   paint.linear.y2 * 100);
      apply_gradient_stops(fill, paint.linear.stops);
      shape->strokeFill(fill);
      break;
    }
    case Paint::Type::Radial: {
      auto *fill = tvg::RadialGradient::gen();
      fill->radial(paint.radial.cx * 100, paint.radial.cy * 100, paint.radial.radius * 100,
                   paint.radial.cx * 100, paint.radial.cy * 100, 0);
      apply_gradient_stops(fill, paint.radial.stops);
      shape->strokeFill(fill);
      break;
    }
    }
  }

  template <typename GradientT>
  void apply_gradient_stops(GradientT *fill, const std::vector<ColorStop> &stops) {
    if (stops.empty())
      return;
    std::vector<tvg::Fill::ColorStop> tvg_stops;
    tvg_stops.reserve(stops.size());
    for (const auto &stop : stops) {
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

  // Retained mode helpers with explicit alpha
  void apply_paint_fill_with_alpha(tvg::Shape *shape, const Paint &paint, float alpha) {
    switch (paint.type) {
    case Paint::Type::Solid: {
      uint8_t a = to_u8(paint.color.a * alpha);
      shape->fill(to_u8(paint.color.r), to_u8(paint.color.g), to_u8(paint.color.b), a);
      break;
    }
    case Paint::Type::Linear: {
      auto *fill = tvg::LinearGradient::gen();
      fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100, paint.linear.x2 * 100,
                   paint.linear.y2 * 100);
      apply_gradient_stops_with_alpha(fill, paint.linear.stops, alpha);
      shape->fill(fill);
      break;
    }
    case Paint::Type::Radial: {
      auto *fill = tvg::RadialGradient::gen();
      fill->radial(paint.radial.cx * 100, paint.radial.cy * 100, paint.radial.radius * 100,
                   paint.radial.cx * 100, paint.radial.cy * 100, 0);
      apply_gradient_stops_with_alpha(fill, paint.radial.stops, alpha);
      shape->fill(fill);
      break;
    }
    default:
      break;
    }
  }

  void apply_paint_stroke_with_alpha(tvg::Shape *shape, const Paint &paint, float width,
                                     float alpha) {
    shape->strokeWidth(width);
    switch (paint.type) {
    case Paint::Type::Solid: {
      uint8_t a = to_u8(paint.color.a * alpha);
      shape->strokeFill(to_u8(paint.color.r), to_u8(paint.color.g), to_u8(paint.color.b), a);
      break;
    }
    case Paint::Type::Linear: {
      auto *fill = tvg::LinearGradient::gen();
      fill->linear(paint.linear.x1 * 100, paint.linear.y1 * 100, paint.linear.x2 * 100,
                   paint.linear.y2 * 100);
      apply_gradient_stops_with_alpha(fill, paint.linear.stops, alpha);
      shape->strokeFill(fill);
      break;
    }
    case Paint::Type::Radial: {
      auto *fill = tvg::RadialGradient::gen();
      fill->radial(paint.radial.cx * 100, paint.radial.cy * 100, paint.radial.radius * 100,
                   paint.radial.cx * 100, paint.radial.cy * 100, 0);
      apply_gradient_stops_with_alpha(fill, paint.radial.stops, alpha);
      shape->strokeFill(fill);
      break;
    }
    default:
      break;
    }
  }

  template <typename GradientT>
  void apply_gradient_stops_with_alpha(GradientT *fill, const std::vector<ColorStop> &stops,
                                       float alpha) {
    if (stops.empty())
      return;
    std::vector<tvg::Fill::ColorStop> tvg_stops;
    tvg_stops.reserve(stops.size());
    for (const auto &stop : stops) {
      tvg::Fill::ColorStop cs;
      cs.offset = stop.offset;
      cs.r = to_u8(stop.color.r);
      cs.g = to_u8(stop.color.g);
      cs.b = to_u8(stop.color.b);
      cs.a = to_u8(stop.color.a * alpha);
      tvg_stops.push_back(cs);
    }
    fill->colorStops(tvg_stops.data(), static_cast<uint32_t>(tvg_stops.size()));
  }

  void apply_matrix(tvg::Paint *paint, const Transform &transform) {
    // Skip if identity - no transform needed
    if (transform.is_identity()) return;
    paint->transform(to_tvg_matrix(transform));
  }
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Renderer> create_thorvg_renderer(CanvasHandle canvas) {
  // Cast opaque handle back to tvg::Canvas*
  return std::make_unique<ThorVGRenderer>(static_cast<tvg::Canvas*>(canvas));
}

} // namespace flex
