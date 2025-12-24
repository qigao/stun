/*
 * Flex Widgets - Theme System
 * Fluent Design (Windows 11) as default
 */

#pragma once

#include <cstdint>
#include <string>

namespace flex {
namespace widgets {

// ARGB color helper
struct ThemeColor {
  uint32_t value;

  constexpr ThemeColor() : value(0xFF000000) {} // Default: opaque black
  constexpr ThemeColor(uint32_t argb) : value(argb) {}
  constexpr ThemeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : value((uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b) {}

  constexpr uint8_t a() const { return (value >> 24) & 0xFF; }
  constexpr uint8_t r() const { return (value >> 16) & 0xFF; }
  constexpr uint8_t g() const { return (value >> 8) & 0xFF; }
  constexpr uint8_t b() const { return value & 0xFF; }

  // Lighten/darken
  ThemeColor lighter(float amount = 0.1f) const {
    auto blend = [](uint8_t c, float amt) -> uint8_t { return uint8_t(c + (255 - c) * amt); };
    return ThemeColor(blend(r(), amount), blend(g(), amount), blend(b(), amount), a());
  }

  ThemeColor darker(float amount = 0.1f) const {
    auto blend = [](uint8_t c, float amt) -> uint8_t { return uint8_t(c * (1.0f - amt)); };
    return ThemeColor(blend(r(), amount), blend(g(), amount), blend(b(), amount), a());
  }

  ThemeColor with_alpha(uint8_t alpha) const { return ThemeColor(r(), g(), b(), alpha); }
};

struct WidgetTheme {
  // ========== Colors ==========
  // Accent colors
  ThemeColor accent;         // Primary action color
  ThemeColor accent_hover;   // Hover state
  ThemeColor accent_pressed; // Pressed state

  // Background colors
  ThemeColor background;      // Page/window background
  ThemeColor surface;         // Control surface (cards, inputs)
  ThemeColor surface_hover;   // Surface hover
  ThemeColor surface_pressed; // Surface pressed

  // Text colors
  ThemeColor text_primary;   // Main text
  ThemeColor text_secondary; // Secondary/hint text
  ThemeColor text_on_accent; // Text on accent background
  ThemeColor text_disabled;  // Disabled text

  // Border colors
  ThemeColor border;       // Default border
  ThemeColor border_hover; // Hover border
  ThemeColor border_focus; // Focus border (usually accent)

  // State colors
  ThemeColor disabled_bg; // Disabled background
  ThemeColor success;     // Success/positive
  ThemeColor warning;     // Warning
  ThemeColor error;       // Error/danger

  // ========== Sizes ==========
  float corner_radius = 4.0f;
  float border_width = 1.0f;
  float font_size = 14.0f;
  float font_size_small = 12.0f;
  float font_size_large = 16.0f;

  float control_height = 32.0f;
  float control_height_small = 24.0f;
  float control_height_large = 40.0f;

  float padding_h = 12.0f;
  float padding_v = 6.0f;
  float spacing = 8.0f;
  float icon_size = 16.0f;

  // ========== Animation ==========
  float transition_fast = 0.1f;    // 100ms - quick feedback
  float transition_normal = 0.15f; // 150ms - state changes
  float transition_slow = 0.25f;   // 250ms - complex animations

  // ========== Typography ==========
  std::string font_family = "sans-serif";
};

// ============================================================================
// Preset Themes
// ============================================================================

inline WidgetTheme theme_fluent() {
  WidgetTheme theme;

  // Fluent Design System - Windows 11
  theme.accent = ThemeColor(0xFF0078D4); // Windows Blue
  theme.accent_hover = ThemeColor(0xFF1A86D8);
  theme.accent_pressed = ThemeColor(0xFF005A9E);

  theme.background = ThemeColor(0xFFF3F3F3); // Mica-like
  theme.surface = ThemeColor(0xFFFFFFFF);
  theme.surface_hover = ThemeColor(0xFFF9F9F9);
  theme.surface_pressed = ThemeColor(0xFFF0F0F0);

  theme.text_primary = ThemeColor(0xFF1A1A1A);
  theme.text_secondary = ThemeColor(0xFF5D5D5D);
  theme.text_on_accent = ThemeColor(0xFFFFFFFF);
  theme.text_disabled = ThemeColor(0xFFA0A0A0);

  theme.border = ThemeColor(0xFFE5E5E5);
  theme.border_hover = ThemeColor(0xFFD0D0D0);
  theme.border_focus = ThemeColor(0xFF0078D4);

  theme.disabled_bg = ThemeColor(0xFFF5F5F5);
  theme.success = ThemeColor(0xFF0F7B0F);
  theme.warning = ThemeColor(0xFFFFB900);
  theme.error = ThemeColor(0xFFC42B1C);

  theme.font_family = "Segoe UI";

  return theme;
}

inline WidgetTheme theme_dark() {
  WidgetTheme theme;

  // Dark theme
  theme.accent = ThemeColor(0xFF60CDFF); // Light blue
  theme.accent_hover = ThemeColor(0xFF7AD4FF);
  theme.accent_pressed = ThemeColor(0xFF4CC2FF);

  theme.background = ThemeColor(0xFF202020);
  theme.surface = ThemeColor(0xFF2D2D2D);
  theme.surface_hover = ThemeColor(0xFF383838);
  theme.surface_pressed = ThemeColor(0xFF404040);

  theme.text_primary = ThemeColor(0xFFFFFFFF);
  theme.text_secondary = ThemeColor(0xFFA0A0A0);
  theme.text_on_accent = ThemeColor(0xFF000000);
  theme.text_disabled = ThemeColor(0xFF6D6D6D);

  theme.border = ThemeColor(0xFF3D3D3D);
  theme.border_hover = ThemeColor(0xFF505050);
  theme.border_focus = ThemeColor(0xFF60CDFF);

  theme.disabled_bg = ThemeColor(0xFF2A2A2A);
  theme.success = ThemeColor(0xFF6CCB5F);
  theme.warning = ThemeColor(0xFFFFB900);
  theme.error = ThemeColor(0xFFFF6961);

  theme.font_family = "Segoe UI";

  return theme;
}

inline WidgetTheme theme_light() {
  // Light theme is essentially the same as Fluent
  return theme_fluent();
}

// Global theme instance
inline WidgetTheme &current_theme() {
  static WidgetTheme theme = theme_fluent();
  return theme;
}

inline void set_theme(const WidgetTheme &theme) { current_theme() = theme; }

} // namespace widgets
} // namespace flex
