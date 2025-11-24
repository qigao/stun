/*
    nanogui/apple_effects.h -- Apple-style visual effects

    Approximates Apple's vibrancy and material effects using NanoVG.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/common.h>
#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleMaterial apple_effects.h nanogui/apple_effects.h
 *
 * \brief Apple-style material effects (approximated)
 */
class NANOGUI_EXPORT AppleMaterial {
public:
    /// Material types
    enum class Type {
        Regular,        ///< Regular material
        Thick,          ///< Thick material
        Thin,           ///< Thin material
        Ultrathin,      ///< Ultra thin material
        Chrome,         ///< Chrome material (reflective)
        Titlebar,       ///< Titlebar material
        Selection,      ///< Selection material
        Menu,           ///< Menu material
        Popover,        ///< Popover material
        Sidebar,        ///< Sidebar material
        HeaderView,     ///< Header view material
        Sheet,          ///< Sheet material
        WindowBackground, ///< Window background material
        HUD,            ///< HUD material
        FullScreen,     ///< Full screen material
        ToolTip,        ///< Tooltip material
        ContentBackground, ///< Content background material
        UnderWindowBackground, ///< Under window background material
        UnderPageBackground   ///< Under page background material
    };

    /// Blending mode
    enum class BlendMode {
        BehindWindow,   ///< Blend with content behind window
        WithinWindow    ///< Blend with content within window
    };

    AppleMaterial(Type type = Type::Regular);

    /// Set material type
    void set_type(Type type) { m_type = type; }
    Type type() const { return m_type; }

    /// Set blend mode
    void set_blend_mode(BlendMode mode) { m_blend_mode = mode; }
    BlendMode blend_mode() const { return m_blend_mode; }

    /// Set material opacity (0.0 to 1.0)
    void set_opacity(float opacity) { m_opacity = std::max(0.0f, std::min(1.0f, opacity)); }
    float opacity() const { return m_opacity; }

    /// Apply material effect to a region
    void apply(NVGcontext *ctx, float x, float y, float w, float h, float corner_radius = 0.0f);

    /// Get material color (approximation)
    Color get_color(bool dark_mode) const;

protected:
    Type m_type;
    BlendMode m_blend_mode;
    float m_opacity;
};

/**
 * \class AppleVibrancy apple_effects.h nanogui/apple_effects.h
 *
 * \brief Apple-style vibrancy effects (approximated)
 */
class NANOGUI_EXPORT AppleVibrancy {
public:
    /// Vibrancy style
    enum class Style {
        Light,          ///< Light vibrancy
        Dark,           ///< Dark vibrancy
        Titlebar,       ///< Titlebar vibrancy
        Selection,      ///< Selection vibrancy
        Menu,           ///< Menu vibrancy
        Popover,        ///< Popover vibrancy
        Sidebar,        ///< Sidebar vibrancy
        MediumLight,    ///< Medium light vibrancy
        UltraDark,      ///< Ultra dark vibrancy
        HeaderView,     ///< Header view vibrancy
        Sheet,          ///< Sheet vibrancy
        WindowBackground, ///< Window background vibrancy
        HUD,            ///< HUD vibrancy
        FullScreenUI,   ///< Full screen UI vibrancy
        ToolTip,        ///< Tooltip vibrancy
        ContentBackground, ///< Content background vibrancy
        UnderWindowBackground, ///< Under window background vibrancy
        UnderPageBackground   ///< Under page background vibrancy
    };

    AppleVibrancy(Style style = Style::Light);

    /// Set vibrancy style
    void set_style(Style style) { m_style = style; }
    Style style() const { return m_style; }

    /// Set vibrancy intensity (0.0 to 1.0)
    void set_intensity(float intensity) { m_intensity = std::max(0.0f, std::min(1.0f, intensity)); }
    float intensity() const { return m_intensity; }

    /// Apply vibrancy effect to a region
    void apply(NVGcontext *ctx, float x, float y, float w, float h, float corner_radius = 0.0f);

    /// Get vibrancy color (approximation)
    Color get_color() const;

protected:
    Style m_style;
    float m_intensity;
};

/**
 * \class AppleBlur apple_effects.h nanogui/apple_effects.h
 *
 * \brief Blur effect (approximated with gradients)
 */
class NANOGUI_EXPORT AppleBlur {
public:
    /// Blur style
    enum class Style {
        Light,          ///< Light blur
        ExtraLight,     ///< Extra light blur
        Dark,           ///< Dark blur
        Regular,        ///< Regular blur
        Prominent       ///< Prominent blur
    };

    AppleBlur(Style style = Style::Regular, float radius = 10.0f);

    /// Set blur style
    void set_style(Style style) { m_style = style; }
    Style style() const { return m_style; }

    /// Set blur radius
    void set_radius(float radius) { m_radius = std::max(0.0f, radius); }
    float radius() const { return m_radius; }

    /// Apply blur effect (approximated with box shadow)
    void apply(NVGcontext *ctx, float x, float y, float w, float h, float corner_radius = 0.0f);

protected:
    Style m_style;
    float m_radius;
};

/**
 * \class AppleShadow apple_effects.h nanogui/apple_effects.h
 *
 * \brief Apple-style shadow effects
 */
class NANOGUI_EXPORT AppleShadow {
public:
    /// Shadow elevation levels
    enum class Elevation {
        Level0 = 0,     ///< No shadow
        Level1 = 1,     ///< 1dp elevation
        Level2 = 2,     ///< 2dp elevation
        Level3 = 3,     ///< 4dp elevation
        Level4 = 4,     ///< 8dp elevation
        Level5 = 5      ///< 16dp elevation
    };

    AppleShadow(Elevation elevation = Elevation::Level2);

    /// Set shadow elevation
    void set_elevation(Elevation elevation) { m_elevation = elevation; }
    Elevation elevation() const { return m_elevation; }

    /// Set shadow color
    void set_color(const Color &color) { m_color = color; }
    const Color &color() const { return m_color; }

    /// Apply shadow effect
    void apply(NVGcontext *ctx, float x, float y, float w, float h, float corner_radius = 0.0f);

protected:
    Elevation m_elevation;
    Color m_color;
};

NAMESPACE_END(nanogui)
