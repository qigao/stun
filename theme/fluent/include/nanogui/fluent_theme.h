/*
    nanogui/fluent_theme.h -- Fluent Design 3 theme for NanoGUI

    This theme implements Google's Fluent Design 3 (Material You) design
    system with support for light/dark modes and dynamic color palettes.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentTheme fluent_theme.h nanogui/fluent_theme.h
 *
 * \brief Fluent Design 3 theme implementation
 *
 * This theme provides a modern Fluent Design appearance with support for:
 * - Light and dark color schemes
 * - Dynamic accent colors
 * - Proper elevation and shadows
 * - Fluent Design typography
 * - Accessible color contrasts
 */
class NANOGUI_EXPORT FluentTheme : public Theme {
public:
    /// Color scheme palette (light or dark mode)
    enum class Palette {
        Light,  ///< Light color scheme
        Dark    ///< Dark color scheme
    };

    /// Corner radius styles for Fluent Design
    enum class CornerRadius {
        None,        ///< 0px - No rounding
        Small,       ///< 2px - Subtle rounding
        Medium,      ///< 4px - Standard controls
        Large,       ///< 8px - Cards, dialogs
        ExtraLarge,  ///< 12px - Large surfaces
        Circle       ///< Fully rounded (pill shape)
    };

    /**
     * \brief Construct a Fluent Design theme
     *
     * \param ctx
     *     NanoVG context for rendering
     *
     * \param palette
     *     Initial color palette (Light or Dark)
     */
    explicit FluentTheme(NVGcontext *ctx, Palette palette = Palette::Light);

    /**
     * \brief Apply a color palette (light or dark mode)
     *
     * \param palette
     *     The palette to apply
     */
    void apply_palette(Palette palette);

    /**
     * \brief Set the accent color
     *
     * The accent color is used for highlights, selected items, and interactive elements.
     *
     * \param accent
     *     The new accent color
     */
    void set_accent_color(const Color &accent);

    /// Get the current palette
    Palette palette() const { return m_palette; }

    /// Get the primary color (main brand color)
    const Color &primary_color() const { return m_primary; }

    /// Get the color for content on primary surfaces
    const Color &on_primary_color() const { return m_onPrimary; }

    /// Get the surface color (for cards, sheets, menus)
    const Color &surface_color() const { return m_surface; }

    /// Get the background color (for the main screen background)
    const Color &background_color() const { return m_background; }

    /// Get the color for content on surfaces
    const Color &on_surface_color() const { return m_onSurface; }

    /// Get the secondary text color (for less prominent text)
    const Color &secondary_text_color() const { return m_secondaryText; }

    /// Get the accent color (for highlights and interactive elements)
    const Color &accent_color() const { return m_accent; }

    /// Get corner radius for the specified style
    float corner_radius(CornerRadius style) const;

protected:
    /**
     * \brief Bake the theme colors
     *
     * This method computes derived colors (button gradients, borders, shadows)
     * based on the current palette and accent color.
     */
    void bake();
    
    /// Load Inter fonts (Segoe UI Variable alternative)
    void load_fonts(NVGcontext *ctx);

private:
    void configure_palette(const Color &primary, const Color &onPrimary,
                          const Color &surface, const Color &background,
                          const Color &onSurface, const Color &secondary,
                          const Color &accent);

    Palette m_palette;
    Color m_primary;
    Color m_onPrimary;
    Color m_surface;
    Color m_background;
    Color m_onSurface;
    Color m_secondaryText;
    Color m_accent;
};

NAMESPACE_END(nanogui)
