/*
    src/fluent_card.cpp -- Fluent Design card implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_card.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentCard::FluentCard(Widget *parent, Elevation elevation)
    : Widget(parent), m_elevation(elevation) {
    m_background_color = Color(0, 0, 0, 0); // Use theme color by default
}

void FluentCard::set_elevation(Elevation elevation) {
    m_elevation = elevation;
}

void FluentCard::draw_shadow(NVGcontext *ctx, float x, float y, float w, float h) {
    if (m_elevation == Elevation::Level0)
        return;
    
    // Fluent Design elevation shadows
    // Higher elevation = larger shadow offset and blur
    float elevation_value = static_cast<float>(m_elevation);
    float shadow_offset = elevation_value * 1.5f;
    float shadow_blur = elevation_value * 3.f;
    float shadow_spread = elevation_value * 0.5f;
    
    // Ambient shadow (soft, large)
    NVGcolor ambient_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.08f + elevation_value * 0.02f);
    NVGcolor ambient_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint ambient_shadow = nvgBoxGradient(
        ctx, x, y + shadow_offset, w, h,
        m_corner_radius, shadow_blur,
        ambient_outer, ambient_inner
    );
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x - shadow_blur - shadow_spread, 
            y - shadow_blur - shadow_spread,
            w + 2 * (shadow_blur + shadow_spread),
            h + 2 * (shadow_blur + shadow_spread) + shadow_offset);
    nvgRoundedRect(ctx, x, y, w, h, m_corner_radius);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, ambient_shadow);
    nvgFill(ctx);
    
    // Key shadow (sharp, directional)
    float key_offset = elevation_value * 2.f;
    float key_blur = elevation_value * 2.f;
    
    NVGcolor key_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.12f + elevation_value * 0.03f);
    NVGcolor key_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint key_shadow = nvgBoxGradient(
        ctx, x, y + key_offset, w, h,
        m_corner_radius, key_blur,
        key_outer, key_inner
    );
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x - key_blur, y - key_blur,
            w + 2 * key_blur, h + 2 * key_blur + key_offset);
    nvgRoundedRect(ctx, x, y, w, h, m_corner_radius);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, key_shadow);
    nvgFill(ctx);
}

void FluentCard::draw(NVGcontext *ctx) {
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    nvgSave(ctx);
    
    // Draw shadows
    draw_shadow(ctx, x, y, w, h);
    
    // Draw card background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, m_corner_radius);
    
    Color bg = m_background_color.w() > 0 ? m_background_color 
                                          : m_theme->m_window_fill_focused;
    nvgFillColor(ctx, nvgRGBAf(bg.r(), bg.g(), bg.b(), bg.w()));
    nvgFill(ctx);
    
    nvgRestore(ctx);
    
    // Draw children
    Widget::draw(ctx);
}

void FluentCard::perform_layout(NVGcontext *ctx) {
    // FluentCard is just a visual container - let the layout manager handle positioning
    Widget::perform_layout(ctx);
}

NAMESPACE_END(nanogui)
