/*
    nanogui/fluent_mica.h -- Fluent Design Mica Material

    Mica is an opaque material that incorporates the desktop wallpaper
    to create a subtle, personalized background texture.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once
#include <algorithm>
#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentMica fluent_mica.h nanogui/fluent_mica.h
 *
 * \brief Fluent Design Mica material effect
 *
 * Mica is an opaque material that creates a subtle texture by sampling
 * the desktop wallpaper. It provides:
 * - Desktop wallpaper sampling (simulated with texture)
 * - Tint color overlay
 * - Noise texture for depth
 * - Smooth transitions
 */
class NANOGUI_EXPORT FluentMica : public Widget {
public:
    /**
     * \brief Construct a Mica surface
     *
     * \param parent
     *     Parent widget
     */
    explicit FluentMica(Widget *parent);

    /// Get the tint color
    const Color &tint_color() const { return m_tint_color; }

    /// Set the tint color (default: theme background color)
    void set_tint_color(const Color &color) { m_tint_color = color; }

    /// Get the tint opacity (0.0 - 1.0)
    float tint_opacity() const { return m_tint_opacity; }

    /// Set the tint opacity (default: 0.5 for light, 0.8 for dark)
    void set_tint_opacity(float opacity) { m_tint_opacity = std::clamp(opacity, 0.f, 1.f); }

    /// Get the luminosity threshold (0.0 - 1.0)
    float luminosity_threshold() const { return m_luminosity_threshold; }

    /// Set the luminosity threshold for wallpaper sampling
    void set_luminosity_threshold(float threshold) { 
        m_luminosity_threshold = std::clamp(threshold, 0.f, 1.f); 
    }

    /// Draw the Mica material
    void draw(NVGcontext *ctx) override;

protected:
    /// Generate base texture if needed
    void ensure_base_texture(NVGcontext *ctx);

private:
    Color m_tint_color;
    float m_tint_opacity;
    float m_luminosity_threshold;
    int m_base_image;
};

NAMESPACE_END(nanogui)
