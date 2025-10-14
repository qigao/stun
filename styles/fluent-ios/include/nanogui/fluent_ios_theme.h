/**
 * \file fluent_ios_theme.h
 * \brief Fluent UI iOS themed palette, typography, and elevation tokens.
 */

#pragma once

#include <array>
#include <cstdint>

#include <nanogui/theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentIOSTheme
 * \brief Encapsulates Fluent UI for iOS design tokens and styling helpers.
 *
 * The theme exposes semantic color roles, typography scales, spacing tokens,
 * and elevation presets that mirror Microsoft's Fluent UI guidance on iOS.
 * Widgets can query these tokens to render native-feeling controls that adapt
 * to light and dark color schemes.
 */
class NANOGUI_EXPORT FluentIOSTheme : public Theme {
public:
    enum class ColorScheme {
        Light,
        Dark
    };

    enum class TypographyStyle {
        LargeTitle,
        Title1,
        Title2,
        Title3,
        Headline,
        Body,
        Callout,
        Subheadline,
        Footnote,
        Caption1,
        Caption2
    };

    struct TypographyToken {
        int size;
        int line_height;
        int weight; // 0 = regular, 1 = semibold/bold
    };

    enum class SpacingToken {
        XSmall,
        Small,
        Medium,
        Large,
        XLarge
    };

    enum class SemanticColor : uint8_t {
        BackgroundPrimary = 0,
        BackgroundSecondary,
        Surface,
        SurfaceElevated,
        Separator,
        TextPrimary,
        TextSecondary,
        TextMuted,
        AccentPrimary,
        AccentSecondary,
        AccentTertiary,
        StatusSuccess,
        StatusWarning,
        StatusDanger
    };

    struct ShadowComponent {
        float x_offset;
        float y_offset;
        float blur_radius;
        float opacity;
    };

    enum class ElevationLevel : uint8_t {
        Level0 = 0,
        Level1,
        Level2,
        Level3,
        Level4,
        Level5,
        Level6
    };

    struct ElevationToken {
        ShadowComponent key;
        ShadowComponent ambient;
    };

    enum class MotionCurve {
        Standard,
        Emphasized,
        Spring
    };

    struct MotionTiming {
        float duration;
        float timing;
    };

    FluentIOSTheme(NVGcontext *ctx, ColorScheme scheme = ColorScheme::Light);

    void apply(ColorScheme scheme);
    void set_accent_color(const Color &accent);

    ColorScheme color_scheme() const { return m_scheme; }

    float corner_radius_small() const { return 6.f; }
    float corner_radius_medium() const { return 12.f; }
    float corner_radius_large() const { return 20.f; }

    const TypographyToken &typography(TypographyStyle style) const;
    float spacing(SpacingToken token) const;
    MotionTiming motion_timing(MotionCurve curve) const;
    const Color &color(SemanticColor token) const;
    const ElevationToken &elevation(ElevationLevel level) const;

protected:
    void bake();

private:
    void configure_palette(const Color &background,
                           const Color &secondary_background,
                           const Color &surface,
                           const Color &on_surface_primary,
                           const Color &on_surface_secondary,
                           const Color &separator);
    void update_semantic_tokens();
    void update_elevation_tokens();

    static constexpr size_t kSemanticColorCount =
        static_cast<size_t>(SemanticColor::StatusDanger) + 1u;
    static constexpr size_t kElevationCount =
        static_cast<size_t>(ElevationLevel::Level6) + 1u;

    std::array<Color, kSemanticColorCount> m_semantic_colors;
    std::array<ElevationToken, kElevationCount> m_elevation_tokens;
    ColorScheme m_scheme;
    Color m_background;
    Color m_surface;
    Color m_on_surface_primary;
    Color m_on_surface_secondary;
    Color m_accent;
    Color m_secondary_background;
    Color m_separator_color;
};

NAMESPACE_END(nanogui)
