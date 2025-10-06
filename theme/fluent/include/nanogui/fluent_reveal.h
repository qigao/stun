/*
    nanogui/fluent_reveal.h -- Fluent Design Reveal Highlight

    Reveal is an interactive lighting effect that follows the cursor
    and creates a subtle glow on hover.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include  <algorithm>
NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentReveal fluent_reveal.h nanogui/fluent_reveal.h
 *
 * \brief Fluent Design Reveal highlight effect
 *
 * Reveal adds an interactive lighting effect that:
 * - Follows the cursor position
 * - Creates a radial gradient glow
 * - Responds to hover and press states
 * - Provides visual feedback for interactive elements
 */
class NANOGUI_EXPORT FluentReveal {
public:
    /**
     * \brief Draw reveal effect on a widget
     *
     * \param ctx
     *     NanoVG context
     *
     * \param x, y, w, h
     *     Widget bounds
     *
     * \param cursor_x, cursor_y
     *     Current cursor position
     *
     * \param is_hovered
     *     Whether the widget is currently hovered
     *
     * \param is_pressed
     *     Whether the widget is currently pressed
     *
     * \param corner_radius
     *     Corner radius for the effect
     */
    static void draw_reveal(NVGcontext *ctx, 
                           float x, float y, float w, float h,
                           float cursor_x, float cursor_y,
                           bool is_hovered, bool is_pressed,
                           float corner_radius = 4.0f);

    /**
     * \brief Draw border reveal effect
     *
     * \param ctx
     *     NanoVG context
     *
     * \param x, y, w, h
     *     Widget bounds
     *
     * \param cursor_x, cursor_y
     *     Current cursor position
     *
     * \param is_hovered
     *     Whether the widget is currently hovered
     *
     * \param corner_radius
     *     Corner radius for the effect
     */
    static void draw_border_reveal(NVGcontext *ctx,
                                   float x, float y, float w, float h,
                                   float cursor_x, float cursor_y,
                                   bool is_hovered,
                                   float corner_radius = 4.0f);

    /// Get the reveal glow radius
    static float glow_radius() { return s_glow_radius; }

    /// Set the reveal glow radius (default: 100px)
    static void set_glow_radius(float radius) { s_glow_radius = radius; }

    /// Get the reveal intensity (0.0 - 1.0)
    static float intensity() { return s_intensity; }

    /// Set the reveal intensity (default: 0.25)
    static void set_intensity(float intensity) { 
        s_intensity = std::clamp(intensity, 0.f, 1.f); 
    }

private:
    static float s_glow_radius;
    static float s_intensity;
};

NAMESPACE_END(nanogui)
