/*
    src/m3_chip.cpp -- Material Design 3 Chip implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_chip.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Chip::M3Chip(Widget *parent, const std::string &label, int icon, Style style)
    : Button(parent, label, icon), m_style(style) {
    set_fixed_height(32); // M3 chip height
}

M3Theme *M3Chip::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Chip::draw_state_layer(NVGcontext *ctx, float x, float y, float w, float h) {
    M3Theme *theme = m3_theme();
    if (!theme || !m_enabled) return;

    float opacity = 0.0f;
    if (m_pushed) {
        opacity = 0.12f;
    } else if (m_mouse_focus) {
        opacity = 0.08f;
    }

    if (opacity > 0.0f) {
        Color base = m_selected ? theme->on_secondary_container() : theme->on_surface();
        Color state_color = theme->state_layer(base, opacity);
        
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, h * 0.5f);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }
}

Vector2i M3Chip::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = 14;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    
    int padding_x = 16;
    int icon_space = m_icon ? 24 : 0;
    int remove_space = m_removable ? 24 : 0;
    
    return Vector2i(static_cast<int>(tw) + 2 * padding_x + icon_space + remove_space, 32);
}

void M3Chip::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Button::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    float corner_radius = h * 0.5f; // Fully rounded

    nvgSave(ctx);

    // Determine colors based on style and state
    Color bg_color, text_color, border_color;
    bool draw_border = false;

    if (!m_enabled) {
        bg_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                        theme->on_surface().b(), 0.12f);
        text_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                          theme->on_surface().b(), 0.38f);
    } else {
        switch (m_style) {
            case Style::Assist:
                bg_color = Color(0, 0, 0, 0); // Transparent
                text_color = theme->on_surface();
                border_color = theme->outline();
                draw_border = true;
                break;
            
            case Style::Filter:
                if (m_selected) {
                    bg_color = theme->secondary_container();
                    text_color = theme->on_secondary_container();
                } else {
                    bg_color = Color(0, 0, 0, 0);
                    text_color = theme->on_surface_variant();
                    border_color = theme->outline();
                    draw_border = true;
                }
                break;
            
            case Style::Input:
                bg_color = Color(0, 0, 0, 0);
                text_color = theme->on_surface_variant();
                border_color = theme->outline();
                draw_border = true;
                break;
            
            case Style::Suggestion:
                bg_color = Color(0, 0, 0, 0);
                text_color = theme->on_surface_variant();
                border_color = theme->outline();
                draw_border = true;
                break;
        }
    }

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    nvgFillColor(ctx, bg_color);
    nvgFill(ctx);

    // Draw state layer
    draw_state_layer(ctx, x, y, w, h);

    // Draw border
    if (draw_border) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1, h - 1, corner_radius);
        nvgStrokeWidth(ctx, 1.0f);
        nvgStrokeColor(ctx, border_color);
        nvgStroke(ctx);
    }

    // Draw content
    float content_x = x + 16;
    float content_y = y + h * 0.5f;

    // Draw icon
    if (m_icon) {
        nvgFontSize(ctx, 18);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, content_x, content_y, utf8(m_icon).data(), nullptr);
        content_x += 24;
    }

    // Draw text
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, text_color);
    nvgText(ctx, content_x, content_y, m_caption.c_str(), nullptr);

    // Draw remove icon for Input chips
    if (m_removable && m_enabled) {
        float remove_x = x + w - 24;
        nvgFontSize(ctx, 18);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, remove_x, content_y, utf8(0xf00d).data(), nullptr); // FA close icon
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
