/*
    src/m3_theme.cpp -- Material Design 3 theme implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_theme.h>
#include <cmath>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

// Clamp value between min and max
template<typename T>
T clamp(T value, T min, T max) {
    return std::max(min, std::min(max, value));
}

// Linear interpolation
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

M3Theme::M3Theme(NVGcontext *ctx, const Color &seed_color, Scheme scheme)
    : Theme(ctx), m_scheme(scheme), m_seed_color(seed_color) {
    generate_palettes(seed_color);
    apply_scheme(scheme);
}

void M3Theme::apply_scheme(Scheme scheme) {
    m_scheme = scheme;

    if (scheme == Scheme::Light) {
        // Light scheme - M3 spec tones
        m_primary = get_tone(m_primary_palette, 40);
        m_on_primary = get_tone(m_primary_palette, 100);
        m_primary_container = get_tone(m_primary_palette, 90);
        m_on_primary_container = get_tone(m_primary_palette, 10);

        m_secondary = get_tone(m_secondary_palette, 40);
        m_on_secondary = get_tone(m_secondary_palette, 100);
        m_secondary_container = get_tone(m_secondary_palette, 90);
        m_on_secondary_container = get_tone(m_secondary_palette, 10);

        m_tertiary = get_tone(m_tertiary_palette, 40);
        m_on_tertiary = get_tone(m_tertiary_palette, 100);
        m_tertiary_container = get_tone(m_tertiary_palette, 90);
        m_on_tertiary_container = get_tone(m_tertiary_palette, 10);

        m_error = get_tone(m_error_palette, 40);
        m_on_error = get_tone(m_error_palette, 100);
        m_error_container = get_tone(m_error_palette, 90);
        m_on_error_container = get_tone(m_error_palette, 10);

        m_background = get_tone(m_neutral_palette, 99);
        m_on_background = get_tone(m_neutral_palette, 10);

        m_surface = get_tone(m_neutral_palette, 99);
        m_on_surface = get_tone(m_neutral_palette, 10);
        m_surface_variant = get_tone(m_neutral_variant_palette, 90);
        m_on_surface_variant = get_tone(m_neutral_variant_palette, 30);

        m_outline = get_tone(m_neutral_variant_palette, 50);
        m_outline_variant = get_tone(m_neutral_variant_palette, 80);

        m_inverse_surface = get_tone(m_neutral_palette, 20);
        m_inverse_on_surface = get_tone(m_neutral_palette, 95);
        m_inverse_primary = get_tone(m_primary_palette, 80);

        m_shadow = Color(0.0f, 0.0f, 0.0f, 1.0f);
        m_scrim = Color(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        // Dark scheme - M3 spec tones
        m_primary = get_tone(m_primary_palette, 80);
        m_on_primary = get_tone(m_primary_palette, 20);
        m_primary_container = get_tone(m_primary_palette, 30);
        m_on_primary_container = get_tone(m_primary_palette, 90);

        m_secondary = get_tone(m_secondary_palette, 80);
        m_on_secondary = get_tone(m_secondary_palette, 20);
        m_secondary_container = get_tone(m_secondary_palette, 30);
        m_on_secondary_container = get_tone(m_secondary_palette, 90);

        m_tertiary = get_tone(m_tertiary_palette, 80);
        m_on_tertiary = get_tone(m_tertiary_palette, 20);
        m_tertiary_container = get_tone(m_tertiary_palette, 30);
        m_on_tertiary_container = get_tone(m_tertiary_palette, 90);

        m_error = get_tone(m_error_palette, 80);
        m_on_error = get_tone(m_error_palette, 20);
        m_error_container = get_tone(m_error_palette, 30);
        m_on_error_container = get_tone(m_error_palette, 90);

        m_background = get_tone(m_neutral_palette, 10);
        m_on_background = get_tone(m_neutral_palette, 90);

        m_surface = get_tone(m_neutral_palette, 10);
        m_on_surface = get_tone(m_neutral_palette, 90);
        m_surface_variant = get_tone(m_neutral_variant_palette, 30);
        m_on_surface_variant = get_tone(m_neutral_variant_palette, 80);

        m_outline = get_tone(m_neutral_variant_palette, 60);
        m_outline_variant = get_tone(m_neutral_variant_palette, 30);

        m_inverse_surface = get_tone(m_neutral_palette, 90);
        m_inverse_on_surface = get_tone(m_neutral_palette, 20);
        m_inverse_primary = get_tone(m_primary_palette, 40);

        m_shadow = Color(0.0f, 0.0f, 0.0f, 1.0f);
        m_scrim = Color(0.0f, 0.0f, 0.0f, 1.0f);
    }

    bake();
}

void M3Theme::set_seed_color(const Color &seed_color) {
    m_seed_color = seed_color;
    generate_palettes(seed_color);
    apply_scheme(m_scheme);
}

void M3Theme::generate_palettes(const Color &seed_color) {
    // Generate primary palette from seed
    m_primary_palette = generate_tonal_palette(seed_color);

    // Generate secondary palette (rotated hue)
    HCT seed_hct = rgb_to_hct(seed_color);
    seed_hct.h = std::fmod(seed_hct.h + 60.0f, 360.0f);
    m_secondary_palette = generate_tonal_palette(hct_to_rgb(seed_hct));

    // Generate tertiary palette (complementary)
    seed_hct.h = std::fmod(seed_hct.h + 120.0f, 360.0f);
    m_tertiary_palette = generate_tonal_palette(hct_to_rgb(seed_hct));

    // Neutral palette (desaturated)
    seed_hct = rgb_to_hct(seed_color);
    seed_hct.c = 4.0f; // Low chroma
    m_neutral_palette = generate_tonal_palette(hct_to_rgb(seed_hct));

    // Neutral variant palette
    seed_hct.c = 8.0f; // Slightly higher chroma
    m_neutral_variant_palette = generate_tonal_palette(hct_to_rgb(seed_hct));

    // Error palette (fixed red)
    m_error_palette = generate_tonal_palette(Color(0.7f, 0.1f, 0.1f, 1.0f));
}

M3Theme::TonalPalette M3Theme::generate_tonal_palette(const Color &source) {
    TonalPalette palette;
    HCT hct = rgb_to_hct(source);

    // Generate tones: 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 99, 100
    const int tones[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 99, 100};
    for (size_t i = 0; i < palette.tones.size(); ++i) {
        hct.t = static_cast<float>(tones[i]);
        palette.tones[i] = hct_to_rgb(hct);
    }

    return palette;
}

Color M3Theme::get_tone(const TonalPalette &palette, int tone) const {
    // Map tone to index
    const int tones[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 99, 100};
    for (size_t i = 0; i < palette.tones.size(); ++i) {
        if (tones[i] == tone) {
            return palette.tones[i];
        }
    }
    return palette.tones[6]; // Default to tone 50
}

M3Theme::HCT M3Theme::rgb_to_hct(const Color &rgb) const {
    // Simplified RGB to HCT conversion
    // In production, use proper CAM16/HCT conversion
    float r = rgb.r(), g = rgb.g(), b = rgb.b();
    
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    HCT hct;
    hct.t = max_c * 100.0f; // Tone (lightness)

    if (delta < 0.0001f) {
        hct.h = 0.0f;
        hct.c = 0.0f;
    } else {
        // Hue
        if (max_c == r) {
            hct.h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        } else if (max_c == g) {
            hct.h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            hct.h = 60.0f * ((r - g) / delta + 4.0f);
        }
        if (hct.h < 0.0f) hct.h += 360.0f;

        // Chroma (simplified)
        hct.c = delta * 100.0f;
    }

    return hct;
}

Color M3Theme::hct_to_rgb(const HCT &hct) const {
    // Simplified HCT to RGB conversion
    float t = hct.t / 100.0f;
    float c = hct.c / 100.0f;
    float h = hct.h;

    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = t - c / 2.0f;

    float r, g, b;
    if (h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    return Color(
        clamp(r + m, 0.0f, 1.0f),
        clamp(g + m, 0.0f, 1.0f),
        clamp(b + m, 0.0f, 1.0f),
        1.0f
    );
}

float M3Theme::corner_radius(ShapeFamily family) const {
    switch (family) {
        case ShapeFamily::None: return 0.0f;
        case ShapeFamily::ExtraSmall: return 4.0f;
        case ShapeFamily::Small: return 8.0f;
        case ShapeFamily::Medium: return 12.0f;
        case ShapeFamily::Large: return 16.0f;
        case ShapeFamily::ExtraLarge: return 28.0f;
        default: return 12.0f;
    }
}

Color M3Theme::elevation_tint(Elevation level) const {
    // M3 uses color overlays for elevation
    float opacity = 0.0f;
    switch (level) {
        case Elevation::Level0: opacity = 0.0f; break;
        case Elevation::Level1: opacity = 0.05f; break;
        case Elevation::Level2: opacity = 0.08f; break;
        case Elevation::Level3: opacity = 0.11f; break;
        case Elevation::Level4: opacity = 0.12f; break;
        case Elevation::Level5: opacity = 0.14f; break;
    }

    Color tint = m_scheme == Scheme::Light ? m_primary : m_primary;
    return Color(tint.r(), tint.g(), tint.b(), opacity);
}

Color M3Theme::state_layer(const Color &base, float opacity) const {
    return Color(base.r(), base.g(), base.b(), opacity);
}

void M3Theme::bake() {
    // Map M3 colors to base Theme properties for compatibility
    m_text_color = m_on_surface;
    m_disabled_text_color = Color(m_on_surface.r(), m_on_surface.g(), 
                                   m_on_surface.b(), 0.38f);
    m_text_color_shadow = Color(0.0f, 0.0f, 0.0f, 0.0f);

    // Button colors
    m_button_gradient_top_focused = m_button_gradient_bot_focused = m_primary;
    m_button_gradient_top_unfocused = m_button_gradient_bot_unfocused = m_primary;
    m_button_gradient_top_pushed = m_button_gradient_bot_pushed = m_primary;

    // Window colors
    m_window_fill_unfocused = m_window_fill_focused = m_surface;
    m_window_title_unfocused = Color(m_on_surface.r(), m_on_surface.g(), 
                                      m_on_surface.b(), 0.6f);
    m_window_title_focused = m_on_surface;
    m_window_header_gradient_top = m_surface_variant;
    m_window_header_gradient_bot = m_surface_variant;
    m_window_header_sep_top = m_outline_variant;
    m_window_header_sep_bot = m_outline;

    // Popup colors
    m_window_popup = m_surface;
    m_window_popup_transparent = Color(m_surface.r(), m_surface.g(), 
                                       m_surface.b(), 0.0f);

    // Border colors
    m_border_light = m_outline_variant;
    m_border_medium = m_outline;
    m_border_dark = m_outline;

    // Shadow and transparency
    m_drop_shadow = Color(m_shadow.r(), m_shadow.g(), m_shadow.b(), 0.15f);
    m_transparent = Color(0.0f, 0.0f, 0.0f, 0.0f);

    // Icon color
    m_icon_color = m_on_primary;
}

NAMESPACE_END(nanogui)
