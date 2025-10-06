/*
    src/fluent_icon_button.cpp -- Fluent Design Icon Button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_icon_button.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentIconButton::FluentIconButton(Widget *parent, int icon, Variant variant)
    : Button(parent, "", icon), m_variant(variant) {
    set_icon_position(IconPosition::LeftCentered);
}

Vector2i FluentIconButton::preferred_size_impl(NVGcontext *) const {
    // Fluent Design icon button: 40x40 or 48x48
    return Vector2i(40, 40);
}

void FluentIconButton::draw(NVGcontext *ctx) {
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float radius = std::min(w, h) * 0.5f;
    
    nvgSave(ctx);
    
    // Determine colors based on variant and state
    Color bg_color, icon_color;
    bool draw_background = false;
    bool draw_border = false;
    
    if (!m_enabled) {
        bg_color = Color(0.f, 0.f, 0.f, 0.12f);
        icon_color = Color(0.f, 0.f, 0.f, 0.38f);
        draw_background = (m_variant != Variant::Standard);
    } else {
        switch (m_variant) {
            case Variant::Standard:
                // No background, just icon
                icon_color = m_text_color.w() == 0 ? Color(0.4f, 0.4f, 0.4f, 1.f) : m_text_color;
                if (m_pushed || m_mouse_focus) {
                    bg_color = Color(0.f, 0.f, 0.f, m_pushed ? 0.12f : 0.08f);
                    draw_background = true;
                }
                break;
            
            case Variant::Filled:
                bg_color = m_pushed ? Color(0.2f, 0.6f, 0.9f, 1.f) : Color(0.25f, 0.7f, 1.f, 1.f);
                icon_color = Color(1.f, 1.f, 1.f, 1.f);
                draw_background = true;
                break;
            
            case Variant::Outlined:
                icon_color = m_text_color.w() == 0 ? Color(0.25f, 0.7f, 1.f, 1.f) : m_text_color;
                if (m_pushed || m_mouse_focus) {
                    bg_color = Color(0.25f, 0.7f, 1.f, m_pushed ? 0.12f : 0.08f);
                    draw_background = true;
                }
                draw_border = true;
                break;
            
            case Variant::Tonal:
                bg_color = m_pushed ? Color(0.9f, 0.95f, 1.f, 1.f) : Color(0.93f, 0.97f, 1.f, 1.f);
                icon_color = Color(0.25f, 0.7f, 1.f, 1.f);
                draw_background = true;
                break;
        }
    }
    
    // Draw background if needed
    if (draw_background) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius);
        nvgFillColor(ctx, nvgRGBAf(bg_color.r(), bg_color.g(), bg_color.b(), bg_color.w()));
        nvgFill(ctx);
    }
    
    // Draw border if needed
    if (draw_border) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, cy, radius);
        nvgStrokeWidth(ctx, 1.f);
        nvgStrokeColor(ctx, nvgRGBAf(0.7f, 0.7f, 0.7f, 1.f));
        nvgStroke(ctx);
    }
    
    // Draw icon
    if (m_icon) {
        auto icon = utf8(m_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(icon_color.r(), icon_color.g(), 
                                   icon_color.b(), icon_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cx, cy, icon.data(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
