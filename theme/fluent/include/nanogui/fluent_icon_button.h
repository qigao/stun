/*
    nanogui/fluent_icon_button.h -- Fluent Design Icon Button widget

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class FluentIconButton fluent_icon_button.h nanogui/fluent_icon_button.h
 *
 * \brief Fluent Design Icon Button widget.
 *
 * Icon-only button with optional background. Commonly used in app bars and toolbars.
 * Variants: Standard, Filled, Outlined, Tonal
 */
class NANOGUI_EXPORT FluentIconButton : public Button {
public:
    enum class Variant {
        Standard,  // No background, just icon
        Filled,    // Filled background
        Outlined,  // Outlined with border
        Tonal      // Tonal background (lighter)
    };

    FluentIconButton(Widget *parent, int icon, Variant variant = Variant::Standard);

    Variant variant() const { return m_variant; }
    void set_variant(Variant variant) { m_variant = variant; }

    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

protected:
    Variant m_variant;
};

NAMESPACE_END(nanogui)
