#include <nanogui/fluent_web_data_viz.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

namespace {

Color themed_color(const FluentWebTheme *theme, FluentWebTheme::ColorToken token,
                   const Color &fallback) {
  return theme ? theme->color(token) : fallback;
}

Color mix(const Color &a, const Color &b, float t) {
  return Color(a.r() + (b.r() - a.r()) * t, a.g() + (b.g() - a.g()) * t,
               a.b() + (b.b() - a.b()) * t, a.w() + (b.w() - a.w()) * t);
}

Color brand_or_default(const FluentWebTheme *theme) {
  if (!theme)
    return Color(0.25f, 0.47f, 0.85f, 1.f);
  return theme->color(FluentWebTheme::ColorToken::colorBrandBackground);
}

static const Color kFallbackPalette[] = {
    Color(0.25f, 0.47f, 0.85f, 1.f),
    Color(0.93f, 0.50f, 0.19f, 1.f),
    Color(0.56f, 0.74f, 0.26f, 1.f),
    Color(0.58f, 0.33f, 0.75f, 1.f),
    Color(0.93f, 0.25f, 0.30f, 1.f),
    Color(0.20f, 0.63f, 0.78f, 1.f)
};
constexpr size_t kFallbackCount =
    sizeof(kFallbackPalette) / sizeof(kFallbackPalette[0]);

} // namespace

/* ----------------------- FluentWebDataPalette ----------------------- */


Color FluentWebDataPalette::lighten(const Color &color, float amount) {
  amount = std::clamp(amount, 0.f, 1.f);
  return mix(color, Color(1.f, 1.f, 1.f, color.w()), amount);
}

Color FluentWebDataPalette::darken(const Color &color, float amount) {
  amount = std::clamp(amount, 0.f, 1.f);
  return mix(color, Color(0.f, 0.f, 0.f, color.w()), amount);
}

std::vector<Color> FluentWebDataPalette::categorical(const FluentWebTheme *theme,
                                                     size_t count) {
  if (count == 0)
    return {};

  std::vector<Color> palette;
  palette.reserve(count);

  Color brand = brand_or_default(theme);
  Color top = lighten(brand, 0.4f);
  Color bottom = darken(brand, 0.25f);

  for (size_t i = 0; i < count; ++i) {
    Color base = kFallbackPalette[i % kFallbackCount];
    float cycle = static_cast<float>(i / kFallbackCount);
    float blend = std::fmod(cycle * 0.35f, 1.f);
    Color tinted = mix(base, brand, blend);
    float shade = (i % 2 == 0) ? 0.15f : 0.3f;
    palette.push_back(mix(tinted, i % 3 == 0 ? top : bottom, shade));
  }

  return palette;
}

std::vector<Color> FluentWebDataPalette::sequential(const FluentWebTheme *theme,
                                                    size_t count) {
  if (count == 0)
    return {};

  std::vector<Color> palette;
  palette.reserve(count);

  Color brand = brand_or_default(theme);
  Color light = lighten(brand, 0.6f);
  Color dark = darken(brand, 0.3f);

  for (size_t i = 0; i < count; ++i) {
    float t = static_cast<float>(i) / std::max<size_t>(1, count - 1);
    palette.push_back(mix(light, dark, t));
  }

  return palette;
}

std::vector<Color> FluentWebDataPalette::diverging(const FluentWebTheme *theme, size_t count) {
  if (count == 0)
    return {};

  std::vector<Color> palette;
  palette.reserve(count);

  Color brand = brand_or_default(theme);
  Color opposite = Color(0.95f, 0.33f, 0.32f, 1.f);
  Color neutral = lighten(brand, 0.65f);

  size_t left = count / 2;
  for (size_t i = 0; i < left; ++i) {
    float t = static_cast<float>(i) / std::max<size_t>(1, left);
    palette.push_back(mix(opposite, neutral, 1.f - t * 0.85f));
  }

  if (count % 2 == 1)
    palette.push_back(neutral);

  size_t right = count / 2;
  for (size_t i = 0; i < right; ++i) {
    float t = static_cast<float>(i) / std::max<size_t>(1, right);
    palette.push_back(mix(brand, neutral, 1.f - t * 0.85f));
  }

  return palette;
}

