/*
    nanogui/m3_button.h -- Material Design 3 button component

    Implements M3 button variants with proper color roles, state layers,
    and shape system.

    Based on: https://m3.material.io/components/buttons

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Button m3_button.h nanogui/m3_button.h
 *
 * \brief Material Design 3 button component
 *
 * Implements M3 button styles with proper color roles and state layers:
 * - Filled: High emphasis, primary color background
 * - Outlined: Medium emphasis, outlined with primary color
 * - Text: Low emphasis, no background or border
 * - Elevated: High emphasis, with elevation tint
 * - Tonal: Medium emphasis, primary container background
 */
class NANOGUI_EXPORT M3Button : public Button {
public:
    /// M3 button style variants
    enum class Style {
        Filled,    ///< High emphasis, filled with primary color
        Outlined,  ///< Medium emphasis, outlined
        Text,      ///< Low emphasis, text only
        Elevated,  ///< High emphasis, with elevation
        Tonal      ///< Medium emphasis, tonal (container color)
    };

    /**
     * \brief Construct an M3 button
     *
     * \param parent Parent widget
     * \param caption Button text
     * \param icon Optional icon (Font Awesome code)
     * \param style Button style variant
     */
    M3Button(Widget *parent, const std::string &caption = "Button",
             int icon = 0, Style style = Style::Filled);

    /// Set button style
    void set_style(Style style);

    /// Get button style
    Style style() const { return m_style; }

    /// Draw the button
    void draw(NVGcontext *ctx) override;

protected:
    /// Calculate preferred size
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

    /// Get M3 theme (cast from base theme)
    M3Theme *m3_theme() const;

    /// Get background color for current style and state
    Color get_background_color() const;

    /// Get text color for current style and state
    Color get_text_color() const;

    /// Get border color for outlined style
    Color get_border_color() const;

    /// Draw state layer (hover/focus/press overlay)
    void draw_state_layer(NVGcontext *ctx, float x, float y, float w, float h);

    Style m_style;
};

NAMESPACE_END(nanogui)
