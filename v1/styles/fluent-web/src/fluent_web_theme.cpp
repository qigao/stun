#include <nanogui/fluent_web_theme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#define FLUENT_WEB_INCLUDE_TOKEN_DATA
#include <nanogui/detail/fluent_web_token_data.inc>

NAMESPACE_BEGIN(nanogui)

namespace {

constexpr size_t kColorCount =
    static_cast<size_t>(FluentWebTheme::ColorToken::Count);
constexpr size_t kSpacingCount =
    static_cast<size_t>(FluentWebTheme::SpaceToken::Count);
constexpr size_t kRadiusCount =
    static_cast<size_t>(FluentWebTheme::RadiusToken::Count);
constexpr size_t kTypographyCount =
    static_cast<size_t>(FluentWebTheme::TypographyToken::Count);

Color make_color(const float rgba[4]) {
    return Color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

std::array<Color, kColorCount> make_defaults(bool dark) {
    std::array<Color, kColorCount> values;
    for (size_t i = 0; i < kColorCount; ++i) {
        values[i] = make_color(dark ? kFluentWebColorTokenData[i].dark
                                    : kFluentWebColorTokenData[i].light);
    }
    return values;
}

const std::array<Color, kColorCount> &light_defaults() {
    static const std::array<Color, kColorCount> defaults = make_defaults(false);
    return defaults;
}

const std::array<Color, kColorCount> &dark_defaults() {
    static const std::array<Color, kColorCount> defaults = make_defaults(true);
    return defaults;
}

constexpr std::array<float, kSpacingCount> kSpacingValues = {
    0.f, 2.f, 4.f, 6.f, 8.f, 10.f, 12.f, 16.f, 20.f, 24.f, 32.f};

constexpr std::array<float, kRadiusCount> kRadiusValues = {
    0.f, 2.f, 4.f, 6.f, 8.f, 10000.f};

constexpr std::array<FluentWebTheme::TypographySpec, kTypographyCount>
    kTypographyValues = {{
        {68.f, 92.f, 600, 0.f},  // Display
        {40.f, 52.f, 600, 0.f},  // LargeTitle
        {32.f, 40.f, 600, 0.f},  // Title1
        {28.f, 36.f, 600, 0.f},  // Title2
        {24.f, 32.f, 600, 0.f},  // Title3
        {20.f, 28.f, 600, 0.f},  // Subtitle1
        {16.f, 22.f, 600, 0.f},  // Subtitle2
        {16.f, 22.f, 700, 0.f},  // Subtitle2Stronger
        {14.f, 20.f, 400, 0.f},  // Body1
        {14.f, 20.f, 600, 0.f},  // Body1Strong
        {14.f, 20.f, 700, 0.f},  // Body1Stronger
        {16.f, 22.f, 400, 0.f},  // Body2
        {12.f, 16.f, 400, 0.f},  // Caption1
        {12.f, 16.f, 600, 0.f},  // Caption1Strong
        {12.f, 16.f, 700, 0.f},  // Caption1Stronger
        {10.f, 14.f, 400, 0.f},  // Caption2
        {10.f, 14.f, 600, 0.f},  // Caption2Strong
    }};

constexpr std::array<int, 16> kBrandStops = {
    10, 20, 30, 40, 50, 60, 70, 80,
    90, 100, 110, 120, 130, 140, 150, 160};

constexpr std::array<float, 16> kBrandTowardBlack = {
    0.7322f, 0.6154f, 0.5053f, 0.3854f, 0.2634f, 0.1605f, 0.0446f, 0.f,
    0.f,     0.f,     0.f,     0.f,     0.f,     0.f,     0.f,     0.f};

constexpr std::array<float, 16> kBrandTowardWhite = {
    0.f,      0.f,      0.f,      0.f,      0.f,      0.f,      0.f,      0.f,
    0.2603f,  0.4740f,  0.5410f,  0.6074f,  0.6997f,  0.7776f,  0.8469f,  0.9299f};

Color mix_linear(const Color &a, const Color &b, float factor) {
    factor = std::clamp(factor, 0.f, 1.f);
    float inv = 1.f - factor;
    return Color(
        a.r() * inv + b.r() * factor,
        a.g() * inv + b.g() * factor,
        a.b() * inv + b.b() * factor,
        a.a() * inv + b.a() * factor
    );
}

size_t brand_index_for(int stop) {
    auto it = std::find(kBrandStops.begin(), kBrandStops.end(), stop);
    if (it == kBrandStops.end())
        return 0;
    return static_cast<size_t>(std::distance(kBrandStops.begin(), it));
}

Color make_srgb(float r, float g, float b) {
    return Color(r, g, b, 1.f);
}

std::array<Color, 16> default_brand_ramp() {
    return {
        make_srgb(0.02352941f, 0.09019608f, 0.14117647f),
        make_srgb(0.03137255f, 0.13725490f, 0.21960784f),
        make_srgb(0.03921569f, 0.18039216f, 0.29019608f),
        make_srgb(0.04705882f, 0.23137255f, 0.36862745f),
        make_srgb(0.05490196f, 0.27843137f, 0.45882353f),
        make_srgb(0.05882353f, 0.32941176f, 0.54901961f),
        make_srgb(0.06666667f, 0.36862745f, 0.63921569f),
        make_srgb(0.05882353f, 0.42352941f, 0.74117647f),
        make_srgb(0.15686275f, 0.52549020f, 0.87058824f),
        make_srgb(0.27843137f, 0.61960784f, 0.96078431f),
        make_srgb(0.38431373f, 0.67058824f, 0.96078431f),
        make_srgb(0.46666667f, 0.71764706f, 0.96862745f),
        make_srgb(0.58823529f, 0.77647059f, 0.98039216f),
        make_srgb(0.70588235f, 0.83921569f, 0.98039216f),
        make_srgb(0.81176471f, 0.89411765f, 0.98039216f),
        make_srgb(0.92156863f, 0.95294118f, 0.98823529f)
    };
}

FluentWebTheme::BrandRamp make_brand_ramp_from_base(const Color &base) {
    FluentWebTheme::BrandRamp ramp;
    Color clamped = Color(std::clamp(base.r(), 0.f, 1.f),
                          std::clamp(base.g(), 0.f, 1.f),
                          std::clamp(base.b(), 0.f, 1.f),
                          std::clamp(base.a(), 0.f, 1.f));
    Color white = Color(1.f, 1.f, 1.f, clamped.a());
    Color black = Color(0.f, 0.f, 0.f, clamped.a());
    for (size_t i = 0; i < kBrandStops.size(); ++i) {
        if (kBrandStops[i] == 80) {
            ramp.stops[i] = clamped;
            continue;
        }
        if (kBrandStops[i] > 80) {
            ramp.stops[i] = mix_linear(clamped, white, kBrandTowardWhite[i]);
        } else {
            ramp.stops[i] = mix_linear(clamped, black, kBrandTowardBlack[i]);
        }
    }
    return ramp;
}

float opacity_of(Color c) {
    return std::clamp(c.a(), 0.f, 1.f);
}

} // namespace

FluentWebTheme::FluentWebTheme(NVGcontext *ctx, Mode mode)
    : Theme(ctx),
      m_mode(mode),
      m_brand_accent(Color(0.05882353f, 0.42352942f, 0.74117649f, 1.f)) {
    m_light_defaults = light_defaults();
    m_dark_defaults = dark_defaults();
    m_colors.fill(Color());
    m_brand_ramp.stops = default_brand_ramp();

    std::copy(kSpacingValues.begin(), kSpacingValues.end(), m_spacing.begin());
    std::copy(kRadiusValues.begin(), kRadiusValues.end(), m_radius.begin());
    std::copy(kTypographyValues.begin(), kTypographyValues.end(),
              m_typography.begin());

    m_elevation.fill({{0.f, 0.f, 0.f, 0.f, 0.f},
                      {0.f, 0.f, 0.f, 0.f, 0.f}});

    apply(mode);
}

void FluentWebTheme::apply(Mode mode) {
    m_mode = mode;
    if (mode == Mode::Light) {
        m_colors = m_light_defaults;
    } else {
        m_colors = m_dark_defaults;
    }

    apply_brand_overrides();
    update_elevation_tokens();
    bake();
}

void FluentWebTheme::set_brand_color(const Color &brand) {
    m_brand_accent = brand;
    m_brand_ramp = make_brand_ramp_from_base(brand);
    apply_brand_overrides();
    bake();
}

void FluentWebTheme::set_brand_variants(const BrandRamp &variants) {
    m_brand_ramp = variants;
    size_t idx = brand_index_for(80);
    if (idx < m_brand_ramp.stops.size())
        m_brand_accent = m_brand_ramp.stops[idx];
    apply_brand_overrides();
    bake();
}

const Color &FluentWebTheme::color(ColorToken token) const {
    return m_colors[index(token)];
}

const FluentWebTheme::TypographySpec &
FluentWebTheme::typography(TypographyToken token) const {
    return m_typography[static_cast<size_t>(token)];
}

float FluentWebTheme::spacing(SpaceToken token) const {
    return m_spacing[static_cast<size_t>(token)];
}

float FluentWebTheme::corner_radius(RadiusToken token) const {
    return m_radius[static_cast<size_t>(token)];
}

const FluentWebTheme::ElevationLayers &
FluentWebTheme::shadow(Elevation level) const {
    return m_elevation[static_cast<size_t>(level)];
}

void FluentWebTheme::bake() {
    const Color &background =
        color(ColorToken::colorNeutralBackground1);
    const Color &surface =
        color(ColorToken::colorNeutralBackground2);
    const Color &surface_alt =
        color(ColorToken::colorNeutralBackground3);
    const Color &primary_text =
        color(ColorToken::colorNeutralForeground1);
    const Color &secondary_text =
        color(ColorToken::colorNeutralForeground2);
    const Color &disabled_text =
        color(ColorToken::colorNeutralForegroundDisabled);
    const Color &brand_background =
        color(ColorToken::colorBrandBackground);
    const Color &brand_hover =
        color(ColorToken::colorBrandBackgroundHover);
    const Color &brand_pressed =
        color(ColorToken::colorBrandBackgroundPressed);
    const Color &brand_selected =
        color(ColorToken::colorBrandBackgroundSelected);
    const Color &border_light =
        color(ColorToken::colorNeutralStroke2);
    const Color &border_medium =
        color(ColorToken::colorNeutralStroke1);
    const Color &focus_primary =
        color(ColorToken::colorStrokeFocus2);
    const Color &logo_color =
        color(ColorToken::colorBrandForeground1);

    m_standard_font_size = static_cast<int>(
        typography(TypographyToken::Body1).font_size);
    m_button_font_size = static_cast<int>(
        typography(TypographyToken::Body1).font_size);
    m_text_box_font_size = m_button_font_size;

    m_window_corner_radius =
        static_cast<int>(corner_radius(RadiusToken::Large));
    m_button_corner_radius =
        static_cast<int>(corner_radius(RadiusToken::Medium));

    m_window_header_height = 36;
    m_window_drop_shadow_size = 18;
    m_tab_border_width = 1.0f;
    m_tab_inner_margin = 10;
    m_tab_min_button_width = 64;
    m_tab_max_button_width = 240;
    m_tab_control_width = 72;
    m_tab_button_horizontal_padding = 12;
    m_tab_button_vertical_padding = 6;

    m_text_color = primary_text;
    m_disabled_text_color = Color(disabled_text.r(), disabled_text.g(),
                                  disabled_text.b(), 0.6f);
    m_text_color_shadow = Color(0.f, 0.f, 0.f,
                                m_mode == Mode::Light ? 0.f : 0.25f);

    m_window_fill_focused = surface;
    m_window_fill_unfocused = surface_alt;
    m_window_title_focused = primary_text;
    m_window_title_unfocused =
        Color(primary_text.r(), primary_text.g(), primary_text.b(), 0.75f);

    m_window_header_gradient_top = brand_hover;
    m_window_header_gradient_bot = brand_pressed;
    m_window_header_sep_top = border_light;
    m_window_header_sep_bot = border_medium;

    m_button_gradient_top_focused = brand_background;
    m_button_gradient_bot_focused = brand_pressed;
    m_button_gradient_top_unfocused = brand_hover;
    m_button_gradient_bot_unfocused = brand_hover;
    m_button_gradient_top_pushed = brand_selected;
    m_button_gradient_bot_pushed = brand_selected;

    m_window_popup = surface_alt;
    m_window_popup_transparent =
        Color(surface_alt.r(), surface_alt.g(), surface_alt.b(), 0.f);

    m_border_light = border_light;
    m_border_medium = border_medium;
    m_border_dark = adjust_luminance(border_medium, -0.2f);

    const Color &shadow_key =
        color(ColorToken::colorNeutralShadowKey);
    m_drop_shadow = Color(0.f, 0.f, 0.f, opacity_of(shadow_key));
    m_transparent = Color(0.f, 0.f, 0.f, 0.f);

    m_icon_color = logo_color;

    const Color &focus_glow =
        color(ColorToken::colorStrokeFocus1);
    m_window_header_sep_top = mix(border_light, focus_glow, 0.25f);
    (void)secondary_text;
}

void FluentWebTheme::apply_brand_overrides() {
    auto set = [&](ColorToken token, const Color &value) {
        m_colors[index(token)] = value;
    };

    auto brand = [&](int stop) -> Color {
        size_t idx = brand_index_for(stop);
        if (idx >= m_brand_ramp.stops.size())
            idx = m_brand_ramp.stops.size() - 1;
        return m_brand_ramp.stops[idx];
    };

    const Color white = Color(1.f, 1.f, 1.f, 1.f);

    set(ColorToken::colorBrandBackgroundInverted, white);
    set(ColorToken::colorBrandBackgroundInvertedHover, brand(160));
    set(ColorToken::colorBrandBackgroundInvertedPressed, brand(140));
    set(ColorToken::colorBrandBackgroundInvertedSelected, brand(150));

    set(ColorToken::colorBrandBackgroundPressed, brand(40));
    set(ColorToken::colorBrandBackgroundSelected, brand(60));
    set(ColorToken::colorBrandBackgroundStatic, brand(80));
    set(ColorToken::colorBrandBackground3Static, brand(60));
    set(ColorToken::colorBrandBackground4Static, brand(40));

    set(ColorToken::colorBrandForegroundOnLight, brand(80));
    set(ColorToken::colorBrandForegroundOnLightHover, brand(70));
    set(ColorToken::colorBrandForegroundOnLightPressed, brand(50));
    set(ColorToken::colorBrandForegroundOnLightSelected, brand(60));

    if (m_mode == Mode::Light) {
        set(ColorToken::colorNeutralForeground2BrandHover, brand(80));
        set(ColorToken::colorNeutralForeground2BrandPressed, brand(70));
        set(ColorToken::colorNeutralForeground2BrandSelected, brand(80));

        set(ColorToken::colorNeutralForeground3BrandHover, brand(80));
        set(ColorToken::colorNeutralForeground3BrandPressed, brand(70));
        set(ColorToken::colorNeutralForeground3BrandSelected, brand(80));

        set(ColorToken::colorBrandForegroundLink, brand(70));
        set(ColorToken::colorBrandForegroundLinkHover, brand(60));
        set(ColorToken::colorBrandForegroundLinkPressed, brand(40));
        set(ColorToken::colorBrandForegroundLinkSelected, brand(70));

        set(ColorToken::colorCompoundBrandForeground1, brand(80));
        set(ColorToken::colorCompoundBrandForeground1Hover, brand(70));
        set(ColorToken::colorCompoundBrandForeground1Pressed, brand(60));

        set(ColorToken::colorBrandForeground1, brand(80));
        set(ColorToken::colorBrandForeground2, brand(70));
        set(ColorToken::colorBrandForeground2Hover, brand(60));
        set(ColorToken::colorBrandForeground2Pressed, brand(30));

        set(ColorToken::colorBrandForegroundInverted, brand(100));
        set(ColorToken::colorBrandForegroundInvertedHover, brand(110));
        set(ColorToken::colorBrandForegroundInvertedPressed, brand(100));

        set(ColorToken::colorBrandBackground, brand(80));
        set(ColorToken::colorBrandBackgroundHover, brand(70));

        set(ColorToken::colorCompoundBrandBackground, brand(80));
        set(ColorToken::colorCompoundBrandBackgroundHover, brand(70));
        set(ColorToken::colorCompoundBrandBackgroundPressed, brand(60));

        set(ColorToken::colorBrandBackground2, brand(160));
        set(ColorToken::colorBrandBackground2Hover, brand(150));
        set(ColorToken::colorBrandBackground2Pressed, brand(130));

        set(ColorToken::colorNeutralStrokeAccessibleSelected, brand(80));
        set(ColorToken::colorBrandStroke1, brand(80));
        set(ColorToken::colorBrandStroke2, brand(140));
        set(ColorToken::colorBrandStroke2Hover, brand(120));
        set(ColorToken::colorBrandStroke2Pressed, brand(80));
        set(ColorToken::colorBrandStroke2Contrast, brand(140));

        set(ColorToken::colorCompoundBrandStroke, brand(80));
        set(ColorToken::colorCompoundBrandStrokeHover, brand(70));
        set(ColorToken::colorCompoundBrandStrokePressed, brand(60));
    } else {
        set(ColorToken::colorNeutralForeground2BrandHover, brand(100));
        set(ColorToken::colorNeutralForeground2BrandPressed, brand(90));
        set(ColorToken::colorNeutralForeground2BrandSelected, brand(100));

        set(ColorToken::colorNeutralForeground3BrandHover, brand(100));
        set(ColorToken::colorNeutralForeground3BrandPressed, brand(90));
        set(ColorToken::colorNeutralForeground3BrandSelected, brand(100));

        set(ColorToken::colorBrandForegroundLink, brand(100));
        set(ColorToken::colorBrandForegroundLinkHover, brand(110));
        set(ColorToken::colorBrandForegroundLinkPressed, brand(90));
        set(ColorToken::colorBrandForegroundLinkSelected, brand(100));

        set(ColorToken::colorCompoundBrandForeground1, brand(100));
        set(ColorToken::colorCompoundBrandForeground1Hover, brand(110));
        set(ColorToken::colorCompoundBrandForeground1Pressed, brand(90));

        set(ColorToken::colorBrandForeground1, brand(100));
        set(ColorToken::colorBrandForeground2, brand(110));
        set(ColorToken::colorBrandForeground2Hover, brand(130));
        set(ColorToken::colorBrandForeground2Pressed, brand(160));

        set(ColorToken::colorBrandForegroundInverted, brand(80));
        set(ColorToken::colorBrandForegroundInvertedHover, brand(70));
        set(ColorToken::colorBrandForegroundInvertedPressed, brand(60));

        set(ColorToken::colorBrandBackground, brand(70));
        set(ColorToken::colorBrandBackgroundHover, brand(80));

        set(ColorToken::colorCompoundBrandBackground, brand(100));
        set(ColorToken::colorCompoundBrandBackgroundHover, brand(110));
        set(ColorToken::colorCompoundBrandBackgroundPressed, brand(90));

        set(ColorToken::colorBrandBackground2, brand(20));
        set(ColorToken::colorBrandBackground2Hover, brand(40));
        set(ColorToken::colorBrandBackground2Pressed, brand(10));

        set(ColorToken::colorNeutralStrokeAccessibleSelected, brand(100));
        set(ColorToken::colorBrandStroke1, brand(100));
        set(ColorToken::colorBrandStroke2, brand(50));
        set(ColorToken::colorBrandStroke2Hover, brand(50));
        set(ColorToken::colorBrandStroke2Pressed, brand(30));
        set(ColorToken::colorBrandStroke2Contrast, brand(50));

        set(ColorToken::colorCompoundBrandStroke, brand(100));
        set(ColorToken::colorCompoundBrandStrokeHover, brand(110));
        set(ColorToken::colorCompoundBrandStrokePressed, brand(90));
    }
}

void FluentWebTheme::update_elevation_tokens() {
    const Color &ambient_color =
        color(ColorToken::colorNeutralShadowAmbient);
    const Color &key_color =
        color(ColorToken::colorNeutralShadowKey);

    auto make_component = [](float x, float y, float blur, float opacity) {
        return ShadowSpec{x, y, blur, 0.f, opacity};
    };

    auto set = [&](Elevation level, float key_y, float key_blur,
                   float ambient_blur, float key_opacity, float ambient_opacity) {
        m_elevation[static_cast<size_t>(level)] = {
            make_component(0.f, key_y, key_blur, key_opacity),
            make_component(0.f, 0.f, ambient_blur, ambient_opacity)};
    };

    float key_opacity = opacity_of(key_color);
    float ambient_opacity = opacity_of(ambient_color);

    set(Elevation::Level1, 1.f, 2.f, 2.f, key_opacity, ambient_opacity);
    set(Elevation::Level2, 2.f, 4.f, 2.f, key_opacity, ambient_opacity);
    set(Elevation::Level3, 4.f, 8.f, 2.f, key_opacity, ambient_opacity);
    set(Elevation::Level4, 8.f, 16.f, 2.f, key_opacity, ambient_opacity);
    set(Elevation::Level5, 14.f, 28.f, 8.f, key_opacity, ambient_opacity);
    set(Elevation::Level6, 32.f, 64.f, 8.f, key_opacity, ambient_opacity);
}

Color FluentWebTheme::mix(const Color &a, const Color &b, float factor) const {
    factor = std::clamp(factor, 0.f, 1.f);
    float inv = 1.f - factor;
    return Color(
        a.r() * inv + b.r() * factor,
        a.g() * inv + b.g() * factor,
        a.b() * inv + b.b() * factor,
        a.a() * inv + b.a() * factor
    );
}

Color FluentWebTheme::adjust_luminance(const Color &color,
                                       float amount) const {
    amount = std::clamp(amount, -1.f, 1.f);
    Color endpoint = amount >= 0.f ? Color(1.f, 1.f, 1.f, color.a())
                                   : Color(0.f, 0.f, 0.f, color.a());
    return mix(color, endpoint, std::abs(amount));
}

NAMESPACE_END(nanogui)
