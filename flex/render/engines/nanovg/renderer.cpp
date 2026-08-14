/*
 * Flex Engine - OpenGL Renderer Implementation
 *
 * Immediate-mode engine backed by an internally owned NanoVG context.
 * Retained mode, shadows/blur and SVG rasterization are intentionally
 * downgraded in this first implementation.
 */

#include "backends/opengl/init.h"
#include "flex/render/engines/nanovg.h"
#include "flex/bridge/renderer.h"

#include <gcanvas/svg.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <nanovg.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

namespace flex {
namespace {

float clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

NVGcolor to_nvg_color(const Color &color) {
  return nvgRGBAf(clamp01(color.r), clamp01(color.g), clamp01(color.b), clamp01(color.a));
}

Color gradient_start_color(const std::vector<ColorStop> &stops) {
  if (stops.empty()) {
    return Color{};
  }
  auto it = std::min_element(stops.begin(), stops.end(),
                             [](const ColorStop &a, const ColorStop &b) { return a.offset < b.offset; });
  return it->color;
}

Color gradient_end_color(const std::vector<ColorStop> &stops) {
  if (stops.empty()) {
    return Color{};
  }
  auto it = std::max_element(stops.begin(), stops.end(),
                             [](const ColorStop &a, const ColorStop &b) { return a.offset < b.offset; });
  return it->color;
}

// Convert SVG arc parameters to center parameterization.
static void arc_to_center(float x1, float y1, float x2, float y2, float rx, float ry, float phi,
                          bool large_arc, bool sweep, float *out_cx, float *out_cy,
                          float *out_theta1, float *out_dtheta) {
  if (rx == 0 || ry == 0) {
    *out_cx = x1;
    *out_cy = y1;
    *out_theta1 = 0;
    *out_dtheta = 0;
    return;
  }

  rx = std::abs(rx);
  ry = std::abs(ry);

  const float cos_phi = std::cos(phi);
  const float sin_phi = std::sin(phi);

  const float dx = (x1 - x2) / 2.0f;
  const float dy = (y1 - y2) / 2.0f;
  const float x1p = cos_phi * dx + sin_phi * dy;
  const float y1p = -sin_phi * dx + cos_phi * dy;

  const float x1p2 = x1p * x1p;
  const float y1p2 = y1p * y1p;
  float rx2 = rx * rx;
  float ry2 = ry * ry;

  const float lambda = x1p2 / rx2 + y1p2 / ry2;
  if (lambda > 1.0f) {
    const float scale = std::sqrt(lambda);
    rx *= scale;
    ry *= scale;
    rx2 = rx * rx;
    ry2 = ry * ry;
  }

  const float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
  const float denom = rx2 * y1p2 + ry2 * x1p2;

  float sq = 0.0f;
  if (denom > 0.0f && num > 0.0f) {
    sq = std::sqrt(num / denom);
  }
  if (large_arc == sweep) {
    sq = -sq;
  }

  const float cxp = sq * rx * y1p / ry;
  const float cyp = -sq * ry * x1p / rx;

  const float cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0f;
  const float cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0f;

  auto angle = [](float ux, float uy, float vx, float vy) -> float {
    const float dot = ux * vx + uy * vy;
    const float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    float ang = (len > 0.0f) ? std::acos(std::clamp(dot / len, -1.0f, 1.0f)) : 0.0f;
    if (ux * vy - uy * vx < 0.0f) {
      ang = -ang;
    }
    return ang;
  };

  const float theta1 = angle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
  float dtheta =
      angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);

  if (!sweep && dtheta > 0.0f) {
    dtheta -= 2.0f * static_cast<float>(M_PI);
  }
  if (sweep && dtheta < 0.0f) {
    dtheta += 2.0f * static_cast<float>(M_PI);
  }

