/*
    src/apple_button.cpp -- Apple HIG button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/apple_button.h>
#include <nanogui/opengl.h>
#include <nanogui/theme.h>

NAMESPACE_BEGIN(nanogui)

AppleButton::AppleButton(Widget *parent, const std::string &caption, int icon,
                         Style style)
    : Button(parent, caption, icon), m_style(style) {
  set_fixed_height(32); // Standard button height
}

void AppleButton::set_style(Style style) {
  m_style = style;
}

AppleTheme *AppleButton::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

Color AppleButton::get_background_color() const {
  AppleTheme *theme = apple_theme();
  if (!theme)
    return Color(0.5f, 0.5f, 0.5f, 1.0f);

  if (!m_enabled) {
    return theme->quaternary_system_fill();
  }

  switch (m_style) {
  case Style::Primary:
    if (m_pushed)
      return theme->accent_secondary();
    else if (m_mouse_focus)
      return Color(theme->accent().r() * 1.1f, theme->accent().g() * 1.1f,
                   theme->accent().b() * 1.1f, theme->accent().a());
    else
      return theme->accent();

  case Style::Secondary:
    if (m_pushed)
      return theme->tertiary_system_fill();
    else if (m_mouse_focus)
      return theme->secondary_system_fill();
    else
      return theme->system_fill();

  case Style::Tertiary:
    if (m_pushed)
      return theme->tertiary_system_fill();
    else if (m_mouse_focus)
      return theme->system_fill();
    else
      return Color(0, 0, 0, 0); // Transparent

  case Style::Destructive:
    if (m_pushed)
      return Color(theme->system_red().r() * 0.85f,
                   theme->system_red().g() * 0.85f,
                   theme->system_red().b() * 0.85f, theme->system_red().a());
    else if (m_mouse_focus)
      return Color(theme->system_red().r() * 1.1f,
                   theme->system_red().g() * 1.1f,
                   theme->system_red().b() * 1.1f, theme->system_red().a());
    else
      return theme->system_red();
  }

  return theme->accent();
}

Color AppleButton::get_text_color() const {
  AppleTheme *theme = apple_theme();
  if (!theme)
    return Color(1.0f, 1.0f, 1.0f, 1.0f);

  if (!m_enabled) {
    return theme->quaternary_label();
  }

  switch (m_style) {
  case Style::Primary:
  case Style::Destructive:
    return Color(255, 255, 255, 255); // White text on colored background

  case Style::Secondary:
  case Style::Tertiary:
    return theme->label();
  }

  return theme->label();
}

Color AppleButton::get_border_color() const {
  AppleTheme *theme = apple_theme();
  if (!theme)
    return Color(0.5f, 0.5f, 0.5f, 1.0f);

  if (!m_enabled) {
    return theme->quaternary_system_fill();
  }

  if (m_pushed || m_mouse_focus) {
    return theme->separator();
  }

  return theme->separator();
}

void AppleButton::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme) {
    Button::draw(ctx);
    return;
  }

  NVGcolor bg_color = get_background_color();
  NVGcolor text_color = get_text_color();

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Medium);

  // Draw background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x() + 1, m_pos.y() + 1, m_size.x() - 2,
                 m_size.y() - 2, corner_radius);

  if (m_style == Style::Tertiary && !m_pushed && !m_mouse_focus) {
    // Tertiary has no background when not interacting
  } else {
    nvgFillColor(ctx, bg_color);
    nvgFill(ctx);
  }

  // Draw border for secondary style
  if (m_style == Style::Secondary) {
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, get_border_color());
    nvgStroke(ctx);
  }

  // Draw icon and text
  float icon_x = m_pos.x();
  float text_x = m_pos.x();
  float center_y = m_pos.y() + m_size.y() * 0.5f;

  if (m_icon) {
    float icon_size = m_font_size * 1.2f;
    nvgFontSize(ctx, icon_size);
    nvgFontFace(ctx, "icons");
    nvgFillColor(ctx, text_color);
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    icon_x = m_pos.x() + AppleTheme::spacing_default;
    nvgText(ctx, icon_x, center_y, utf8(m_icon).data(), nullptr);

    text_x = icon_x + icon_size + AppleTheme::spacing_tight;
  } else {
    text_x = m_pos.x() + m_size.x() * 0.5f;
  }

  // Draw caption
  nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Body));
  nvgFontFace(ctx, "sans");
  nvgFillColor(ctx, text_color);

  if (m_icon) {
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  } else {
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  }

  nvgText(ctx, text_x, center_y, m_caption.c_str(), nullptr);
}

Vector2i AppleButton::preferred_size_impl(NVGcontext *ctx) const {
  AppleTheme *theme = apple_theme();
  float font_size = theme ? theme->font_size(AppleTheme::TextStyle::Body)
                          : m_font_size;

  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans");

  float tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);

  float iw = 0.0f;
  if (m_icon) {
    float icon_size = font_size * 1.2f;
    iw = icon_size + AppleTheme::spacing_tight;
  }

  float padding = AppleTheme::spacing_default * 2;
  int width = static_cast<int>(tw + iw + padding);
  int height = 32; // Standard button height

  return Vector2i(width, height);
}

NAMESPACE_END(nanogui)