/* --------------------------- FluentWebBarChart --------------------------- */

FluentWebBarChart::FluentWebBarChart(Widget *parent)
    : Widget(parent), m_max_value(1.f), m_show_grid(true) {
  refresh_tokens();
}

void FluentWebBarChart::set_values(const std::vector<float> &values) {
  m_values = values;
  m_max_value = 0.f;
  for (float v : m_values)
    m_max_value = std::max(m_max_value, v);
  ensure_palette();
  preferred_size_changed();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebBarChart::set_labels(const std::vector<std::string> &labels) {
  m_labels = labels;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebBarChart::clear() {
  m_values.clear();
  m_labels.clear();
  m_palette.clear();
  m_max_value = 1.f;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebBarChart::set_palette(const std::vector<Color> &palette) {
  m_palette = palette;
  ensure_palette();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebBarChart::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  ensure_palette();
}

Vector2i FluentWebBarChart::preferred_size_impl(NVGcontext *) const {
  return Vector2i(240, 160);
}

void FluentWebBarChart::draw(NVGcontext *ctx) {
  Widget::draw(ctx);
  if (m_values.empty())
    return;

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 8.f);
  nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w()));
  nvgFill(ctx);

  const float padding = 16.f;
  float chart_left = x + padding;
  float chart_bottom = y + h - padding;
  float chart_top = y + padding;
  float chart_width = w - padding * 2.f;
  float chart_height = h - padding * 2.f;

  if (m_show_grid && m_max_value > 0.f) {
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, nvgRGBAf(m_grid_color.r(), m_grid_color.g(), m_grid_color.b(), m_grid_color.w()));
    const int steps = 4;
    for (int i = 1; i <= steps; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(steps);
      float y_line = chart_bottom - t * chart_height;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, chart_left, y_line);
      nvgLineTo(ctx, chart_left + chart_width, y_line);
      nvgStroke(ctx);
    }
  }

  nvgStrokeWidth(ctx, 1.5f);
  nvgStrokeColor(ctx, nvgRGBAf(m_axis_color.r(), m_axis_color.g(), m_axis_color.b(), m_axis_color.w()));
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, chart_left, chart_top);
  nvgLineTo(ctx, chart_left, chart_bottom);
  nvgLineTo(ctx, chart_left + chart_width, chart_bottom);
  nvgStroke(ctx);

  size_t count = m_values.size();
  float bar_spacing = 12.f;
  float total_spacing = bar_spacing * static_cast<float>(count + 1);
  float bar_width = std::max(8.f, (chart_width - total_spacing) / std::max<size_t>(1, count));
  float cursor_x = chart_left + bar_spacing;

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBAf(m_label_color.r(), m_label_color.g(), m_label_color.b(), m_label_color.w()));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

  for (size_t i = 0; i < count; ++i) {
    float value = m_values[i];
    float ratio = (m_max_value <= 0.f) ? 0.f : (value / m_max_value);
    float bar_height = ratio * chart_height;
    float bar_x = cursor_x;
    float bar_y = chart_bottom - bar_height;

    const Color &bar_color = m_palette[i % m_palette.size()];
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, bar_x, bar_y, bar_width, bar_height, 4.f);
    nvgFillColor(ctx, nvgRGBAf(bar_color.r(), bar_color.g(), bar_color.b(), bar_color.w()));
    nvgFill(ctx);

    if (i < m_labels.size())
      nvgText(ctx, bar_x + bar_width * 0.5f, chart_bottom + 4.f, m_labels[i].c_str(), nullptr);

    cursor_x += bar_width + bar_spacing;
  }
}

void FluentWebBarChart::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_axis_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke1,
                              Color(0.65f, 0.65f, 0.65f, 1.f));
  m_grid_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                              Color(0.82f, 0.82f, 0.82f, 1.f));
  m_label_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                               Color(0.35f, 0.35f, 0.35f, 1.f));
}

