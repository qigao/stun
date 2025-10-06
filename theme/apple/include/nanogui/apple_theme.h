/*
    nanogui/apple_theme.h -- Apple Human Interface Guidelines theme

    This theme implements Apple's HIG design system with system colors,
    SF Pro typography, and native macOS/iOS styling.

    Based on: https://developer.apple.com/design/human-interface-guidelines/

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleTheme apple_theme.h nanogui/apple_theme.h
 *
 * \brief Apple Human Interface Guidelines theme implementation
 *
 * Implements Apple's design system with:
 * - System colors that adapt to light/dark mode
 * - SF Pro typography scale
 * - Vibrancy and translucency effects
 * - Continuous corner curves
 * - Semantic color roles
 */
class NANOGUI_EXPORT AppleTheme : public Theme {
public:
  /// Appearance mode (light or dark)
  enum class Appearance {
    Light, ///< Light appearance
    Dark,  ///< Dark appearance
    Auto   ///< Follow system preference
  };

  /// Accent color options
  enum class AccentColor {
    Blue,    ///< Default blue
    Purple,  ///< Purple
    Pink,    ///< Pink
    Red,     ///< Red
    Orange,  ///< Orange
    Yellow,  ///< Yellow
    Green,   ///< Green
    Gray     ///< Graphite
  };

  /// Corner radius styles
  enum class CornerStyle {
    Small,      ///< 4pt - Buttons, badges
    Medium,     ///< 8pt - Text fields, cards
    Large,      ///< 12pt - Sheets, popovers
    ExtraLarge, ///< 16pt - Large cards
    Continuous  ///< Apple's continuous curve
  };

  /// Typography styles (SF Pro scale)
  enum class TextStyle {
    LargeTitle,   ///< 34pt, Regular
    Title1,       ///< 28pt, Regular
    Title2,       ///< 22pt, Regular
    Title3,       ///< 20pt, Regular
    Headline,     ///< 17pt, Semibold
    Body,         ///< 17pt, Regular
    Callout,      ///< 16pt, Regular
    Subheadline,  ///< 15pt, Regular
    Footnote,     ///< 13pt, Regular
    Caption1,     ///< 12pt, Regular
    Caption2      ///< 11pt, Regular
  };

  /**
   * \brief Construct an Apple theme
   *
   * \param ctx NanoVG context
   * \param appearance Light or dark appearance
   * \param accent Accent color
   */
  explicit AppleTheme(NVGcontext *ctx,
                      Appearance appearance = Appearance::Light,
                      AccentColor accent = AccentColor::Blue);

  /// Apply appearance mode
  void apply_appearance(Appearance appearance);

  /// Set accent color
  void set_accent_color(AccentColor accent);

  /// Get current appearance
  Appearance appearance() const { return m_appearance; }

  /// Get current accent color
  AccentColor accent_color() const { return m_accent_color; }

  // System Colors - Labels
  const Color &label() const { return m_label; }
  const Color &secondary_label() const { return m_secondary_label; }
  const Color &tertiary_label() const { return m_tertiary_label; }
  const Color &quaternary_label() const { return m_quaternary_label; }

  // System Colors - Text
  const Color &text() const { return m_text; }
  const Color &placeholder_text() const { return m_placeholder_text; }
  const Color &selected_text() const { return m_selected_text; }
  const Color &text_background() const { return m_text_background; }
  const Color &selected_text_background() const {
    return m_selected_text_background;
  }

  // System Colors - Backgrounds
  const Color &system_background() const { return m_system_background; }
  const Color &secondary_system_background() const {
    return m_secondary_system_background;
  }
  const Color &tertiary_system_background() const {
    return m_tertiary_system_background;
  }

  // System Colors - Grouped Backgrounds
  const Color &system_grouped_background() const {
    return m_system_grouped_background;
  }
  const Color &secondary_system_grouped_background() const {
    return m_secondary_system_grouped_background;
  }
  const Color &tertiary_system_grouped_background() const {
    return m_tertiary_system_grouped_background;
  }

  // System Colors - Fills
  const Color &system_fill() const { return m_system_fill; }
  const Color &secondary_system_fill() const { return m_secondary_system_fill; }
  const Color &tertiary_system_fill() const { return m_tertiary_system_fill; }
  const Color &quaternary_system_fill() const {
    return m_quaternary_system_fill;
  }

  // Semantic Colors
  const Color &system_red() const { return m_system_red; }
  const Color &system_orange() const { return m_system_orange; }
  const Color &system_yellow() const { return m_system_yellow; }
  const Color &system_green() const { return m_system_green; }
  const Color &system_blue() const { return m_system_blue; }
  const Color &system_purple() const { return m_system_purple; }
  const Color &system_pink() const { return m_system_pink; }
  const Color &system_gray() const { return m_system_gray; }

  // Accent color (user-selected)
  const Color &accent() const { return m_accent; }
  const Color &accent_secondary() const { return m_accent_secondary; }

  // Separators
  const Color &separator() const { return m_separator; }
  const Color &opaque_separator() const { return m_opaque_separator; }

  // Links
  const Color &link() const { return m_link; }

  /// Get corner radius for style
  float corner_radius(CornerStyle style) const;

  /// Get font size for text style
  float font_size(TextStyle style) const;

  /// Get font weight for text style (0=thin, 1=regular, 2=semibold, 3=bold)
  int font_weight(TextStyle style) const;

  /// Standard spacing values
  static constexpr float spacing_tight = 4.0f;
  static constexpr float spacing_default = 8.0f;
  static constexpr float spacing_comfortable = 12.0f;
  static constexpr float spacing_section = 16.0f;
  static constexpr float spacing_large = 20.0f;
  static constexpr float spacing_extra_large = 24.0f;

protected:
  /// Generate accent color variations
  void generate_accent_colors(AccentColor accent);
  
  /// Load Inter fonts (SF Pro alternative)
  void load_fonts(NVGcontext *ctx);

  Appearance m_appearance;
  AccentColor m_accent_color;

  // Label colors
  Color m_label;
  Color m_secondary_label;
  Color m_tertiary_label;
  Color m_quaternary_label;

  // Text colors
  Color m_text;
  Color m_placeholder_text;
  Color m_selected_text;
  Color m_text_background;
  Color m_selected_text_background;

  // Background colors
  Color m_system_background;
  Color m_secondary_system_background;
  Color m_tertiary_system_background;

  // Grouped background colors
  Color m_system_grouped_background;
  Color m_secondary_system_grouped_background;
  Color m_tertiary_system_grouped_background;

  // Fill colors
  Color m_system_fill;
  Color m_secondary_system_fill;
  Color m_tertiary_system_fill;
  Color m_quaternary_system_fill;

  // Semantic colors
  Color m_system_red;
  Color m_system_orange;
  Color m_system_yellow;
  Color m_system_green;
  Color m_system_blue;
  Color m_system_purple;
  Color m_system_pink;
  Color m_system_gray;

  // Accent colors
  Color m_accent;
  Color m_accent_secondary;

  // Separators
  Color m_separator;
  Color m_opaque_separator;

  // Links
  Color m_link;
};

NAMESPACE_END(nanogui)
