/*
    src/apple_theme.cpp -- Apple HIG theme implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/apple_theme.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

AppleTheme::AppleTheme(NVGcontext *ctx, Appearance appearance,
                       AccentColor accent)
    : Theme(ctx), m_appearance(appearance), m_accent_color(accent) {
  // Load Inter font (SF Pro alternative) if available
  // Falls back to Roboto if Inter is not found
  load_fonts(ctx);
  
  generate_accent_colors(accent);
  apply_appearance(appearance);
}

void AppleTheme::apply_appearance(Appearance appearance) {
  m_appearance = appearance;

  if (appearance == Appearance::Light) {
    // Light mode - macOS/iOS system colors
    m_label = Color(0, 0, 0, 255);
    m_secondary_label = Color(60, 60, 67, 153);
    m_tertiary_label = Color(60, 60, 67, 76);
    m_quaternary_label = Color(60, 60, 67, 45);

    m_text = Color(0, 0, 0, 255);
    m_placeholder_text = Color(60, 60, 67, 76);
    m_selected_text = Color(255, 255, 255, 255);
    m_text_background = Color(255, 255, 255, 255);
    m_selected_text_background = Color(0, 99, 225, 255);

    m_system_background = Color(255, 255, 255, 255);
    m_secondary_system_background = Color(242, 242, 247, 255);
    m_tertiary_system_background = Color(255, 255, 255, 255);

    m_system_grouped_background = Color(242, 242, 247, 255);
    m_secondary_system_grouped_background = Color(255, 255, 255, 255);
    m_tertiary_system_grouped_background = Color(242, 242, 247, 255);

    m_system_fill = Color(120, 120, 128, 51);
    m_secondary_system_fill = Color(120, 120, 128, 40);
    m_tertiary_system_fill = Color(118, 118, 128, 30);
    m_quaternary_system_fill = Color(116, 116, 128, 20);

    m_separator = Color(60, 60, 67, 73);
    m_opaque_separator = Color(198, 198, 200, 255);

    m_link = Color(0, 122, 255, 255);

    // Semantic colors - light mode
    m_system_red = Color(255, 59, 48, 255);
    m_system_orange = Color(255, 149, 0, 255);
    m_system_yellow = Color(255, 204, 0, 255);
    m_system_green = Color(52, 199, 89, 255);
    m_system_blue = Color(0, 122, 255, 255);
    m_system_purple = Color(175, 82, 222, 255);
    m_system_pink = Color(255, 45, 85, 255);
    m_system_gray = Color(142, 142, 147, 255);

  } else {
    // Dark mode - macOS/iOS system colors
    m_label = Color(255, 255, 255, 255);
    m_secondary_label = Color(235, 235, 245, 153);
    m_tertiary_label = Color(235, 235, 245, 76);
    m_quaternary_label = Color(235, 235, 245, 45);

    m_text = Color(255, 255, 255, 255);
    m_placeholder_text = Color(235, 235, 245, 76);
    m_selected_text = Color(255, 255, 255, 255);
    m_text_background = Color(28, 28, 30, 255);
    m_selected_text_background = Color(10, 132, 255, 255);

    m_system_background = Color(0, 0, 0, 255);
    m_secondary_system_background = Color(28, 28, 30, 255);
    m_tertiary_system_background = Color(44, 44, 46, 255);

    m_system_grouped_background = Color(0, 0, 0, 255);
    m_secondary_system_grouped_background = Color(28, 28, 30, 255);
    m_tertiary_system_grouped_background = Color(44, 44, 46, 255);

    m_system_fill = Color(120, 120, 128, 91);
    m_secondary_system_fill = Color(120, 120, 128, 81);
    m_tertiary_system_fill = Color(118, 118, 128, 61);
    m_quaternary_system_fill = Color(118, 118, 128, 45);

    m_separator = Color(84, 84, 88, 163);
    m_opaque_separator = Color(56, 56, 58, 255);

    m_link = Color(9, 132, 255, 255);

    // Semantic colors - dark mode
    m_system_red = Color(255, 69, 58, 255);
    m_system_orange = Color(255, 159, 10, 255);
    m_system_yellow = Color(255, 214, 10, 255);
    m_system_green = Color(48, 209, 88, 255);
    m_system_blue = Color(10, 132, 255, 255);
    m_system_purple = Color(191, 90, 242, 255);
    m_system_pink = Color(255, 55, 95, 255);
    m_system_gray = Color(152, 152, 157, 255);
  }

  // Update base theme colors
  m_drop_shadow = Color(0, 0, 0, 128);
  m_transparent = Color(0, 0, 0, 0);
  m_border_dark = m_separator;
  m_border_light = m_separator;
  m_border_medium = m_separator;
  m_text_color = m_label;
  m_disabled_text_color = m_tertiary_label;
  m_text_color_shadow = Color(0, 0, 0, 160);
  m_icon_color = m_label;

  m_button_gradient_top_focused = m_accent;
  m_button_gradient_bot_focused = m_accent_secondary;
  m_button_gradient_top_unfocused = m_system_fill;
  m_button_gradient_bot_unfocused = m_secondary_system_fill;
  m_button_gradient_top_pushed = m_accent_secondary;
  m_button_gradient_bot_pushed = m_accent;

  m_window_fill_unfocused = m_system_background;
  m_window_fill_focused = m_system_background;
  m_window_title_unfocused = m_secondary_label;
  m_window_title_focused = m_label;

  m_window_header_gradient_top = m_secondary_system_background;
  m_window_header_gradient_bot = m_secondary_system_background;
  m_window_header_sep_top = m_separator;
  m_window_header_sep_bot = m_separator;

  m_window_popup = m_secondary_system_background;
  m_window_popup_transparent = Color(m_window_popup.r(), m_window_popup.g(),
                                      m_window_popup.b(), 0.95f);
}

void AppleTheme::generate_accent_colors(AccentColor accent) {
  m_accent_color = accent;

  // Generate accent color based on selection
  switch (accent) {
  case AccentColor::Blue:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(0, 122, 255, 255)
                   : Color(10, 132, 255, 255);
    break;
  case AccentColor::Purple:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(175, 82, 222, 255)
                   : Color(191, 90, 242, 255);
    break;
  case AccentColor::Pink:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(255, 45, 85, 255)
                   : Color(255, 55, 95, 255);
    break;
  case AccentColor::Red:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(255, 59, 48, 255)
                   : Color(255, 69, 58, 255);
    break;
  case AccentColor::Orange:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(255, 149, 0, 255)
                   : Color(255, 159, 10, 255);
    break;
  case AccentColor::Yellow:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(255, 204, 0, 255)
                   : Color(255, 214, 10, 255);
    break;
  case AccentColor::Green:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(52, 199, 89, 255)
                   : Color(48, 209, 88, 255);
    break;
  case AccentColor::Gray:
    m_accent = (m_appearance == Appearance::Light)
                   ? Color(142, 142, 147, 255)
                   : Color(152, 152, 157, 255);
    break;
  }

  // Generate secondary accent (slightly darker/lighter)
  float factor = (m_appearance == Appearance::Light) ? 0.85f : 1.15f;
  m_accent_secondary = Color(m_accent.r() * factor, m_accent.g() * factor,
                             m_accent.b() * factor, m_accent.a());
}

void AppleTheme::set_accent_color(AccentColor accent) {
  generate_accent_colors(accent);
  apply_appearance(m_appearance); // Refresh colors
}

float AppleTheme::corner_radius(CornerStyle style) const {
  switch (style) {
  case CornerStyle::Small:
    return 4.0f;
  case CornerStyle::Medium:
    return 8.0f;
  case CornerStyle::Large:
    return 12.0f;
  case CornerStyle::ExtraLarge:
    return 16.0f;
  case CornerStyle::Continuous:
    return 10.0f; // Approximation of continuous curve
  default:
    return 8.0f;
  }
}

float AppleTheme::font_size(TextStyle style) const {
  switch (style) {
  case TextStyle::LargeTitle:
    return 34.0f;
  case TextStyle::Title1:
    return 28.0f;
  case TextStyle::Title2:
    return 22.0f;
  case TextStyle::Title3:
    return 20.0f;
  case TextStyle::Headline:
    return 17.0f;
  case TextStyle::Body:
    return 17.0f;
  case TextStyle::Callout:
    return 16.0f;
  case TextStyle::Subheadline:
    return 15.0f;
  case TextStyle::Footnote:
    return 13.0f;
  case TextStyle::Caption1:
    return 12.0f;
  case TextStyle::Caption2:
    return 11.0f;
  default:
    return 17.0f;
  }
}

int AppleTheme::font_weight(TextStyle style) const {
  switch (style) {
  case TextStyle::Headline:
    return 2; // Semibold
  default:
    return 1; // Regular
  }
}

void AppleTheme::load_fonts(NVGcontext *ctx) {
  // Try to load Inter font (SF Pro alternative)
  // If Inter is not available, the base Theme class already loaded Roboto
  
  // Check if Inter fonts are available by trying to create them
  // Note: This requires Inter fonts to be in resources/ and rebuilt
  
  // For now, we use the fonts already loaded by Theme base class
  // To use Inter, add Inter-Regular.ttf, Inter-SemiBold.ttf, Inter-Bold.ttf
  // to resources/ and rebuild. The build system will automatically embed them.
  
  // Font loading will be:
  // 1. Try Inter (if available in nanogui_resources.h)
  // 2. Fall back to Roboto (already loaded by Theme)
  
  // The actual Inter font loading will be added once the fonts are in resources
  // For now, Roboto provides excellent readability
}

NAMESPACE_END(nanogui)
