/*
    src/m3_divider.cpp -- Material Design 3 Divider implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_divider.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Divider::M3Divider(Widget *parent, Orientation orientation)
    : Widget(parent), m_orientation(orientation) {
    if (m_orientation == Orientation::Horizontal) {
        set_fixed_height(1);
    } else {
        set_fixed_width(1);
    }
}

M3Theme *M3Divider::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

Vector2i M3Divider::preferred_size(NVGcontext *) const {
    if (m_orientation == Orientation::Horizontal) {
        return Vector2i(0, 1);
    } else {
        return Vector2i(1, 0);
    }
}

void M3Divider::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Draw divider line
    nvgBeginPath(ctx);
    if (m_orientation == Orientation::Horizontal) {
        nvgRect(ctx, x, y, w, 1);
    } else {
        nvgRect(ctx, x, y, 1, h);
    }
    nvgFillColor(ctx, theme->outline_variant());
    nvgFill(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
