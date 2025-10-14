#include <nanogui/fluent_ios_theme.h>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

Color lighten(const Color &color, float amount) {
    float r = std::clamp(color.r() + (1.f - color.r()) * amount, 0.f, 1.f);
    float g = std::clamp(color.g() + (1.f - color.g()) * amount, 0.f, 1.f);
    float b = std::clamp(color.b() + (1.f - color.b()) * amount, 0.f, 1.f);
    return Color(r, g, b, color.a());
}

Color darken(const Color &color, float amount) {
    float r = std::clamp(color.r() * (1.f - amount), 0.f, 1.f);
    float g = std::clamp(color.g() * (1.f - amount), 0.f, 1.f);
    float b = std::clamp(color.b() * (1.f - amount), 0.f, 1.f);
    return Color(r, g, b, color.a());
}

} // namespace

FluentIOSTheme::FluentIOSTheme(NVGcontext *ctx, ColorScheme scheme)
    : Theme(ctx), m_scheme(scheme) {
    m_semantic_colors.fill(Color());
    m_elevation_tokens.fill({0.f, 0.f, 0.f, 0.f});
    apply(scheme);
}

void FluentIOSTheme::apply(ColorScheme scheme) {
    m_scheme = scheme;

    if (scheme == ColorScheme::Light) {
        configure_palette(
            Color(1.f, 1.f, 1.f, 1.f),             // background
            Color(0.949f, 0.953f, 0.976f, 1.f),    // secondary background (system grouped)
            Color(0.964f, 0.968f, 0.984f, 1.f),    // surface
            Color(0.078f, 0.078f, 0.082f, 0.94f),  // on surface primary
            Color(0.078f, 0.078f, 0.082f, 0.6f),   // on surface secondary
            Color(0.827f, 0.831f, 0.847f, 1.f));   // separator
        m_accent = Color(0.0f, 0.478f, 1.0f, 1.0f); // iOS system blue
    } else {
        configure_palette(
            Color(0.071f, 0.078f, 0.086f, 1.f),    // background
            Color(0.118f, 0.125f, 0.141f, 1.f),    // secondary background
            Color(0.153f, 0.157f, 0.176f, 1.f),    // surface
            Color(0.973f, 0.973f, 0.984f, 0.94f),  // on surface primary
            Color(0.973f, 0.973f, 0.984f, 0.7f),   // on surface secondary
            Color(0.353f, 0.365f, 0.388f, 1.f));   // separator
        m_accent = Color(0.392f, 0.675f, 1.0f, 1.0f); // lighter blue for dark mode
    }

    update_semantic_tokens();
    update_elevation_tokens();
    bake();
}

void FluentIOSTheme::set_accent_color(const Color &accent) {
    m_accent = accent;
    update_semantic_tokens();
    bake();
}

void FluentIOSTheme::configure_palette(const Color &background,
                                       const Color &secondary_background,
                                       const Color &surface,
                                       const Color &on_surface_primary,
                                       const Color &on_surface_secondary,
                                       const Color &separator) {
    m_background = background;
    m_secondary_background = secondary_background;
    m_surface = surface;
    m_on_surface_primary = on_surface_primary;
    m_on_surface_secondary = on_surface_secondary;
    m_separator_color = separator;
}

const Color &FluentIOSTheme::color(SemanticColor token) const {
    return m_semantic_colors[static_cast<size_t>(token)];
}

const FluentIOSTheme::ElevationToken &FluentIOSTheme::elevation(ElevationLevel level) const {
    return m_elevation_tokens[static_cast<size_t>(level)];
}

void FluentIOSTheme::update_semantic_tokens() {
    auto assign = [&](SemanticColor token, const Color &value) {
        m_semantic_colors[static_cast<size_t>(token)] = value;
    };

    assign(SemanticColor::BackgroundPrimary, m_background);
    assign(SemanticColor::BackgroundSecondary, m_secondary_background);
    assign(SemanticColor::Surface, m_surface);

    Color elevated = m_scheme == ColorScheme::Light
        ? darken(m_surface, 0.12f)
        : lighten(m_surface, 0.14f);
    assign(SemanticColor::SurfaceElevated, elevated);

    assign(SemanticColor::Separator, m_separator_color);
    assign(SemanticColor::TextPrimary, m_on_surface_primary);
    assign(SemanticColor::TextSecondary, m_on_surface_secondary);

    float muted_alpha = std::clamp(m_on_surface_secondary.w() * 0.8f, 0.f, 1.f);
    assign(SemanticColor::TextMuted,
           Color(m_on_surface_secondary.r(),
                 m_on_surface_secondary.g(),
                 m_on_surface_secondary.b(),
                 muted_alpha));

    assign(SemanticColor::AccentPrimary, m_accent);
    assign(SemanticColor::AccentSecondary,
           m_scheme == ColorScheme::Light ? lighten(m_accent, 0.18f)
                                          : lighten(m_accent, 0.08f));
    assign(SemanticColor::AccentTertiary,
           m_scheme == ColorScheme::Light ? lighten(m_accent, 0.32f)
                                          : lighten(m_accent, 0.15f));

    Color success_light(0.086f, 0.620f, 0.275f, 1.f);
    Color success_dark(0.282f, 0.859f, 0.514f, 1.f);
    assign(SemanticColor::StatusSuccess,
           m_scheme == ColorScheme::Light ? success_light : success_dark);

    Color warning_light(0.980f, 0.745f, 0.102f, 1.f);
    Color warning_dark(0.992f, 0.827f, 0.333f, 1.f);
    assign(SemanticColor::StatusWarning,
           m_scheme == ColorScheme::Light ? warning_light : warning_dark);

    Color danger_light(0.941f, 0.286f, 0.259f, 1.f);
    Color danger_dark(0.992f, 0.376f, 0.353f, 1.f);
    assign(SemanticColor::StatusDanger,
           m_scheme == ColorScheme::Light ? danger_light : danger_dark);
}

