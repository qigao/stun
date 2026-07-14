#include "backends/d2d/init.h"
#include "backends/renderer.h"

#include <lunasvg/lunasvg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <d2d1helper.h>
#endif

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace flex {

#ifdef _WIN32
namespace {

float clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

D2D1_COLOR_F to_d2d_color(const Color& color, float alpha_scale = 1.0f) {
  return D2D1::ColorF(
      clamp01(color.r),
      clamp01(color.g),
      clamp01(color.b),
      clamp01(color.a * alpha_scale));
}

D2D1_MATRIX_3X2_F to_d2d_matrix(const Transform& transform) {
  return D2D1::Matrix3x2F(
      transform.data[0],
      transform.data[3],
      transform.data[1],
      transform.data[4],
      transform.data[2],
      transform.data[5]);
}

Vec2 transform_point(const Transform& transform, float x, float y) {
  return transform * Vec2{x, y};
}

D2D1_RECT_F transformed_rect(const Transform& transform, float x, float y, float w, float h) {
  Vec2 p1 = transform_point(transform, x, y);
  Vec2 p2 = transform_point(transform, x + w, y);
  Vec2 p3 = transform_point(transform, x, y + h);
  Vec2 p4 = transform_point(transform, x + w, y + h);

  return D2D1::RectF(
      std::min({p1.x, p2.x, p3.x, p4.x}),
      std::min({p1.y, p2.y, p3.y, p4.y}),
      std::max({p1.x, p2.x, p3.x, p4.x}),
      std::max({p1.y, p2.y, p3.y, p4.y}));
}

std::wstring to_wstring(const std::string& value) {
  if (value.empty()) {
    return {};
  }

  const int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (needed <= 1) {
    return {};
  }

  std::wstring result(static_cast<size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), needed);
  result.pop_back();
  return result;
}

void arc_to_center(float x1, float y1, float x2, float y2, float rx, float ry, float phi,
                   bool large_arc, bool sweep, float* out_cx, float* out_cy,
                   float* out_theta1, float* out_dtheta) {
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

void arc_segment_to_bezier(ID2D1GeometrySink* sink, float cx, float cy, float rx, float ry,
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

  sink->AddBezier(D2D1::BezierSegment(
      D2D1::Point2F(x1 + alpha * dx1, y1 + alpha * dy1),
      D2D1::Point2F(x2 - alpha * dx2, y2 - alpha * dy2),
      D2D1::Point2F(x2, y2)));
}

void draw_arc(ID2D1GeometrySink* sink, float x1, float y1, float x2, float y2, float rx, float ry,
              float x_axis_rotation, bool large_arc, bool sweep) {
  if (x1 == x2 && y1 == y2) {
    return;
  }
  if (rx == 0.0f || ry == 0.0f) {
    sink->AddLine(D2D1::Point2F(x2, y2));
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
    arc_segment_to_bezier(sink, cx, cy, rx, ry, cos_phi, sin_phi, current_theta, segment_dtheta);
    current_theta += segment_dtheta;
  }
}

Microsoft::WRL::ComPtr<ID2D1PathGeometry> build_path_geometry(const std::string& d) {
  d2d_backend::init();
  if (!d2d_backend::d2d_factory() || d.empty()) {
    return {};
  }

  Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
  if (FAILED(d2d_backend::d2d_factory()->CreatePathGeometry(geometry.GetAddressOf()))) {
    return {};
  }

  Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
  if (FAILED(geometry->Open(sink.GetAddressOf()))) {
    return {};
  }

  float cx = 0.0f;
  float cy = 0.0f;
  float sx = 0.0f;
  float sy = 0.0f;
  size_t i = 0;
  char cmd = 0;
  bool figure_open = false;

  auto close_figure = [&](D2D1_FIGURE_END end_mode) {
    if (figure_open) {
      sink->EndFigure(end_mode);
      figure_open = false;
    }
  };

  auto begin_figure = [&](float x, float y) {
    if (!figure_open) {
      sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
      figure_open = true;
    }
  };

  auto skip_ws = [&]() {
    while (i < d.size() &&
           (d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == ',')) {
      ++i;
    }
  };

  auto parse_num = [&]() -> float {
    skip_ws();
    const size_t start = i;
    if (i < d.size() && (d[i] == '-' || d[i] == '+')) {
      ++i;
    }
    while (i < d.size() && ((d[i] >= '0' && d[i] <= '9') || d[i] == '.')) {
      ++i;
    }
    if (i < d.size() && (d[i] == 'e' || d[i] == 'E')) {
      const size_t e_start = i;
      ++i;
      if (i < d.size() && (d[i] == '-' || d[i] == '+')) {
        ++i;
      }
      const size_t digit_start = i;
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

    if (std::isalpha(static_cast<unsigned char>(d[i]))) {
      cmd = d[i++];
    }
    if (!cmd) {
      break;
    }

    switch (cmd) {
      case 'M':
      case 'm': {
        const bool relative = (cmd == 'm');
        cx = parse_num();
        cy = parse_num();
        if (relative) {
          cx += sx;
          cy += sy;
        }
        close_figure(D2D1_FIGURE_END_OPEN);
        sink->BeginFigure(D2D1::Point2F(cx, cy), D2D1_FIGURE_BEGIN_FILLED);
        figure_open = true;
        sx = cx;
        sy = cy;
        cmd = relative ? 'l' : 'L';
        break;
      }
      case 'L':
      case 'l': {
        const bool relative = (cmd == 'l');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
          float x = parse_num();
          float y = parse_num();
          if (relative) {
            x += cx;
            y += cy;
          }
          begin_figure(cx, cy);
          sink->AddLine(D2D1::Point2F(x, y));
          cx = x;
          cy = y;
        }
        break;
      }
      case 'H':
      case 'h': {
        const bool relative = (cmd == 'h');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
          float x = parse_num();
          if (relative) {
            x += cx;
          }
          begin_figure(cx, cy);
          sink->AddLine(D2D1::Point2F(x, cy));
          cx = x;
        }
        break;
      }
      case 'V':
      case 'v': {
        const bool relative = (cmd == 'v');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
          float y = parse_num();
          if (relative) {
            y += cy;
          }
          begin_figure(cx, cy);
          sink->AddLine(D2D1::Point2F(cx, y));
          cy = y;
        }
        break;
      }
      case 'C':
      case 'c': {
        const bool relative = (cmd == 'c');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
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
          begin_figure(cx, cy);
          sink->AddBezier(D2D1::BezierSegment(
              D2D1::Point2F(x1, y1),
              D2D1::Point2F(x2, y2),
              D2D1::Point2F(x, y)));
          cx = x;
          cy = y;
        }
        break;
      }
      case 'Q':
      case 'q': {
        const bool relative = (cmd == 'q');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
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
          const float cp1x = cx + (2.0f / 3.0f) * (x1 - cx);
          const float cp1y = cy + (2.0f / 3.0f) * (y1 - cy);
          const float cp2x = x + (2.0f / 3.0f) * (x1 - x);
          const float cp2y = y + (2.0f / 3.0f) * (y1 - y);
          begin_figure(cx, cy);
          sink->AddBezier(D2D1::BezierSegment(
              D2D1::Point2F(cp1x, cp1y),
              D2D1::Point2F(cp2x, cp2y),
              D2D1::Point2F(x, y)));
          cx = x;
          cy = y;
        }
        break;
      }
      case 'A':
      case 'a': {
        const bool relative = (cmd == 'a');
        while (i < d.size()) {
          skip_ws();
          if (i >= d.size() || std::isalpha(static_cast<unsigned char>(d[i]))) {
            break;
          }
          const float rx = parse_num();
          const float ry = parse_num();
          const float rot = parse_num();
          const bool large = (parse_num() != 0.0f);
          const bool sweep = (parse_num() != 0.0f);
          float x = parse_num();
          float y = parse_num();
          if (relative) {
            x += cx;
            y += cy;
          }
          begin_figure(cx, cy);
          draw_arc(sink.Get(), cx, cy, x, y, rx, ry, rot, large, sweep);
          cx = x;
          cy = y;
        }
        break;
      }
      case 'Z':
      case 'z':
        begin_figure(cx, cy);
        sink->AddLine(D2D1::Point2F(sx, sy));
        close_figure(D2D1_FIGURE_END_CLOSED);
        cx = sx;
        cy = sy;
        break;
      default:
        ++i;
        break;
    }
  }

  close_figure(D2D1_FIGURE_END_OPEN);
  sink->Close();
  return geometry;
}