void FluentWebBarChart::ensure_palette() {
  if (m_values.empty())
    return;
  if (m_palette.size() < m_values.size())
    m_palette = FluentWebDataPalette::categorical(dynamic_cast<FluentWebTheme *>(theme()),
                                                  std::max<size_t>(m_values.size(), 1));
}

/* --------------------------- FluentWebLineChart --------------------------- */

FluentWebLineChart::FluentWebLineChart(Widget *parent)
    : Widget(parent), m_min_value(0.f), m_max_value(1.f), m_show_markers(true) {
  refresh_tokens();
}

void FluentWebLineChart::set_points(const std::vector<float> &points) {
  m_points = points;
  if (m_points.empty()) {
    m_min_value = 0.f;
    m_max_value = 1.f;
  } else {
    auto [min_it, max_it] = std::minmax_element(m_points.begin(), m_points.end());
    m_min_value = *min_it;
    m_max_value = *max_it;
    if (m_min_value == m_max_value)
      m_max_value = m_min_value + 1.f;
  }
  ensure_palette();
  preferred_size_changed();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebLineChart::set_palette(const std::vector<Color> &palette) {
  m_palette = palette;
  ensure_palette();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebLineChart::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  ensure_palette();
}

Vector2i FluentWebLineChart::preferred_size_impl(NVGcontext *) const {
  return Vector2i(240, 160);
}

void FluentWebLineChart::draw(NVGcontext *ctx) {
  Widget::draw(ctx);
  if (m_points.empty())
    return;

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 8.f);
  nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w()));
  nvgFill(ctx);

  const float padding = 16.f;
  float chart_left = x + padding;
  float chart_bottom = y + h - padding;
  float chart_top = y + padding;
  float chart_width = w - padding * 2.f;
  float chart_height = h - padding * 2.f;

  nvgStrokeWidth(ctx, 1.f);
  nvgStrokeColor(ctx, nvgRGBAf(m_grid_color.r(), m_grid_color.g(), m_grid_color.b(), m_grid_color.w()));
  const int steps = 4;
  for (int i = 0; i <= steps; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(steps);
    float y_line = chart_bottom - t * chart_height;
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, chart_left, y_line);
    nvgLineTo(ctx, chart_left + chart_width, y_line);
    nvgStroke(ctx);
  }

  nvgStrokeWidth(ctx, 1.5f);
  nvgStrokeColor(ctx, nvgRGBAf(m_axis_color.r(), m_axis_color.g(), m_axis_color.b(), m_axis_color.w()));
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, chart_left, chart_top);
  nvgLineTo(ctx, chart_left, chart_bottom);
  nvgLineTo(ctx, chart_left + chart_width, chart_bottom);
  nvgStroke(ctx);

  ensure_palette();
  const Color series_color = m_palette.front();
  nvgStrokeWidth(ctx, 2.5f);
  nvgStrokeColor(ctx, nvgRGBAf(series_color.r(), series_color.g(), series_color.b(), series_color.w()));
  nvgLineJoin(ctx, NVG_ROUND);
  nvgLineCap(ctx, NVG_ROUND);

  float range = (m_max_value - m_min_value);
  if (range <= 0.f)
    range = 1.f;

  nvgBeginPath(ctx);
  for (size_t i = 0; i < m_points.size(); ++i) {
    float t = static_cast<float>(i) / std::max<size_t>(1, m_points.size() - 1);
    float px = chart_left + t * chart_width;
    float py = chart_bottom - ((m_points[i] - m_min_value) / range) * chart_height;
    if (i == 0)
      nvgMoveTo(ctx, px, py);
    else
      nvgLineTo(ctx, px, py);
  }
  nvgStroke(ctx);

  if (m_show_markers) {
    nvgFillColor(ctx, nvgRGBAf(series_color.r(), series_color.g(), series_color.b(), series_color.w()));
    for (size_t i = 0; i < m_points.size(); ++i) {
      float t = static_cast<float>(i) / std::max<size_t>(1, m_points.size() - 1);
      float px = chart_left + t * chart_width;
      float py = chart_bottom - ((m_points[i] - m_min_value) / range) * chart_height;
      nvgBeginPath(ctx);
      nvgCircle(ctx, px, py, 3.5f);
      nvgFill(ctx);
    }
  }
}

