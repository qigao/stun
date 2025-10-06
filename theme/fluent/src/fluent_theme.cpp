/*
    src/fluent_theme.cpp -- Fluent Design 3 theme implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_theme.h>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

/// Lighten a color by mixing it with white
Color lighten_color(const Color &color, float amount) {
    float r = std::clamp(color.r() + (1.f - color.r()) * amount, 0.f, 1.f);
    float g = std::clamp(color.g() + (1.f - color.g()) * amount, 0.f, 1.f);
    float b = std::clamp(color.b() + (1.f - color.b()) * amount, 0.f, 1.f);
    return Color(r, g, b, color.a());
}

/// Darken a color by reducing its brightness
Color darken_color(const Color &color, float amount) {
    float r = std::clamp(color.r() * (1.f - amount), 0.f, 1.f);
    float g = std::clamp(color.g() * (1.f - amount), 0.f, 1.f);
    float b = std::clamp(color.b() * (1.f - amount), 0.f, 1.f);
    return Color(r, g, b, color.a());
}

} // namespace

FluentTheme::FluentTheme(NVGcontext *ctx, Palette palette)
    : Theme(ctx), m_palette(palette) {
    // Load Inter font (Segoe UI alternative) if available
    // Falls back to Roboto if Inter is not found
    load_fonts(ctx);
    
    apply_palette(palette);
}

void FluentTheme::apply_palette(Palette palette) {
    m_palette = palette;

    if (palette == Palette::Light) {
        // Fluent Design 3 Light Theme
        configure_palette(
            Color(0.1176f, 0.5333f, 0.8980f, 1.f),  // primary (Material Blue)
            Color(1.f, 1.f, 1.f, 1.f),              // onPrimary (white)
            Color(1.f, 1.f, 1.f, 1.f),              // surface (white)
            Color(0.949f, 0.949f, 0.949f, 1.f),     // background (light gray)
            Color(0.102f, 0.102f, 0.102f, 0.87f),   // onSurface (dark gray, high emphasis)
            Color(0.102f, 0.102f, 0.102f, 0.6f),    // secondary text (medium emphasis)
            Color(0.0118f, 0.8549f, 0.7725f, 1.f)   // accent (teal)
        );
    } else {
        // Fluent Design 3 Dark Theme
        configure_palette(
            Color(0.325f, 0.525f, 0.972f, 1.f),  // primary (lighter blue for dark mode)
            Color(0.f, 0.f, 0.f, 1.f),           // onPrimary (black)
            Color(0.121f, 0.121f, 0.129f, 1.f),  // surface (dark gray)
            Color(0.078f, 0.078f, 0.086f, 1.f),  // background (darker gray)
            Color(1.f, 1.f, 1.f, 0.87f),         // onSurface (white, high emphasis)
            Color(1.f, 1.f, 1.f, 0.6f),          // secondary text (medium emphasis)
            Color(0.180f, 0.800f, 0.820f, 1.f)   // accent (lighter teal)
        );
    }

    bake();
}

void FluentTheme::set_accent_color(const Color &accent) {
    m_accent = accent;
    bake();
}

void FluentTheme::bake() {
    // Text colors
    m_text_color = m_onSurface;
    m_disabled_text_color = Color(m_onSurface.r(), m_onSurface.g(), 
                                   m_onSurface.b(), 0.38f);
    m_text_color_shadow = Color(0.f, 0.f, 0.f, 0.f);

    // Button colors
    m_button_gradient_top_focused = m_button_gradient_bot_focused = m_primary;
    m_button_gradient_top_unfocused = lighten_color(
        m_primary, m_palette == Palette::Light ? 0.12f : 0.08f);
    m_button_gradient_bot_unfocused = m_button_gradient_top_unfocused;
    m_button_gradient_top_pushed = darken_color(m_primary, 0.18f);
    m_button_gradient_bot_pushed = m_button_gradient_top_pushed;

    // Window colors
    m_window_fill_unfocused = m_window_fill_focused = m_surface;
    m_window_title_unfocused = Color(m_onSurface.r(), m_onSurface.g(), 
                                      m_onSurface.b(), 0.54f);
    m_window_title_focused = m_onSurface;
    m_window_header_gradient_top = lighten_color(m_primary, 0.04f);
    m_window_header_gradient_bot = darken_color(m_primary, 0.16f);
    m_window_header_sep_top = lighten_color(m_primary, 0.35f);
    m_window_header_sep_bot = darken_color(m_primary, 0.30f);

    // Popup colors
    m_window_popup = lighten_color(
        m_surface, m_palette == Palette::Light ? 0.04f : 0.08f);
    m_window_popup_transparent = Color(m_window_popup.r(), m_window_popup.g(), 
                                       m_window_popup.b(), 0.f);

    // Border colors
    m_border_light = lighten_color(m_surface, 0.12f);
    m_border_medium = darken_color(m_surface, 0.12f);
    m_border_dark = darken_color(m_surface, 0.24f);

    // Shadow and transparency
    m_drop_shadow = Color(0.f, 0.f, 0.f, 
                          m_palette == Palette::Light ? 0.18f : 0.4f);
    m_transparent = Color(0.f, 0.f, 0.f, 0.f);

    // Icon color
    m_icon_color = m_onPrimary;
}

void FluentTheme::configure_palette(const Color &primary, const Color &onPrimary,
                                      const Color &surface, const Color &background,
                                      const Color &onSurface, const Color &secondary,
                                      const Color &accent) {
    m_primary = primary;
    m_onPrimary = onPrimary;
    m_surface = surface;
    m_background = background;
    m_onSurface = onSurface;
    m_secondaryText = secondary;
    m_accent = accent;
}

float FluentTheme::corner_radius(CornerRadius style) const {
    switch (style) {
        case CornerRadius::None:
            return 0.0f;
        case CornerRadius::Small:
            return 2.0f;
        case CornerRadius::Medium:
            return 4.0f;
        case CornerRadius::Large:
            return 8.0f;
        case CornerRadius::ExtraLarge:
            return 12.0f;
        case CornerRadius::Circle:
            return 9999.0f; // Large value for pill shape
        default:
            return 4.0f;
    }
}

void FluentTheme::load_fonts(NVGcontext *ctx) {
  // Try to load Inter font (Segoe UI Variable alternative)
  // If Inter is not available, the base Theme class already loaded Roboto
  
  // Font loading will be:
  // 1. Try Inter (if available in nanogui_resources.h)
  // 2. Fall back to Roboto (already loaded by Theme)
  
  // To use Inter, add Inter-Regular.ttf, Inter-SemiBold.ttf, Inter-Bold.ttf
  // to resources/ and rebuild. The build system will automatically embed them.
  
  // For now, Roboto provides excellent readability and is already available
}

NAMESPACE_END(nanogui)