  *out_cx = cx;
  *out_cy = cy;
  *out_theta1 = theta1;
  *out_dtheta = dtheta;
}

static void arc_segment_to_bezier(NVGcontext *vg, float cx, float cy, float rx, float ry,
                                  float cos_phi, float sin_phi, float theta1, float dtheta) {
  const float t = std::tan(dtheta / 4.0f);
  const float alpha = std::sin(dtheta) * (std::sqrt(4.0f + 3.0f * t * t) - 1.0f) / 3.0f;

  const float cos_t1 = std::cos(theta1);
  const float sin_t1 = std::sin(theta1);
  const float cos_t2 = std::cos(theta1 + dtheta);
  const float sin_t2 = std::sin(theta1 + dtheta);

  const float x1 = cx + rx * cos_phi * cos_t1 - ry * sin_phi * sin_t1;
  const float y1 = cy + rx * sin_phi * cos_t1 + ry * cos_phi * sin_t1;
  const float x2 = cx + rx * cos_phi * cos_t2 - ry * sin_phi * sin_t2;
  const float y2 = cy + rx * sin_phi * cos_t2 + ry * cos_phi * sin_t2;

  const float dx1 = -rx * cos_phi * sin_t1 - ry * sin_phi * cos_t1;
  const float dy1 = -rx * sin_phi * sin_t1 + ry * cos_phi * cos_t1;
  const float dx2 = -rx * cos_phi * sin_t2 - ry * sin_phi * cos_t2;
  const float dy2 = -rx * sin_phi * sin_t2 + ry * cos_phi * cos_t2;

  const float cp1x = x1 + alpha * dx1;
  const float cp1y = y1 + alpha * dy1;
  const float cp2x = x2 - alpha * dx2;
  const float cp2y = y2 - alpha * dy2;

  nvgBezierTo(vg, cp1x, cp1y, cp2x, cp2y, x2, y2);
}

static void draw_arc(NVGcontext *vg, float x1, float y1, float x2, float y2, float rx, float ry,
                     float x_axis_rotation, bool large_arc, bool sweep) {
  if (x1 == x2 && y1 == y2) {
    return;
  }
  if (rx == 0.0f || ry == 0.0f) {
    nvgLineTo(vg, x2, y2);
    return;
  }

  const float phi = x_axis_rotation * static_cast<float>(M_PI) / 180.0f;
  const float cos_phi = std::cos(phi);
  const float sin_phi = std::sin(phi);

  float cx = 0.0f;
  float cy = 0.0f;
  float theta1 = 0.0f;
  float dtheta = 0.0f;
  arc_to_center(x1, y1, x2, y2, rx, ry, phi, large_arc, sweep, &cx, &cy, &theta1, &dtheta);

  rx = std::abs(rx);
  ry = std::abs(ry);

  int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (static_cast<float>(M_PI) / 2.0f)));
  if (segments < 1) {
    segments = 1;
  }

  const float segment_dtheta = dtheta / static_cast<float>(segments);
  float current_theta = theta1;

  for (int i = 0; i < segments; ++i) {
    arc_segment_to_bezier(vg, cx, cy, rx, ry, cos_phi, sin_phi, current_theta, segment_dtheta);
    current_theta += segment_dtheta;
  }
}

