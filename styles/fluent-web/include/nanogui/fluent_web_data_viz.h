#pragma once

#include <nanogui/widget.h>
#include <nanogui/vector.h>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Helper producing Fluent-styled data visualization color palettes.
 */
class NANOGUI_EXPORT FluentWebDataPalette {
public:
  /// Returns a categorical palette sized to `count` entries.
  static std::vector<Color> categorical(const FluentWebTheme *theme, size_t count);

  /// Returns a sequential palette ranging from brand to neutral tones.
  static std::vector<Color> sequential(const FluentWebTheme *theme, size_t count);

  /// Returns a diverging palette centered around a neutral midpoint.
  static std::vector<Color> diverging(const FluentWebTheme *theme, size_t count);

private:
  static Color lighten(const Color &color, float amount);
  static Color darken(const Color &color, float amount);
};

/**
 * Simple Fluent-themed vertical bar chart.
 */
class NANOGUI_EXPORT FluentWebBarChart : public Widget {
public:
  explicit FluentWebBarChart(Widget *parent);

  void set_values(const std::vector<float> &values);
  void set_labels(const std::vector<std::string> &labels);
  void clear();

  void set_show_grid(bool value) { m_show_grid = value; }
  bool show_grid() const { return m_show_grid; }

  void set_palette(const std::vector<Color> &palette);

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;

protected:
  void refresh_tokens();
  void ensure_palette();

  std::vector<float> m_values;
  std::vector<std::string> m_labels;
  std::vector<Color> m_palette;

  float m_max_value;
  bool m_show_grid;

  Color m_background;
  Color m_axis_color;
  Color m_grid_color;
  Color m_label_color;
};

/**
 * Fluent-themed line chart with uniform X spacing.
 */
class NANOGUI_EXPORT FluentWebLineChart : public Widget {
public:
  explicit FluentWebLineChart(Widget *parent);

  void set_points(const std::vector<float> &points);
  void set_palette(const std::vector<Color> &palette);
  void set_show_markers(bool value) { m_show_markers = value; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;

protected:
  void refresh_tokens();
  void ensure_palette();

  std::vector<float> m_points;
  std::vector<Color> m_palette;

  float m_min_value;
  float m_max_value;
  bool m_show_markers;

  Color m_background;
  Color m_axis_color;
  Color m_grid_color;
  Color m_label_color;
};

/**
 * Fluent-styled pie / donut chart.
 */
class NANOGUI_EXPORT FluentWebPieChart : public Widget {
public:
  explicit FluentWebPieChart(Widget *parent);

  void set_values(const std::vector<float> &values);
  void set_labels(const std::vector<std::string> &labels);
  void set_palette(const std::vector<Color> &palette);
  void set_inner_radius_ratio(float ratio) { m_inner_radius_ratio = ratio; }
  void set_show_labels(bool value) { m_show_labels = value; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;

protected:
  void refresh_tokens();
  void ensure_palette();

  std::vector<float> m_values;
  std::vector<std::string> m_labels;
  std::vector<Color> m_palette;

  float m_total;
  float m_inner_radius_ratio;
  bool m_show_labels;

  Color m_background;
  Color m_label_color;
};

NAMESPACE_END(nanogui)
