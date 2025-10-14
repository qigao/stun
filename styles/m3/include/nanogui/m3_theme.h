/*
    nanogui/m3_theme.h -- Material Design 3 (Material You) theme

    This theme implements Google's Material Design 3 design system with
    dynamic color palettes, tonal surfaces, and adaptive components.

    Based on: https://m3.material.io

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <array>
#include <nanogui/theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Theme m3_theme.h nanogui/m3_theme.h
 *
 * \brief Material Design 3 (Material You) theme implementation
 *
 * Implements the M3 design system with:
 * - Dynamic color from seed color
 * - Tonal palettes (13 color roles)
 * - State layers for interactions
 * - Elevation through color
 * - M3 typography scale
 */
class M3Theme : public Theme {
public:
  /// Color scheme (light or dark mode)
  enum class Scheme {
    Light, ///< Light color scheme
    Dark   ///< Dark color scheme
  };

  /// Shape families for corner radius
  enum class ShapeFamily {
    None,       ///< 0dp - No rounding
    ExtraSmall, ///< 4dp
    Small,      ///< 8dp
    Medium,     ///< 12dp
    Large,      ///< 16dp
    ExtraLarge  ///< 28dp
  };

  /// Elevation levels (0-5)
  enum class Elevation {
    Level0 = 0, ///< 0dp - No elevation
    Level1 = 1, ///< 1dp
    Level2 = 2, ///< 3dp
    Level3 = 3, ///< 6dp
    Level4 = 4, ///< 8dp
    Level5 = 5  ///< 12dp
  };

  /**
   * \brief Construct an M3 theme from a seed color
   *
   * \param ctx NanoVG context
   * \param seed_color Source color for generating tonal palettes
   * \param scheme Light or dark color scheme
   */
  explicit M3Theme(NVGcontext *ctx,
                   const Color &seed_color = Color(0.4f, 0.2f, 0.8f, 1.0f),
                   Scheme scheme = Scheme::Light);

  /// Apply a color scheme (light or dark)
  void apply_scheme(Scheme scheme);

  /// Set seed color and regenerate palettes
  void set_seed_color(const Color &seed_color);

  /// Get current scheme
  Scheme scheme() const { return m_scheme; }

  // M3 Color Roles - Primary
  const Color &primary() const { return m_primary; }
  const Color &on_primary() const { return m_on_primary; }
  const Color &primary_container() const { return m_primary_container; }
  const Color &on_primary_container() const { return m_on_primary_container; }

  // M3 Color Roles - Secondary
  const Color &secondary() const { return m_secondary; }
  const Color &on_secondary() const { return m_on_secondary; }
  const Color &secondary_container() const { return m_secondary_container; }
  const Color &on_secondary_container() const {
    return m_on_secondary_container;
  }

  // M3 Color Roles - Tertiary
  const Color &tertiary() const { return m_tertiary; }
  const Color &on_tertiary() const { return m_on_tertiary; }
  const Color &tertiary_container() const { return m_tertiary_container; }
  const Color &on_tertiary_container() const { return m_on_tertiary_container; }

  // M3 Color Roles - Error
  const Color &error() const { return m_error; }
  const Color &on_error() const { return m_on_error; }
  const Color &error_container() const { return m_error_container; }
  const Color &on_error_container() const { return m_on_error_container; }

  // M3 Color Roles - Surface
  const Color &surface() const { return m_surface; }
  const Color &on_surface() const { return m_on_surface; }
  const Color &surface_variant() const { return m_surface_variant; }
  const Color &on_surface_variant() const { return m_on_surface_variant; }

  // M3 Color Roles - Background
  const Color &background() const { return m_background; }
  const Color &on_background() const { return m_on_background; }

  // M3 Color Roles - Outline
  const Color &outline() const { return m_outline; }
  const Color &outline_variant() const { return m_outline_variant; }

  // M3 Color Roles - Inverse
  const Color &inverse_surface() const { return m_inverse_surface; }
  const Color &inverse_on_surface() const { return m_inverse_on_surface; }
  const Color &inverse_primary() const { return m_inverse_primary; }

  // M3 Color Roles - Shadow & Scrim
  const Color &shadow() const { return m_shadow; }
  const Color &scrim() const { return m_scrim; }

  /// Get corner radius for shape family
  float corner_radius(ShapeFamily family) const;

  /// Get elevation tint color and opacity
  Color elevation_tint(Elevation level) const;

  /// Get state layer color for interaction states
  Color state_layer(const Color &base, float opacity) const;

  /// Set whether animations should be disabled (for accessibility)
  void set_animations_enabled(bool enabled) { m_animations_enabled = enabled; }

  /// Check if animations are enabled (respects prefers-reduced-motion)
  bool animations_enabled() const { return m_animations_enabled; }

protected:
  /// Generate tonal palettes from seed color
  void generate_palettes(const Color &seed_color);

  /// Bake theme colors into base Theme properties
  void bake();

private:
  // Core palette generation
  struct TonalPalette {
    std::array<Color, 13>
        tones; // Tones: 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 99, 100
  };

  TonalPalette generate_tonal_palette(const Color &source);
  Color get_tone(const TonalPalette &palette, int tone) const;

  // Color conversion helpers
  struct HCT {
    float h, c, t;
  }; // Hue, Chroma, Tone
  HCT rgb_to_hct(const Color &rgb) const;
  Color hct_to_rgb(const HCT &hct) const;

  Scheme m_scheme;
  Color m_seed_color;

  // M3 Color Roles
  Color m_primary, m_on_primary, m_primary_container, m_on_primary_container;
  Color m_secondary, m_on_secondary, m_secondary_container,
      m_on_secondary_container;
  Color m_tertiary, m_on_tertiary, m_tertiary_container,
      m_on_tertiary_container;
  Color m_error, m_on_error, m_error_container, m_on_error_container;
  Color m_surface, m_on_surface, m_surface_variant, m_on_surface_variant;
  Color m_background, m_on_background;
  Color m_outline, m_outline_variant;
  Color m_inverse_surface, m_inverse_on_surface, m_inverse_primary;
  Color m_shadow, m_scrim;

  // Tonal palettes
  TonalPalette m_primary_palette;
  TonalPalette m_secondary_palette;
  TonalPalette m_tertiary_palette;
  TonalPalette m_neutral_palette;
  TonalPalette m_neutral_variant_palette;
  TonalPalette m_error_palette;

  // Animation preferences
  bool m_animations_enabled = true;  // Default to enabled
};

NAMESPACE_END(nanogui)
