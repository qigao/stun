/*
    nanogui/m3_icon_button.h -- Material Design 3 Icon Button

    Based on: https://m3.material.io/components/icon-buttons

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/button.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3IconButton : public Button {
public:
    enum class Variant {
        Standard,  ///< Standard icon button
        Filled,    ///< Filled background
        Tonal,     ///< Tonal background
        Outlined   ///< Outlined border
    };

    M3IconButton(Widget *parent, int icon, Variant variant = Variant::Standard);

    void set_variant(Variant variant) { m_variant = variant; }
    Variant variant() const { return m_variant; }

    void draw(NVGcontext *ctx) override;

protected:
    M3Theme *m3_theme() const;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

    Variant m_variant;
};

NAMESPACE_END(nanogui)