class D2DRenderer final : public Renderer {
 public:
  explicit D2DRenderer(ID2D1RenderTarget* render_target) : render_target_(render_target) {
    state_stack_.reserve(32);
    d2d_backend::init();
  }

  bool register_font(const std::string& family,
                     const std::string& path) override {
    return d2d_backend::load_font(family.c_str(), path.c_str());
  }

  void unregister_font(const std::string& family) override {
    d2d_backend::unload_font(family.c_str());
  }

  void begin_frame(float width, float height, float pixel_ratio) override {
    width_ = width;
    height_ = height;
    pixel_ratio_ = pixel_ratio;
    global_alpha_ = 1.0f;
    current_transform_ = Transform{};
    shadow_ = Shadow{};
    blur_ = BlurFilter{};
    pop_all_clips();
    state_stack_.clear();

    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    render_target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    render_target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  }

  void end_frame() override {
    pop_all_clips();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    last_end_draw_hr_ = render_target_->EndDraw();
  }

  bool requires_surface_recreation() const override {
    return last_end_draw_hr_ == D2DERR_RECREATE_TARGET;
  }

  void acknowledge_surface_recreation() override {
    last_end_draw_hr_ = S_OK;
  }

  void set_retained_mode(bool enabled) override { retained_mode_ = enabled; }