void FluentWebLineChart::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_axis_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke1,
                              Color(0.65f, 0.65f, 0.65f, 1.f));
  m_grid_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                              Color(0.82f, 0.82f, 0.82f, 1.f));
  m_label_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                               Color(0.35f, 0.35f, 0.35f, 1.f));
}

void FluentWebLineChart::ensure_palette() {
  if (m_palette.empty())
    m_palette = FluentWebDataPalette::sequential(dynamic_cast<FluentWebTheme *>(theme()), 6);
}

/* --------------------------- FluentWebPieChart --------------------------- */

FluentWebPieChart::FluentWebPieChart(Widget *parent)
    : Widget(parent), m_total(0.f), m_inner_radius_ratio(0.f), m_show_labels(true) {
  refresh_tokens();
}

void FluentWebPieChart::set_values(const std::vector<float> &values) {
  m_values = values;
  m_total = 0.f;
  for (float v : m_values)
    m_total += std::max(0.f, v);
  ensure_palette();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebPieChart::set_labels(const std::vector<std::string> &labels) {
  m_labels = labels;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebPieChart::set_palette(const std::vector<Color> &palette) {
  m_palette = palette;
  ensure_palette();
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebPieChart::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  ensure_palette();
}

Vector2i FluentWebPieChart::preferred_size_impl(NVGcontext *) const {
  return Vector2i(200, 200);
}

void FluentWebPieChart::draw(NVGcontext *ctx) {
  Widget::draw(ctx);
  if (m_values.empty() || m_total <= 0.f)
    return;

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 8.f);
  nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w()));
  nvgFill(ctx);

  float cx = x + w * 0.5f;
  float cy = y + h * 0.5f;
  float radius = std::min(w, h) * 0.5f - 8.f;
  float start_angle = -NVG_PI / 2.f;

  for (size_t i = 0; i < m_values.size(); ++i) {
    float value = std::max(0.f, m_values[i]);
    float angle = (value / m_total) * NVG_PI * 2.f;
    float end_angle = start_angle + angle;
    const Color &color = m_palette[i % m_palette.size()];

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, cx, cy);
    nvgArc(ctx, cx, cy, radius, start_angle, end_angle, NVG_CW);
    nvgClosePath(ctx);
    nvgFillColor(ctx, nvgRGBAf(color.r(), color.g(), color.b(), color.w()));
    nvgFill(ctx);

    if (m_inner_radius_ratio > 0.f) {
      float inner = radius * m_inner_radius_ratio;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, cx, cy);
      nvgArc(ctx, cx, cy, inner, start_angle, end_angle, NVG_CCW);
      nvgClosePath(ctx);
      nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w()));
      nvgFill(ctx);
    }

    if (m_show_labels && i < m_labels.size()) {
      float mid = start_angle + angle * 0.5f;
      float label_radius = radius * (m_inner_radius_ratio > 0.f ? (m_inner_radius_ratio + 1.f) * 0.5f
                                                                : 0.7f);
      float lx = cx + std::cos(mid) * label_radius;
      float ly = cy + std::sin(mid) * label_radius;
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, 12.f);
      nvgFillColor(ctx, nvgRGBAf(m_label_color.r(), m_label_color.g(), m_label_color.b(), m_label_color.w()));
      nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(ctx, lx, ly, m_labels[i].c_str(), nullptr);
    }

    start_angle = end_angle;
  }
}

void FluentWebPieChart::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_label_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                               Color(0.2f, 0.2f, 0.2f, 1.f));
}

void FluentWebPieChart::ensure_palette() {
  if (m_values.empty())
    return;
  if (m_palette.size() < m_values.size())
    m_palette = FluentWebDataPalette::categorical(dynamic_cast<FluentWebTheme *>(theme()),
                                                  std::max<size_t>(m_values.size(), 1));
}

NAMESPACE_END(nanogui)
