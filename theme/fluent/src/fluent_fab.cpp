/*
    src/fluent_fab.cpp -- Fluent Design Floating Action Button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_fab.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentFAB::FluentFAB(Widget *parent, int icon, Size size)
    : Button(parent, "", icon), m_fab_size(size) {
    
    // FABs are always circular and icon-only
    set_icon_position(IconPosition::LeftCentered);
    
    // Set size based on variant
    set_fab_size(size);
    
    // FABs have high elevation by default
    m_elevation = 3.f;
}

void FluentFAB::set_fab_size(Size size) {
    m_fab_size = size;
    
    int fab_size;
    switch (size) {
        case Size::Small:
            fab_size = 40;
            break;
        case Size::Regular:
            fab_size = 56;
            break;
        case Size::Large:
            fab_size = 96;
            break;
    }
    
    set_fixed_size(Vector2i(fab_size, fab_size));
}

Vector2i FluentFAB::preferred_size_impl(NVGcontext *) const {
    switch (m_fab_size) {
        case Size::Small:
            return Vector2i(40, 40);
        case Size::Regular:
            return Vector2i(56, 56);
        case Size::Large:
            return Vector2i(96, 96);
    }
    return Vector2i(56, 56);
}

void FluentFAB::draw_shadow(NVGcontext *ctx, float cx, float cy, float radius) {
    // Fluent Design FAB shadows
    float shadow_offset = m_elevation * 1.5f;
    float shadow_blur = m_elevation * 3.f;
    
    // Ambient shadow (soft, large)
    NVGcolor ambient_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.12f + m_elevation * 0.02f);
    NVGcolor ambient_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint ambient_shadow = nvgRadialGradient(
        ctx, cx, cy + shadow_offset, radius * 0.5f, radius + shadow_blur,
        ambient_outer, ambient_inner
    );
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy + shadow_offset, radius + shadow_blur);
    nvgFillPaint(ctx, ambient_shadow);
    nvgFill(ctx);
    
    // Key shadow (sharp, directional)
    float key_offset = m_elevation * 2.f;
    float key_blur = m_elevation * 2.f;
    
    NVGcolor key_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.15f + m_elevation * 0.03f);
    NVGcolor key_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint key_shadow = nvgRadialGradient(
        ctx, cx, cy + key_offset, radius * 0.3f, radius + key_blur,
        key_outer, key_inner
    );
    
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy + key_offset, radius + key_blur);
    nvgFillPaint(ctx, key_shadow);
    nvgFill(ctx);
}

void FluentFAB::draw(NVGcontext *ctx) {
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float radius = std::min(w, h) * 0.5f;
    
    nvgSave(ctx);
    
    // Draw shadow
    draw_shadow(ctx, cx, cy, radius);
    
    // Determine colors based on state
    Color bg_color, icon_color;
    
    if (!m_enabled) {
        bg_color = Color(0.5f, 0.5f, 0.5f, 0.38f);
        icon_color = Color(0.5f, 0.5f, 0.5f, 0.38f);
    } else {
        if (m_background_color.w() != 0) {
            bg_color = m_background_color;
        } else {
            // Use theme primary color or default
            bg_color = m_pushed ? Color(0.2f, 0.6f, 0.9f, 1.f)  // Darker blue when pressed
                               : Color(0.25f, 0.7f, 1.f, 1.f);   // Default blue
        }
        
        icon_color = m_text_color.w() == 0 ? Color(1.f, 1.f, 1.f, 1.f) : m_text_color;
    }
    
    // Draw FAB background (circle)
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    nvgFillColor(ctx, nvgRGBAf(bg_color.r(), bg_color.g(), bg_color.b(), bg_color.w()));
    nvgFill(ctx);
    
    // Draw icon
    if (m_icon) {
        auto icon = utf8(m_icon);
        
        // Icon size based on FAB size
        float icon_size;
        switch (m_fab_size) {
            case Size::Small:
                icon_size = 18.f;
                break;
            case Size::Regular:
                icon_size = 24.f;
                break;
            case Size::Large:
                icon_size = 36.f;
                break;
        }
        
        nvgFontSize(ctx, icon_size);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(icon_color.r(), icon_color.g(), 
                                   icon_color.b(), icon_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cx, cy, icon.data(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