  void save() override {
    state_stack_.push_back({current_transform_, global_alpha_, current_clip_depth_});
  }

  void restore() override {
    if (state_stack_.empty()) {
      return;
    }
    SavedState saved = state_stack_.back();
    state_stack_.pop_back();
    pop_to_clip_depth(saved.clip_depth);
    current_transform_ = saved.transform;
    global_alpha_ = saved.alpha;
  }

  void reset() override {
    pop_all_clips();
    state_stack_.clear();
    current_transform_ = Transform{};
    global_alpha_ = 1.0f;
    shadow_ = Shadow{};
    blur_ = BlurFilter{};
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
  }

  void set_transform(const Transform& transform) override { current_transform_ = transform; }

  void translate(float x, float y) override {
    current_transform_.data[2] += x;
    current_transform_.data[5] += y;
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
  }

  void scale(float sx, float sy) override {
    current_transform_.data[0] *= sx;
    current_transform_.data[1] *= sy;
    current_transform_.data[3] *= sx;
    current_transform_.data[4] *= sy;
  }

  void clip_rect(float x, float y, float w, float h) override {
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    render_target_->PushAxisAlignedClip(
        transformed_rect(current_transform_, x, y, w, h),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++current_clip_depth_;
  }

  void reset_clip() override {
    if (current_clip_depth_ == 0) {
      return;
    }
    render_target_->PopAxisAlignedClip();
    --current_clip_depth_;
  }

  void set_global_alpha(float alpha) override {
    const float base_alpha = state_stack_.empty() ? 1.0f : state_stack_.back().alpha;
    global_alpha_ = base_alpha * alpha;
  }

  void set_shadow(const Shadow& shadow) override { shadow_ = shadow; }
  void clear_shadow() override { shadow_ = Shadow{}; }
  void set_blur(const BlurFilter& blur) override { blur_ = blur; }
  void clear_blur() override { blur_ = BlurFilter{}; }

  void fill_path(const std::string& d, const Paint& paint) override {
    auto geometry = build_path_geometry(d);
    if (!geometry) {
      return;
    }

    D2D1_RECT_F bounds{};
    geometry->GetBounds(nullptr, &bounds);
    auto brush = create_brush(paint, bounds);
    if (!brush) {
      return;
    }

    apply_transform();
    render_target_->FillGeometry(geometry.Get(), brush.Get());
  }

  void stroke_path(const std::string& d, const Paint& paint, float width) override {
    auto geometry = build_path_geometry(d);
    if (!geometry) {
      return;
    }

    D2D1_RECT_F bounds{};
    geometry->GetBounds(nullptr, &bounds);
    auto brush = create_brush(paint, bounds);
    if (!brush) {
      return;
    }

    apply_transform();
    render_target_->DrawGeometry(geometry.Get(), brush.Get(), width);
  }

  void draw_line(float x1, float y1, float x2, float y2, const Paint& paint, float width) override {
    auto brush = create_brush(
        paint,
        D2D1::RectF(std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2)));
    if (!brush) {
      return;
    }

    apply_transform();
    render_target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush.Get(), width);
  }

  void draw_rect(float x, float y, float w, float h, float r,
                 const Paint& fill, const Paint& stroke, float stroke_width) override {
    apply_transform();
    const D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, r, r);

    if (fill.type != Paint::Type::None) {
      auto fill_brush = create_brush(fill, rect);
      if (fill_brush) {
        if (r > 0.0f) {
          render_target_->FillRoundedRectangle(rounded, fill_brush.Get());
        } else {
          render_target_->FillRectangle(rect, fill_brush.Get());
        }
      }
    }

    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      auto stroke_brush = create_brush(stroke, rect);
      if (stroke_brush) {
        if (r > 0.0f) {
          render_target_->DrawRoundedRectangle(rounded, stroke_brush.Get(), stroke_width);
        } else {
          render_target_->DrawRectangle(rect, stroke_brush.Get(), stroke_width);
        }
      }
    }
  }

  void draw_circle(float cx, float cy, float r,
                   const Paint& fill, const Paint& stroke, float stroke_width) override {
    draw_ellipse(cx, cy, r, r, fill, stroke, stroke_width);
  }

  void draw_ellipse(float cx, float cy, float rx, float ry,
                    const Paint& fill, const Paint& stroke, float stroke_width) override {
    apply_transform();
    const D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry);
    const D2D1_RECT_F bounds = D2D1::RectF(cx - rx, cy - ry, cx + rx, cy + ry);

    if (fill.type != Paint::Type::None) {
      auto fill_brush = create_brush(fill, bounds);
      if (fill_brush) {
        render_target_->FillEllipse(ellipse, fill_brush.Get());
      }
    }

    if (stroke.type != Paint::Type::None && stroke_width > 0.0f) {
      auto stroke_brush = create_brush(stroke, bounds);
      if (stroke_brush) {
        render_target_->DrawEllipse(ellipse, stroke_brush.Get(), stroke_width);
      }
    }
  }

  void draw_text(const std::string& text, float x, float y,
                 const std::string& font_family, float font_size,
                 bool bold, const Color& color) override {
    if (!d2d_backend::dwrite_factory() || text.empty()) {
      return;
    }

    const std::wstring family = to_wstring(resolve_font_family(font_family));
    const std::wstring text_w = to_wstring(text);
    if (family.empty() || text_w.empty()) {
      return;
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    if (FAILED(d2d_backend::dwrite_factory()->CreateTextFormat(
            family.c_str(),
            nullptr,
            bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            font_size > 0.0f ? font_size : 14.0f,
            L"",
            format.GetAddressOf()))) {
      return;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(render_target_->CreateSolidColorBrush(
            to_d2d_color(color, global_alpha_), brush.GetAddressOf()))) {
      return;
    }

    apply_transform();
    const float layout_w = std::max(width_, x + font_size * static_cast<float>(text.size()) * 1.5f);
    const float layout_h = std::max(height_, y + font_size * 2.0f);
    render_target_->DrawText(
        text_w.c_str(),
        static_cast<UINT32>(text_w.size()),
        format.Get(),
        D2D1::RectF(x, y, layout_w, layout_h),
        brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
  }

  void draw_image(const std::string& src, float x, float y, float width, float height) override {
    auto bitmap = ensure_image_bitmap(src);
    if (!bitmap) {
      return;
    }

    apply_transform();
    render_target_->DrawBitmap(
        bitmap.Get(),
        D2D1::RectF(x, y, x + width, y + height),
        global_alpha_,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
  }

  void draw_svg(const std::string& src, float x, float y, float width, float height) override {
    auto document = lunasvg::Document::loadFromFile(src);
    if (!document) {
      return;
    }

    auto bitmap = ensure_svg_bitmap(svg_file_cache_key(src, width, height), *document, width, height);
    if (!bitmap.bitmap) {
      return;
    }

    apply_transform();
    render_target_->DrawBitmap(
        bitmap.bitmap.Get(),
        D2D1::RectF(x, y, x + width, y + height),
        global_alpha_,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
  }

  void draw_svg_data(const std::string& data, float x, float y, float width, float height) override {
    auto document = lunasvg::Document::loadFromData(data);
    if (!document) {
      return;
    }

    auto bitmap = ensure_svg_bitmap(svg_data_cache_key(data, width, height), *document, width, height);
    if (!bitmap.bitmap) {
      return;
    }

    apply_transform();
    render_target_->DrawBitmap(
        bitmap.bitmap.Get(),
        D2D1::RectF(x, y, x + width, y + height),
        global_alpha_,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
  }

  void clear(const Color& color) override {
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    const D2D1_COLOR_F d2d_color = to_d2d_color(color);
    render_target_->Clear(&d2d_color);
  }

  Bounds viewport() const override { return Bounds{0.0f, 0.0f, width_, height_}; }
  RendererCapabilities capabilities() const override {
    RendererCapabilities caps;
    caps.retained_mode = false;
    caps.surface_recreation = true;
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
  void remove_cached(PaintHandle) override {}
  PaintHandle push_rect(float, float, float, float, float, const Paint&, const Paint&, float,
                        const Transform&, float) override { return nullptr; }
  PaintHandle push_circle(float, float, float, const Paint&, const Paint&, float,
                          const Transform&, float) override { return nullptr; }
  PaintHandle push_ellipse(float, float, float, float, const Paint&, const Paint&, float,
                           const Transform&, float) override { return nullptr; }
  PaintHandle push_polygon(int, float, const Paint&, const Paint&, float,
                           const Transform&, float) override { return nullptr; }
  PaintHandle push_star(int, float, float, const Paint&, const Paint&, float,
                        const Transform&, float) override { return nullptr; }
  PaintHandle push_path(const std::string&, const Paint&, const Paint&, float,
                        const Transform&, float) override { return nullptr; }
  void update_transform(PaintHandle, const Transform&) override {}

 private:
  struct SavedState {
    Transform transform{};
    float alpha = 1.0f;
    std::size_t clip_depth = 0;
  };

  struct CachedBitmap {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    int width = 0;
    int height = 0;
  };

  void apply_transform() {
    render_target_->SetTransform(to_d2d_matrix(current_transform_));
  }

  void pop_to_clip_depth(std::size_t clip_depth) {
    while (current_clip_depth_ > clip_depth) {
      render_target_->PopAxisAlignedClip();
      --current_clip_depth_;
    }
  }

  void pop_all_clips() {
    pop_to_clip_depth(0);
  }

  Microsoft::WRL::ComPtr<ID2D1Brush> create_brush(const Paint& paint, const D2D1_RECT_F& bounds) {
    switch (paint.type) {
      case Paint::Type::None:
        return {};
      case Paint::Type::Solid: {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(render_target_->CreateSolidColorBrush(
                to_d2d_color(paint.color, global_alpha_), brush.GetAddressOf()))) {
          return {};
        }
        Microsoft::WRL::ComPtr<ID2D1Brush> base;
        brush.As(&base);
        return base;
      }
      case Paint::Type::Linear:
        return create_linear_gradient_brush(paint.linear, bounds);
      case Paint::Type::Radial:
        return create_radial_gradient_brush(paint.radial, bounds);
    }
    return {};
  }

  Microsoft::WRL::ComPtr<ID2D1Brush> create_linear_gradient_brush(const LinearGradient& gradient,
                                                                  const D2D1_RECT_F& bounds) {
    std::vector<D2D1_GRADIENT_STOP> stops;
    if (!gradient.stops.empty()) {
      stops.reserve(gradient.stops.size());
      for (const auto& stop : gradient.stops) {
        stops.push_back({std::clamp(stop.offset, 0.0f, 1.0f), to_d2d_color(stop.color, global_alpha_)});
      }
    } else {
      stops.push_back({0.0f, to_d2d_color(Color{1, 1, 1, 1}, global_alpha_)});
      stops.push_back({1.0f, to_d2d_color(Color{0, 0, 0, 1}, global_alpha_)});
    }

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(render_target_->CreateGradientStopCollection(
            stops.data(), static_cast<UINT32>(stops.size()), collection.GetAddressOf()))) {
      return {};
    }

    const float bw = bounds.right - bounds.left;
    const float bh = bounds.bottom - bounds.top;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> brush;
    if (FAILED(render_target_->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(bounds.left + gradient.x1 * bw, bounds.top + gradient.y1 * bh),
                D2D1::Point2F(bounds.left + gradient.x2 * bw, bounds.top + gradient.y2 * bh)),
            collection.Get(),
            brush.GetAddressOf()))) {
      return {};
    }

    Microsoft::WRL::ComPtr<ID2D1Brush> base;
    brush.As(&base);
    return base;
  }

  Microsoft::WRL::ComPtr<ID2D1Brush> create_radial_gradient_brush(const RadialGradient& gradient,
                                                                  const D2D1_RECT_F& bounds) {
    std::vector<D2D1_GRADIENT_STOP> stops;
    if (!gradient.stops.empty()) {
      stops.reserve(gradient.stops.size());
      for (const auto& stop : gradient.stops) {
        stops.push_back({std::clamp(stop.offset, 0.0f, 1.0f), to_d2d_color(stop.color, global_alpha_)});
      }
    } else {
      stops.push_back({0.0f, to_d2d_color(Color{1, 1, 1, 1}, global_alpha_)});
      stops.push_back({1.0f, to_d2d_color(Color{0, 0, 0, 1}, global_alpha_)});
    }

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(render_target_->CreateGradientStopCollection(
            stops.data(), static_cast<UINT32>(stops.size()), collection.GetAddressOf()))) {
      return {};
    }

    const float bw = bounds.right - bounds.left;
    const float bh = bounds.bottom - bounds.top;
    const float radius = std::max(bw, bh) * std::max(gradient.radius, 0.0f);
    Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
    if (FAILED(render_target_->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(
                D2D1::Point2F(bounds.left + gradient.cx * bw, bounds.top + gradient.cy * bh),
                D2D1::Point2F((gradient.fx - gradient.cx) * bw, (gradient.fy - gradient.cy) * bh),
                radius,
                radius),
            collection.Get(),
            brush.GetAddressOf()))) {
      return {};
    }

    Microsoft::WRL::ComPtr<ID2D1Brush> base;
    brush.As(&base);
    return base;
  }

  Microsoft::WRL::ComPtr<ID2D1Bitmap> ensure_image_bitmap(const std::string& src) {
    auto it = image_cache_.find(src);
    if (it != image_cache_.end()) {
      return it->second;
    }
    if (!d2d_backend::wic_factory()) {
      return {};
    }

    const std::wstring path = to_wstring(src);
    if (path.empty()) {
      return {};
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(d2d_backend::wic_factory()->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()))) {
      return {};
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) {
      return {};
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(d2d_backend::wic_factory()->CreateFormatConverter(converter.GetAddressOf()))) {
      return {};
    }

    if (FAILED(converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
            nullptr, 0.0f, WICBitmapPaletteTypeMedianCut))) {
      return {};
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    if (FAILED(render_target_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap.GetAddressOf()))) {
      return {};
    }

    image_cache_.emplace(src, bitmap);
    return bitmap;
  }

  CachedBitmap ensure_svg_bitmap(const std::string& key, const lunasvg::Document& document,
                                 float width, float height) {
    auto it = svg_cache_.find(key);
    if (it != svg_cache_.end()) {
      return it->second;
    }

    const int target_width = width > 0.0f ? std::max(1, static_cast<int>(std::lround(width))) : -1;
    const int target_height =
        height > 0.0f ? std::max(1, static_cast<int>(std::lround(height))) : -1;

    lunasvg::Bitmap bitmap = document.renderToBitmap(target_width, target_height, 0x00000000);
    if (bitmap.isNull() || bitmap.width() <= 0 || bitmap.height() <= 0) {
      return {};
    }

    bitmap.convertToRGBA();
    std::vector<std::uint8_t> premultiplied(static_cast<size_t>(bitmap.width()) * bitmap.height() * 4);
    const std::uint8_t* src = bitmap.data();
    for (size_t index = 0; index < premultiplied.size(); index += 4) {
      const std::uint8_t alpha = src[index + 3];
      premultiplied[index + 0] =
          static_cast<std::uint8_t>((static_cast<unsigned int>(src[index + 0]) * alpha) / 255);
      premultiplied[index + 1] =
          static_cast<std::uint8_t>((static_cast<unsigned int>(src[index + 1]) * alpha) / 255);
      premultiplied[index + 2] =
          static_cast<std::uint8_t>((static_cast<unsigned int>(src[index + 2]) * alpha) / 255);
      premultiplied[index + 3] = alpha;
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> d2d_bitmap;
    if (FAILED(render_target_->CreateBitmap(
            D2D1::SizeU(bitmap.width(), bitmap.height()),
            premultiplied.data(),
            static_cast<UINT32>(bitmap.width() * 4),
            D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            d2d_bitmap.GetAddressOf()))) {
      return {};
    }

    CachedBitmap cached{d2d_bitmap, bitmap.width(), bitmap.height()};
    svg_cache_.emplace(key, cached);
    return cached;
  }

  static std::string svg_file_cache_key(const std::string& src, float width, float height) {
    return "file:" + src + "|" + std::to_string(width) + "|" + std::to_string(height);
  }

  static std::string svg_data_cache_key(const std::string& data, float width, float height) {
    return "data:" + std::to_string(std::hash<std::string>{}(data)) + "|" + std::to_string(data.size()) +
           "|" + std::to_string(width) + "|" + std::to_string(height);
  }

  std::string resolve_font_family(const std::string& requested) const {
    if (!requested.empty()) {
      std::lock_guard<std::mutex> lock(d2d_backend::font_registry_mutex());
      auto& registry = d2d_backend::font_registry();
      auto it = registry.find(requested);
      if (it != registry.end() && !it->second.empty()) {
        return it->second;
      }
      return requested;
    }

    {
      std::lock_guard<std::mutex> lock(d2d_backend::font_registry_mutex());
      auto& registry = d2d_backend::font_registry();
      auto it = registry.find("sans-serif");
      if (it != registry.end() && !it->second.empty()) {
        return it->second;
      }
    }

    return "Segoe UI";
  }

  ID2D1RenderTarget* render_target_ = nullptr;
  HRESULT last_end_draw_hr_ = S_OK;
  float width_ = 0.0f;
  float height_ = 0.0f;
  float pixel_ratio_ = 1.0f;
  float global_alpha_ = 1.0f;
  Transform current_transform_{};
  Shadow shadow_{};
  BlurFilter blur_{};
  std::size_t current_clip_depth_ = 0;
  std::vector<SavedState> state_stack_;
  std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID2D1Bitmap>> image_cache_;
  std::unordered_map<std::string, CachedBitmap> svg_cache_;
};

} // namespace

std::unique_ptr<Renderer> create_d2d_renderer(CanvasHandle canvas) {
  auto* render_target = static_cast<ID2D1RenderTarget*>(canvas);
  if (!render_target) {
    return nullptr;
  }
  return std::make_unique<D2DRenderer>(render_target);
}

#else

std::unique_ptr<Renderer> create_d2d_renderer(CanvasHandle) {
  return nullptr;
}

#endif

} // namespace flex