void FluentIOSTheme::update_elevation_tokens() {
    auto layer = [](float x_offset,
                    float y_offset,
                    float blur_radius,
                    float opacity) -> ShadowComponent {
        return {x_offset, y_offset, blur_radius, opacity};
    };
    auto assign = [&](ElevationLevel level,
                      const ShadowComponent &key,
                      const ShadowComponent &ambient) {
        m_elevation_tokens[static_cast<size_t>(level)] = {key, ambient};
    };

    ShadowComponent none = layer(0.f, 0.f, 0.f, 0.f);
    assign(ElevationLevel::Level0, none, none);
    assign(ElevationLevel::Level1,
           layer(0.f, 1.f, 2.f, 0.14f),
           layer(0.f, 0.f, 2.f, 0.14f));
    assign(ElevationLevel::Level2,
           layer(0.f, 2.f, 4.f, 0.14f),
           layer(0.f, 0.f, 4.f, 0.14f));
    assign(ElevationLevel::Level3,
           layer(0.f, 4.f, 8.f, 0.14f),
           layer(0.f, 0.f, 8.f, 0.14f));
    assign(ElevationLevel::Level4,
           layer(0.f, 8.f, 16.f, 0.14f),
           layer(0.f, 0.f, 16.f, 0.14f));
    assign(ElevationLevel::Level5,
           layer(0.f, 14.f, 28.f, 0.24f),
           layer(0.f, 0.f, 8.f, 0.20f));
    assign(ElevationLevel::Level6,
           layer(0.f, 32.f, 64.f, 0.24f),
           layer(0.f, 0.f, 8.f, 0.20f));
}

const FluentIOSTheme::TypographyToken &FluentIOSTheme::typography(TypographyStyle style) const {
    static const TypographyToken tokens[] = {
        {34, 41, 1}, // LargeTitle
        {28, 34, 0}, // Title1
        {22, 28, 0}, // Title2
        {20, 25, 0}, // Title3
        {17, 22, 1}, // Headline
        {17, 22, 0}, // Body
        {16, 21, 0}, // Callout
        {15, 20, 0}, // Subheadline
        {13, 18, 0}, // Footnote
        {12, 16, 0}, // Caption1
        {11, 13, 0}  // Caption2
    };
    return tokens[static_cast<int>(style)];
}

float FluentIOSTheme::spacing(SpacingToken token) const {
    switch (token) {
        case SpacingToken::XSmall: return 8.f;
        case SpacingToken::Small:  return 12.f;
        case SpacingToken::Medium: return 16.f;
        case SpacingToken::Large:  return 20.f;
        case SpacingToken::XLarge: return 28.f;
        default: return 16.f;
    }
}

FluentIOSTheme::MotionTiming FluentIOSTheme::motion_timing(MotionCurve curve) const {
    switch (curve) {
        case MotionCurve::Standard:
            return {0.33f, 0.18f};
        case MotionCurve::Emphasized:
            return {0.43f, 0.30f};
        case MotionCurve::Spring:
        default:
            return {0.5f, 0.55f};
    }
}

void FluentIOSTheme::bake() {
    m_window_header_height = 0;
    m_window_drop_shadow_size = 12;
    m_button_font_size = 17;
    m_text_box_font_size = 17;
    m_standard_font_size = 16;

    m_window_fill_focused = m_background;
    m_window_fill_unfocused = m_secondary_background;
    m_window_popup = m_surface;
    m_window_popup_transparent = Color(m_surface.r(), m_surface.g(), m_surface.b(), 0.f);

    m_text_color = m_on_surface_primary;
    m_disabled_text_color = Color(m_on_surface_primary.r(), m_on_surface_primary.g(), m_on_surface_primary.b(), 0.35f);
    m_text_color_shadow = Color(0.f, 0.f, 0.f, 0.f);

    m_window_title_focused = m_on_surface_primary;
    m_window_title_unfocused = Color(m_on_surface_primary.r(), m_on_surface_primary.g(), m_on_surface_primary.b(), 0.76f);

    m_border_medium = m_separator_color;
    m_border_light = lighten(m_separator_color, 0.35f);
    m_border_dark = darken(m_separator_color, 0.35f);

    m_button_gradient_top_focused = m_accent;
    m_button_gradient_bot_focused = darken(m_accent, 0.12f);
    m_button_gradient_top_unfocused = lighten(m_accent, 0.08f);
    m_button_gradient_bot_unfocused = m_button_gradient_top_unfocused;
    m_button_gradient_top_pushed = darken(m_accent, 0.2f);
    m_button_gradient_bot_pushed = m_button_gradient_top_pushed;

    m_drop_shadow = Color(0.f, 0.f, 0.f, m_scheme == ColorScheme::Light ? 0.18f : 0.35f);
    m_transparent = Color(0.f, 0.f, 0.f, 0.f);

    m_icon_color = m_accent;
}

NAMESPACE_END(nanogui)
