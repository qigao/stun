/*
    nanogui/fluent_acrylic.h -- Fluent Design Acrylic Material

    Acrylic is a translucent material with blur effects, used for
    surfaces like navigation panes, command bars, and app backgrounds.

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once
#include <algorithm>
#include <nanogui/widget.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentAcrylic fluent_acrylic.h nanogui/fluent_acrylic.h
 *
 * \brief Fluent Design Acrylic material effect
 *
 * Acrylic provides a translucent, blurred background effect that creates
 * depth and hierarchy. It consists of:
 * - Background blur
 * - Tint color overlay
 * - Noise texture
 * - Exclusion blend for luminosity
 */
class NANOGUI_EXPORT FluentAcrylic : public Widget {
public:
    /// Acrylic material types
    enum class Type {
        Background,  ///< Background acrylic (80% opacity, more blur)
        InApp        ///< In-app acrylic (70% opacity, less blur)
    };

    /**
     * \brief Construct an acrylic surface
     *
     * \param parent
     *     Parent widget
     *
     * \param type
     *     Acrylic material type
     */
    FluentAcrylic(Widget *parent, Type type = Type::InApp);

    /// Get the acrylic type
    Type type() const { return m_type; }

    /// Set the acrylic type
    void set_type(Type type) { m_type = type; }

    /// Get the tint color
    const Color &tint_color() const { return m_tint_color; }

    /// Set the tint color (default: theme surface color)
    void set_tint_color(const Color &color) { m_tint_color = color; }

    /// Get the tint opacity (0.0 - 1.0)
    float tint_opacity() const { return m_tint_opacity; }

    /// Set the tint opacity
    void set_tint_opacity(float opacity) { m_tint_opacity = std::clamp(opacity, 0.f, 1.f); }

    /// Get the blur radius in pixels
    float blur_radius() const { return m_blur_radius; }

    /// Set the blur radius
    void set_blur_radius(float radius) { m_blur_radius = std::max(0.f, radius); }

    /// Get the noise opacity (0.0 - 1.0)
    float noise_opacity() const { return m_noise_opacity; }

    /// Set the noise opacity
    void set_noise_opacity(float opacity) { m_noise_opacity = std::clamp(opacity, 0.f, 1.f); }

    /// Draw the acrylic material
    void draw(NVGcontext *ctx) override;

protected:
    /// Initialize default values based on type
    void initialize_defaults();

    /// Generate noise texture if needed
    void ensure_noise_texture(NVGcontext *ctx);

private:
    Type m_type;
    Color m_tint_color;
    float m_tint_opacity;
    float m_blur_radius;
    float m_noise_opacity;
    int m_noise_image;
};

NAMESPACE_END(nanogui)
