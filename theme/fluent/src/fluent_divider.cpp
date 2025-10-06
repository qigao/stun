/*
    src/fluent_divider.cpp -- Fluent Design Divider implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_divider.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentDivider::FluentDivider(Widget *parent, Orientation orientation)
    : Widget(parent), m_orientation(orientation), 
      m_color(0.f, 0.f, 0.f, 0.12f), m_thickness(1) {
}

Vector2i FluentDivider::preferred_size_impl(NVGcontext *) const {
    if (m_orientation == Orientation::Horizontal) {
        return Vector2i(0, m_thickness); // Width will be filled by layout
    } else {
        return Vector2i(m_thickness, 0); // Height will be filled by layout
    }
}

void FluentDivider::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    nvgSave(ctx);
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
    nvgFill(ctx);
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
