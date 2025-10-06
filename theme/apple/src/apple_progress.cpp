/*
    src/apple_progress.cpp -- Apple HIG progress implementation
*/

#include <nanogui/apple_progress.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

AppleProgress::AppleProgress(Widget *parent, Style style)
    : Widget(parent), m_value(0.0f), m_style(style), m_indeterminate(false) {
  if (style == Style::Linear) {
    set_fixed_height(4);
  } else {
    set_fixed_size(Vector2i(20, 20));
  }
}

AppleTheme *AppleProgress::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleProgress::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  if (m_style == Style::Linear) {
    // Linear progress bar
    float track_height = 4.0f;
    float corner_radius = track_height * 0.5f;

    // Draw track
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), track_height, corner_radius);
    nvgFillColor(ctx, theme->tertiary_system_fill());
    nvgFill(ctx);

    // Draw progress
    if (m_value > 0.0f) {
      float progress_width = m_size.x() * m_value;
      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), progress_width, track_height,
                     corner_radius);
      nvgFillColor(ctx, theme->accent());
      nvgFill(ctx);
    }
  } else {
    // Circular activity indicator
    float cx = m_pos.x() + m_size.x() * 0.5f;
    float cy = m_pos.y() + m_size.y() * 0.5f;
    float radius = std::min(m_size.x(), m_size.y()) * 0.4f;

    if (m_indeterminate) {
      // Spinning indicator
      float time = (float)Screen::get_time();
      float angle = fmod(time * 2.0f, 2.0f * NVG_PI);
      
      nvgBeginPath(ctx);
      nvgArc(ctx, cx, cy, radius, angle, angle + NVG_PI * 1.5f, NVG_CW);
      nvgStrokeWidth(ctx, 2.0f);
      nvgStrokeColor(ctx, theme->accent());
      nvgStroke(ctx);
    } else {
      // Progress circle
      float angle = m_value * 2.0f * NVG_PI;
      
      // Background circle
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx, cy, radius);
      nvgStrokeWidth(ctx, 2.0f);
      nvgStrokeColor(ctx, theme->tertiary_system_fill());
      nvgStroke(ctx);

      // Progress arc
      if (m_value > 0.0f) {
        nvgBeginPath(ctx);
        nvgArc(ctx, cx, cy, radius, -NVG_PI * 0.5f, -NVG_PI * 0.5f + angle, NVG_CW);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStrokeColor(ctx, theme->accent());
        nvgStroke(ctx);
      }
    }
  }
}

Vector2i AppleProgress::preferred_size_impl(NVGcontext *ctx) const {
  if (m_style == Style::Linear) {
    return Vector2i(200, 4);
  } else {
    return Vector2i(20, 20);
  }
}

NAMESPACE_END(nanogui)