static void parse_svg_path(NVGcontext *vg, const std::string &d) {
  if (d.empty()) {
    return;
  }

  float cx = 0.0f;
  float cy = 0.0f;
  float sx = 0.0f;
  float sy = 0.0f;
  size_t i = 0;
  char cmd = 0;

  auto skip_ws = [&]() {
    while (i < d.size() &&
           (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ',')) {
      ++i;
    }
  };

  auto parse_num = [&]() -> float {
    skip_ws();
    size_t start = i;
    if (i < d.size() && (d[i] == '-' || d[i] == '+')) {
      ++i;
    }
    while (i < d.size() && ((d[i] >= '0' && d[i] <= '9') || d[i] == '.')) {
      ++i;
    }
    if (i < d.size() && (d[i] == 'e' || d[i] == 'E')) {
      size_t e_start = i;
      ++i;
      if (i < d.size() && (d[i] == '-' || d[i] == '+')) {
        ++i;
      }
      size_t digit_start = i;
      while (i < d.size() && (d[i] >= '0' && d[i] <= '9')) {
        ++i;
      }
      if (i == digit_start) {
        i = e_start;
      }
    }
    if (start == i) {
      return 0.0f;
    }
    return std::stof(d.substr(start, i - start));
  };

  while (i < d.size()) {
    skip_ws();
    if (i >= d.size()) {
      break;
    }

    const char c = d[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      cmd = c;
      ++i;
    }

    const bool relative = (cmd >= 'a' && cmd <= 'z');
    const char ucmd = relative ? static_cast<char>(cmd - 32) : cmd;

    switch (ucmd) {
    case 'M': {
      float x = parse_num();
      float y = parse_num();
      if (relative) {
        x += cx;
        y += cy;
      }
      nvgMoveTo(vg, x, y);
      cx = sx = x;
      cy = sy = y;
      cmd = relative ? 'l' : 'L';
      break;
    }
    case 'L': {
      float x = parse_num();
      float y = parse_num();
      if (relative) {
        x += cx;
        y += cy;
      }
      nvgLineTo(vg, x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'H': {
      float x = parse_num();
      if (relative) {
        x += cx;
      }
      nvgLineTo(vg, x, cy);
      cx = x;
      break;
    }
    case 'V': {
      float y = parse_num();
      if (relative) {
        y += cy;
      }
      nvgLineTo(vg, cx, y);
      cy = y;
      break;
    }
    case 'C': {
      float x1 = parse_num();
      float y1 = parse_num();
      float x2 = parse_num();
      float y2 = parse_num();
      float x = parse_num();
      float y = parse_num();
      if (relative) {
        x1 += cx;
        y1 += cy;
        x2 += cx;
        y2 += cy;
        x += cx;
        y += cy;
      }
      nvgBezierTo(vg, x1, y1, x2, y2, x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'Q': {
      float x1 = parse_num();
      float y1 = parse_num();
      float x = parse_num();
      float y = parse_num();
      if (relative) {
        x1 += cx;
        y1 += cy;
        x += cx;
        y += cy;
      }
      const float cx1 = cx + 2.0f / 3.0f * (x1 - cx);
      const float cy1 = cy + 2.0f / 3.0f * (y1 - cy);
      const float cx2 = x + 2.0f / 3.0f * (x1 - x);
      const float cy2 = y + 2.0f / 3.0f * (y1 - y);
      nvgBezierTo(vg, cx1, cy1, cx2, cy2, x, y);
      cx = x;
      cy = y;
      break;
    }
    case 'A': {
      const float rx = parse_num();
      const float ry = parse_num();
      const float rotation = parse_num();
      const float large_arc_flag = parse_num();
      const float sweep_flag = parse_num();
      float x = parse_num();
      float y = parse_num();
      if (relative) {
        x += cx;
        y += cy;
      }
      draw_arc(vg, cx, cy, x, y, rx, ry, rotation, large_arc_flag != 0.0f, sweep_flag != 0.0f);
      cx = x;
      cy = y;
      break;
    }
    case 'Z':
      nvgClosePath(vg);
      cx = sx;
      cy = sy;
      break;
    default:
      ++i;
      break;
    }
  }
}

class NanoVGRenderer final : public Renderer {
public:
  explicit NanoVGRenderer(NVGcontext *vg) : vg_(vg) {
    state_stack_.reserve(32);
  }

  ~NanoVGRenderer() override {
    if (!vg_) {
      return;
    }
    for (const auto &entry : image_cache_) {
      if (entry.second > 0) {
        nvgDeleteImage(vg_, entry.second);
      }
    }
    for (const auto &entry : svg_cache_) {
      if (entry.second.handle > 0) {
        nvgDeleteImage(vg_, entry.second.handle);
      }
    }
    nvgDeleteGL3(vg_);
  }

  bool register_font(const std::string& family,
                     const std::string& path) override {
    return opengl_backend::load_font(family.c_str(), path.c_str());
  }

  void unregister_font(const std::string& family) override {
    opengl_backend::unload_font(family.c_str());
  }

  void begin_frame(float width, float height, float pixel_ratio) override {
    width_ = width;
    height_ = height;
    pixel_ratio_ = pixel_ratio;
    global_alpha_ = 1.0f;
    current_transform_ = Transform{};
    current_clip_ = ClipRect{};
    state_stack_.clear();

    glViewport(0, 0,
               static_cast<GLsizei>(std::lround(width * pixel_ratio)),
               static_cast<GLsizei>(std::lround(height * pixel_ratio)));
    nvgBeginFrame(vg_, width, height, pixel_ratio);
    nvgGlobalAlpha(vg_, global_alpha_);
  }

  void end_frame() override {
    nvgEndFrame(vg_);
  }

  void set_retained_mode(bool enabled) override {
    retained_mode_ = enabled;
  }

  void save() override {
    nvgSave(vg_);
    state_stack_.push_back({current_transform_, global_alpha_, current_clip_});
  }

  void restore() override {
    if (state_stack_.empty()) {
      return;
    }
    nvgRestore(vg_);
    current_transform_ = state_stack_.back().transform;
    global_alpha_ = state_stack_.back().alpha;
    current_clip_ = state_stack_.back().clip;
    state_stack_.pop_back();
  }

  void reset() override {
    nvgReset(vg_);
    global_alpha_ = 1.0f;
    current_transform_ = Transform{};
    current_clip_ = ClipRect{};
    nvgGlobalAlpha(vg_, global_alpha_);
  }

  void set_transform(const Transform &transform) override {
    current_transform_ = transform;
    nvgResetTransform(vg_);
    nvgTransform(vg_, transform.data[0], transform.data[3], transform.data[1], transform.data[4],
                 transform.data[2], transform.data[5]);
  }

  void translate(float x, float y) override {
    current_transform_.data[2] += x;
    current_transform_.data[5] += y;
    nvgTranslate(vg_, x, y);
  }

  void rotate(float degrees) override {
    if (degrees == 0.0f) {
      return;
    }
    const float rad = degrees * (3.14159265f / 180.0f);
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float m00 = current_transform_.data[0];
    const float m01 = current_transform_.data[1];
    const float m10 = current_transform_.data[3];
    const float m11 = current_transform_.data[4];
    current_transform_.data[0] = m00 * c + m01 * s;
    current_transform_.data[1] = -m00 * s + m01 * c;
    current_transform_.data[3] = m10 * c + m11 * s;
    current_transform_.data[4] = -m10 * s + m11 * c;
    nvgRotate(vg_, rad);
  }

  void scale(float sx, float sy) override {
    current_transform_.data[0] *= sx;
    current_transform_.data[1] *= sy;
    current_transform_.data[3] *= sx;
    current_transform_.data[4] *= sy;
    nvgScale(vg_, sx, sy);
  }

  void clip_rect(float x, float y, float w, float h) override {
    if (current_clip_.active) {
      nvgIntersectScissor(vg_, x, y, w, h);
    } else {
      nvgScissor(vg_, x, y, w, h);
    }
    current_clip_ = {x, y, w, h, true};
  }

  void reset_clip() override {
    current_clip_ = ClipRect{};
    nvgResetScissor(vg_);
  }

  void set_global_alpha(float alpha) override {
    const float base_alpha = state_stack_.empty() ? 1.0f : state_stack_.back().alpha;
    global_alpha_ = clamp01(base_alpha * alpha);
    nvgGlobalAlpha(vg_, global_alpha_);
  }

  void set_shadow(const Shadow &shadow) override {
    (void)shadow;
  }

  void clear_shadow() override {}

  void set_blur(const BlurFilter &blur) override {
    (void)blur;
  }

  void clear_blur() override {}

  void fill_path(const std::string &d, const Paint &paint) override {
    if (paint.type == Paint::Type::None) {
      return;
    }
    nvgBeginPath(vg_);
    parse_svg_path(vg_, d);
    apply_fill(paint);
    nvgFill(vg_);
  }

  void stroke_path(const std::string &d, const Paint &paint, float width) override {
    if (paint.type == Paint::Type::None || width <= 0.0f) {
      return;
    }
    nvgBeginPath(vg_);
    parse_svg_path(vg_, d);
    apply_stroke(paint, width);
    nvgStroke(vg_);
  }

  void draw_line(float x1, float y1, float x2, float y2, const Paint &paint, float width) override {
    if (paint.type == Paint::Type::None || width <= 0.0f) {
      return;
    }
    nvgBeginPath(vg_);
    nvgMoveTo(vg_, x1, y1);
    nvgLineTo(vg_, x2, y2);
    apply_stroke(paint, width);
    nvgStroke(vg_);
  }

  void draw_rect(float x, float y, float w, float h, float r, const Paint &fill, const Paint &stroke,
                 float stroke_width) override {
    nvgBeginPath(vg_);
    if (r > 0.0f) {
      nvgRoundedRect(vg_, x, y, w, h, r);
    } else {
      nvgRect(vg_, x, y, w, h);
    }
    if (fill.type != Paint::Type::None) {
      apply_fill(fill);
      nvgFill(vg_);
    }
    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      nvgBeginPath(vg_);
      if (r > 0.0f) {
        nvgRoundedRect(vg_, x, y, w, h, r);
      } else {
        nvgRect(vg_, x, y, w, h);
      }
      apply_stroke(stroke, stroke_width);
      nvgStroke(vg_);
    }
  }

  void draw_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                   float stroke_width) override {
    nvgBeginPath(vg_);
    nvgCircle(vg_, cx, cy, r);
    if (fill.type != Paint::Type::None) {
      apply_fill(fill);
      nvgFill(vg_);
    }
    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      nvgBeginPath(vg_);
      nvgCircle(vg_, cx, cy, r);
      apply_stroke(stroke, stroke_width);
      nvgStroke(vg_);
    }
  }

  void draw_ellipse(float cx, float cy, float rx, float ry, const Paint &fill, const Paint &stroke,
                    float stroke_width) override {
    nvgBeginPath(vg_);
    nvgEllipse(vg_, cx, cy, rx, ry);
    if (fill.type != Paint::Type::None) {
      apply_fill(fill);
      nvgFill(vg_);
    }
    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      nvgBeginPath(vg_);
      nvgEllipse(vg_, cx, cy, rx, ry);
      apply_stroke(stroke, stroke_width);
      nvgStroke(vg_);
    }
  }

  void draw_text(const std::string &text, float x, float y, const std::string &font_family,
                 float font_size, bool bold, const Color &color) override {
    if (text.empty()) {
      return;
    }
    const std::string font_name = resolve_font_name(font_family, bold);
    if (font_name.empty()) {
      return;
    }

    nvgFontSize(vg_, font_size);
    nvgFontFace(vg_, font_name.c_str());
    nvgTextAlign(vg_, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg_, to_nvg_color(color));
    nvgText(vg_, x, y, text.c_str(), nullptr);
  }

  void draw_image(const std::string &src, float x, float y, float width, float height) override {
    const int image = ensure_image(src);
    if (image <= 0) {
      return;
    }

    int image_w = 0;
    int image_h = 0;
    nvgImageSize(vg_, image, &image_w, &image_h);
    if (image_w <= 0 || image_h <= 0) {
      return;
    }

    if (width <= 0.0f) {
      width = static_cast<float>(image_w);
    }
    if (height <= 0.0f) {
      height = static_cast<float>(image_h);
    }

    nvgBeginPath(vg_);
    nvgRect(vg_, x, y, width, height);
    nvgFillPaint(vg_, nvgImagePattern(vg_, x, y, width, height, 0.0f, image, 1.0f));
    nvgFill(vg_);
  }

  void draw_svg(const std::string &src, float x, float y, float width, float height) override {
    auto document = ::gcanvas::SvgDocument::load_file(src);
    if (!document) {
      return;
    }

    const RasterImage image =
        ensure_svg_image(svg_file_cache_key(src, width, height), *document, width, height);
    draw_cached_raster_image(image, x, y, width, height);
  }

  void draw_svg_data(const std::string &data, float x, float y, float width, float height) override {
    auto document = ::gcanvas::SvgDocument::load_data(data);
    if (!document) {
      return;
    }

    const RasterImage image =
        ensure_svg_image(svg_data_cache_key(data, width, height), *document, width, height);
    draw_cached_raster_image(image, x, y, width, height);
  }

  void clear(const Color &color) override {
    glClearColor(clamp01(color.r), clamp01(color.g), clamp01(color.b), clamp01(color.a));
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
    caps.shadow = false;
    caps.blur = false;
    return caps;
  }

  bool supports_retained_mode() const override {
    return false;
  }

  void remove_cached(PaintHandle paint) override {
    (void)paint;
  }

  PaintHandle push_rect(float x, float y, float w, float h, float r, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)r;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  PaintHandle push_circle(float cx, float cy, float r, const Paint &fill, const Paint &stroke,
                          float stroke_width, const Transform &transform, float alpha) override {
    (void)cx;
    (void)cy;
    (void)r;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  PaintHandle push_ellipse(float cx, float cy, float rx, float ry, const Paint &fill,
                           const Paint &stroke, float stroke_width, const Transform &transform,
                           float alpha) override {
    (void)cx;
    (void)cy;
    (void)rx;
    (void)ry;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  PaintHandle push_polygon(int sides, float radius, const Paint &fill, const Paint &stroke,
                           float stroke_width, const Transform &transform, float alpha) override {
    (void)sides;
    (void)radius;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  PaintHandle push_star(int points, float outer_radius, float inner_radius, const Paint &fill,
                        const Paint &stroke, float stroke_width, const Transform &transform,
                        float alpha) override {
    (void)points;
    (void)outer_radius;
    (void)inner_radius;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  PaintHandle push_path(const std::string &d, const Paint &fill, const Paint &stroke,
                        float stroke_width, const Transform &transform, float alpha) override {
    (void)d;
    (void)fill;
    (void)stroke;
    (void)stroke_width;
    (void)transform;
    (void)alpha;
    return nullptr;
  }

  void update_transform(PaintHandle paint, const Transform &transform) override {
    (void)paint;
    (void)transform;
  }

private:
  struct ClipRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool active = false;
  };

  struct SavedState {
    Transform transform;
    float alpha = 1.0f;
    ClipRect clip;
  };

  struct RasterImage {
    int handle = 0;
    int width = 0;
    int height = 0;
  };

  void apply_fill(const Paint &paint) {
    switch (paint.type) {
    case Paint::Type::Solid:
      nvgFillColor(vg_, to_nvg_color(paint.color));
      break;
    case Paint::Type::Linear: {
      const Color start = gradient_start_color(paint.linear.stops);
      const Color end = gradient_end_color(paint.linear.stops);
      nvgFillPaint(vg_,
                   nvgLinearGradient(vg_, paint.linear.x1 * 100.0f, paint.linear.y1 * 100.0f,
                                     paint.linear.x2 * 100.0f, paint.linear.y2 * 100.0f,
                                     to_nvg_color(start), to_nvg_color(end)));
      break;
    }
    case Paint::Type::Radial: {
      const Color start = gradient_start_color(paint.radial.stops);
      const Color end = gradient_end_color(paint.radial.stops);
      nvgFillPaint(vg_,
                   nvgRadialGradient(vg_, paint.radial.cx * 100.0f, paint.radial.cy * 100.0f, 0.0f,
                                     paint.radial.radius * 100.0f, to_nvg_color(start),
                                     to_nvg_color(end)));
      break;
    }
    case Paint::Type::None:
      break;
    }
  }

  void apply_stroke(const Paint &paint, float width) {
    nvgStrokeWidth(vg_, width);
    switch (paint.type) {
    case Paint::Type::Solid:
      nvgStrokeColor(vg_, to_nvg_color(paint.color));
      break;
    case Paint::Type::Linear: {
      const Color start = gradient_start_color(paint.linear.stops);
      const Color end = gradient_end_color(paint.linear.stops);
      nvgStrokePaint(vg_,
                     nvgLinearGradient(vg_, paint.linear.x1 * 100.0f, paint.linear.y1 * 100.0f,
                                       paint.linear.x2 * 100.0f, paint.linear.y2 * 100.0f,
                                       to_nvg_color(start), to_nvg_color(end)));
      break;
    }
    case Paint::Type::Radial: {
      const Color start = gradient_start_color(paint.radial.stops);
      const Color end = gradient_end_color(paint.radial.stops);
      nvgStrokePaint(vg_, nvgRadialGradient(vg_, paint.radial.cx * 100.0f,
                                            paint.radial.cy * 100.0f, 0.0f,
                                            paint.radial.radius * 100.0f, to_nvg_color(start),
                                            to_nvg_color(end)));
      break;
    }
    case Paint::Type::None:
      break;
    }
  }

  void draw_cached_raster_image(const RasterImage &image, float x, float y, float width, float height) {
    if (image.handle <= 0 || image.width <= 0 || image.height <= 0) {
      return;
    }

    if (width <= 0.0f) {
      width = static_cast<float>(image.width);
    }
    if (height <= 0.0f) {
      height = static_cast<float>(image.height);
    }

    nvgBeginPath(vg_);
    nvgRect(vg_, x, y, width, height);
    nvgFillPaint(vg_, nvgImagePattern(vg_, x, y, width, height, 0.0f, image.handle, 1.0f));
    nvgFill(vg_);
  }

  int ensure_image(const std::string &src) {
    auto it = image_cache_.find(src);
    if (it != image_cache_.end()) {
      return it->second;
    }

    const int image = nvgCreateImage(vg_, src.c_str(), 0);
    if (image > 0) {
      image_cache_.emplace(src, image);
    }
    return image;
  }

  RasterImage ensure_svg_image(const std::string &key, const ::gcanvas::SvgDocument &document, float width,
                               float height) {
    auto it = svg_cache_.find(key);
    if (it != svg_cache_.end()) {
      return it->second;
    }

    const int target_width = width > 0.0f ? std::max(1, static_cast<int>(std::lround(width))) : -1;
    const int target_height =
        height > 0.0f ? std::max(1, static_cast<int>(std::lround(height))) : -1;

    ::gcanvas::SvgBitmap bitmap = document.rasterize(target_width, target_height);
    if (bitmap.empty()) {
      return {};
    }

    const int handle =
        nvgCreateImageRGBA(vg_, bitmap.width, bitmap.height, 0, bitmap.rgba.data());
    if (handle <= 0) {
      return {};
    }

    RasterImage image{handle, bitmap.width, bitmap.height};
    svg_cache_.emplace(key, image);
    return image;
  }

  static std::string svg_file_cache_key(const std::string &src, float width, float height) {
    return "file:" + src + "|" + std::to_string(width) + "|" + std::to_string(height);
  }

  static std::string svg_data_cache_key(const std::string &data, float width, float height) {
    return "data:" + std::to_string(std::hash<std::string>{}(data)) + "|" + std::to_string(data.size()) +
           "|" + std::to_string(width) + "|" + std::to_string(height);
  }

  std::string resolve_font_name(const std::string &requested, bool bold) {
    if (bold) {
      const std::string bold_name = requested + "-bold";
      if (!requested.empty() && ensure_font_loaded(bold_name)) {
        return bold_name;
      }
    }

    if (!requested.empty() && ensure_font_loaded(requested)) {
      return requested;
    }

    if (ensure_font_loaded("sans-serif")) {
      return "sans-serif";
    }

    std::string fallback;
    {
      std::lock_guard<std::mutex> lock(opengl_backend::font_registry_mutex());
      auto &registry = opengl_backend::font_registry();
      if (!registry.empty()) {
        fallback = registry.begin()->first;
      }
    }
    if (!fallback.empty()) {
      if (ensure_font_loaded(fallback)) {
        return fallback;
      }
    }

    return {};
  }

  bool ensure_font_loaded(const std::string &name) {
    if (name.empty()) {
      return false;
    }
    if (nvgFindFont(vg_, name.c_str()) >= 0) {
      return true;
    }

    std::string path;
    {
      std::lock_guard<std::mutex> lock(opengl_backend::font_registry_mutex());
      auto &registry = opengl_backend::font_registry();
      auto it = registry.find(name);
      if (it == registry.end()) {
        return false;
      }
      path = it->second;
    }

    if (path.empty()) {
      return false;
    }

    return nvgCreateFont(vg_, name.c_str(), path.c_str()) >= 0;
  }

  NVGcontext *vg_ = nullptr;
  float width_ = 0.0f;
  float height_ = 0.0f;
  float pixel_ratio_ = 1.0f;
  float global_alpha_ = 1.0f;
  Transform current_transform_{};
  ClipRect current_clip_{};
  std::vector<SavedState> state_stack_;
  std::unordered_map<std::string, int> image_cache_;
  std::unordered_map<std::string, RasterImage> svg_cache_;
};

} // namespace

std::unique_ptr<Renderer> render::engines::nanovg::create_renderer(
    bool antialias, bool stencil_strokes, bool debug) {
  int flags = 0;
  if (antialias) {
    flags |= NVG_ANTIALIAS;
  }
  if (stencil_strokes) {
    flags |= NVG_STENCIL_STROKES;
  }
  if (debug) {
    flags |= NVG_DEBUG;
  }

  NVGcontext *vg = nvgCreateGL3(flags);
  if (!vg) {
    return nullptr;
  }
  try {
    return std::make_unique<NanoVGRenderer>(vg);
  } catch (...) {
    nvgDeleteGL3(vg);
    throw;
  }
}

} // namespace flex
