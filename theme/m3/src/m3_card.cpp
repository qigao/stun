/*
    src/m3_card.cpp -- Material Design 3 Card implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_card.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Card::M3Card(Widget *parent, Style style)
    : Widget(parent), m_style(style) {
}

M3Theme *M3Card::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Card::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Medium);

    nvgSave(ctx);

    // Draw shadow for Elevated style
    if (m_style == Style::Elevated) {
        float shadow_offset = static_cast<int>(m_elevation) * 2.0f;
        float shadow_blur = static_cast<int>(m_elevation) * 4.0f;
        
        NVGpaint shadow = nvgBoxGradient(ctx, x, y + shadow_offset, w, h,
                                         corner_radius, shadow_blur,
                                         nvgRGBAf(0, 0, 0, 0.15f),
                                         nvgRGBAf(0, 0, 0, 0));
        nvgBeginPath(ctx);
        nvgRect(ctx, x - shadow_blur, y - shadow_blur,
                w + 2 * shadow_blur, h + 2 * shadow_blur + shadow_offset);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgPathWinding(ctx, NVG_HOLE);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }

    // Draw background
    Color bg_color;
    switch (m_style) {
        case Style::Elevated:
            bg_color = theme->surface();
            break;
        case Style::Filled:
            bg_color = theme->surface_variant();
            break;
        case Style::Outlined:
            bg_color = theme->surface();
            break;
    }

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgFillColor(ctx, bg_color);
    nvgFill(ctx);

    // Draw elevation tint for Elevated style
    if (m_style == Style::Elevated) {
        Color tint = theme->elevation_tint(m_elevation);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);
    }

    // Draw outline for Outlined style
    if (m_style == Style::Outlined) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1, h - 1, corner_radius);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, theme->outline());
        nvgStroke(ctx);
    }

    nvgRestore(ctx);

    // Draw children
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
