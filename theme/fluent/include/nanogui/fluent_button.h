/*
    nanogui/fluent_button.h -- Fluent Design button variants

    Fluent Design buttons with proper elevation, ripple effects,
    and multiple style variants (text, outlined, filled, elevated).

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentButton fluent_button.h nanogui/fluent_button.h
 *
 * \brief Fluent Design button with multiple style variants
 *
 * Implements Fluent Design 3 button styles:
 * - Text: Low emphasis, no background
 * - Outlined: Medium emphasis, with border
 * - Filled: High emphasis, with background
 * - Elevated: High emphasis, with shadow
 * - Tonal: Medium emphasis, with tinted background
 */
class NANOGUI_EXPORT FluentButton : public Button {
public:
    /// Button style variants following Fluent Design 3
    enum class Style {
        Text,      ///< Text-only button (low emphasis)
        Outlined,  ///< Button with border (medium emphasis)
        Filled,    ///< Button with solid background (high emphasis)
        Elevated,  ///< Button with shadow (high emphasis)
        Tonal      ///< Button with tinted background (medium emphasis)
    };

    /**
     * \brief Construct a Fluent Design button
     *
     * \param parent
     *     Parent widget
     *
     * \param caption
     *     Button text
     *
     * \param icon
     *     Optional icon (Font Awesome code)
     *
     * \param style
     *     Button style variant
     */
    FluentButton(Widget *parent, const std::string &caption = "Button",
                   int icon = 0, Style style = Style::Filled);

    /// Set the button style
    void set_style(Style style);

    /// Get the current button style
    Style style() const { return m_style; }

    /// Set corner radius (default: 20 for full rounding)
    void set_corner_radius(float radius) { m_corner_radius = radius; }

    /// Get corner radius
    float corner_radius() const { return m_corner_radius; }

    /// Set elevation (shadow depth) for Elevated style
    void set_elevation(float elevation) { m_elevation = elevation; }

    /// Get elevation
    float elevation() const { return m_elevation; }

    /// Draw the button
    void draw(NVGcontext *ctx) override;

protected:
    /// Preferred size calculation
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    /// Draw ripple effect (for future animation support)
    void draw_ripple(NVGcontext *ctx, float x, float y, float w, float h);

    /// Draw elevation shadow
    void draw_elevation(NVGcontext *ctx, float x, float y, float w, float h);

    Style m_style;
    float m_corner_radius = 20.f;
    float m_elevation = 1.f;
};

NAMESPACE_END(nanogui)
