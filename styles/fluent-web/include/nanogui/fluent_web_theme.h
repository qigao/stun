/**
 * \file fluent_web_theme.h
 * \brief Fluent 2 web design tokens and theme infrastructure.
 */

#pragma once

#include <array>
#include <cstdint>

#include <nanogui/theme.h>

#define FLUENT_WEB_INCLUDE_TOKEN_LIST
#include <nanogui/detail/fluent_web_token_data.inc>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentWebTheme
 * \brief Fluent 2 web design system implementation for NanoGUI.
 *
 * The theme mirrors Microsoft's Fluent 2 token catalog, exposing semantic
 * colors, typography specifications, and spacing constants. Widgets can query
 * these tokens to ensure visual consistency with Fluent-enabled web surfaces
 * while remaining fully integrated with NanoGUI's rendering pipeline.
 */
class NANOGUI_EXPORT FluentWebTheme : public Theme {
public:
  enum class Mode : uint8_t { Light = 0, Dark };

  enum class ColorToken : uint16_t {
#define FLUENT_WEB_COLOR_ENUM(name) name,
    FLUENT_WEB_COLOR_TOKENS(FLUENT_WEB_COLOR_ENUM)
#undef FLUENT_WEB_COLOR_ENUM
        Count
  };

  struct TypographySpec {
    float font_size;
    float line_height;
    int font_weight;
    float letter_spacing;
  };

  enum class TypographyToken : uint8_t {
    Display,
    LargeTitle,
    Title1,
    Title2,
    Title3,
    Subtitle1,
    Subtitle2,
    Subtitle2Stronger,
    Body1,
    Body1Strong,
    Body1Stronger,
    Body2,
    Caption1,
    Caption1Strong,
    Caption1Stronger,
    Caption2,
    Caption2Strong,
    Count
  };

  enum class SpaceToken : uint8_t { None, XXS, XS, SNudge, S, MNudge, M, L, XL, XXL, XXXL, Count };

  enum class RadiusToken : uint8_t { None, Small, Medium, Large, XLarge, Circular, Count };

  struct ShadowSpec {
    float x_offset;
    float y_offset;
    float blur_radius;
    float spread;
    float opacity;
  };

  enum class Elevation : uint8_t { Level1, Level2, Level3, Level4, Level5, Level6, Count };

  struct ElevationLayers {
    ShadowSpec key;
    ShadowSpec ambient;
  };

  struct BrandRamp {
    std::array<Color, 16> stops{};
  };

  explicit FluentWebTheme(NVGcontext *ctx, Mode mode = Mode::Light);

  void apply(Mode mode);
  void set_brand_color(const Color &brand);
  void set_brand_variants(const BrandRamp &variants);

  Mode mode() const { return m_mode; }

  const Color &color(ColorToken token) const;
  const TypographySpec &typography(TypographyToken token) const;
  float spacing(SpaceToken token) const;
  float corner_radius(RadiusToken token) const;
  const ElevationLayers &shadow(Elevation level) const;

protected:
  void bake();

private:
  static constexpr size_t kColorCount = static_cast<size_t>(ColorToken::Count);
  static constexpr size_t kTypographyCount = static_cast<size_t>(TypographyToken::Count);
  static constexpr size_t kSpacingCount = static_cast<size_t>(SpaceToken::Count);
  static constexpr size_t kRadiusCount = static_cast<size_t>(RadiusToken::Count);
  static constexpr size_t kElevationCount = static_cast<size_t>(Elevation::Count);

  void apply_brand_overrides();
  void update_elevation_tokens();

  Color mix(const Color &a, const Color &b, float factor) const;
  Color adjust_luminance(const Color &color, float amount) const;
  static constexpr size_t index(ColorToken token) { return static_cast<size_t>(token); }

  Mode m_mode;
  Color m_brand_accent;
  std::array<Color, kColorCount> m_light_defaults;
  std::array<Color, kColorCount> m_dark_defaults;
  std::array<Color, kColorCount> m_colors;
  std::array<TypographySpec, kTypographyCount> m_typography;
  std::array<float, kSpacingCount> m_spacing;
  std::array<float, kRadiusCount> m_radius;
  std::array<ElevationLayers, kElevationCount> m_elevation;
  BrandRamp m_brand_ramp;
};

NAMESPACE_END(nanogui)
