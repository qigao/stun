/*
    src/fluent_button.cpp -- Fluent Design button implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_button.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentButton::FluentButton(Widget *parent, const std::string &caption,
                               int icon, Style style)
    : Button(parent, caption, icon), m_style(style) {
    
    // Fluent Design buttons have specific padding
    set_fixed_height(40);
}

void FluentButton::set_style(Style style) {
    m_style = style;
}

Vector2i FluentButton::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans-bold");
    
    float tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    
    // Fluent Design button padding
    int padding_x = 24;
    int padding_y = 10;
    
    if (m_icon) {
        // Icon + text spacing
        float icon_width = font_size * 1.2f;
        tw += icon_width + 8;
    }
    
    return Vector2i(static_cast<int>(tw) + 2 * padding_x,
                    font_size + 2 * padding_y);
}

void FluentButton::draw_elevation(NVGcontext *ctx, float x, float y, 
                                    float w, float h) {
    if (m_style != Style::Elevated)
        return;
    
    float shadow_offset = m_elevation * 2.f;
    float shadow_blur = m_elevation * 4.f;
    
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.15f * m_elevation);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + shadow_offset, w, h,
                                     m_corner_radius, shadow_blur,
                                     shadow_outer, shadow_inner);
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x - shadow_blur, y - shadow_blur,
            w + 2 * shadow_blur, h + 2 * shadow_blur + shadow_offset);
    nvgRoundedRect(ctx, x, y, w, h, m_corner_radius);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);
}

void FluentButton::draw_ripple(NVGcontext *, float, float, float, float) {
    // Ripple effect would require animation support
    // Placeholder for future implementation
}

void FluentButton::draw(NVGcontext *ctx) {
    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    
    nvgSave(ctx);
    
    // Draw elevation shadow for Elevated style
    draw_elevation(ctx, x, y, w, h);
    
    // Determine colors based on style and state
    Color bg_color, text_color, border_color;
    bool draw_border = false;
    
    if (!m_enabled) {
        bg_color = Color(0.5f, 0.5f, 0.5f, 0.12f);
        text_color = Color(0.5f, 0.5f, 0.5f, 0.38f);
    } else {
        switch (m_style) {
            case Style::Text:
                bg_color = m_pushed ? Color(0.f, 0.f, 0.f, 0.08f) 
                                   : Color(0.f, 0.f, 0.f, 0.f);
                text_color = m_text_color.w() == 0 ? m_theme->m_text_color : m_text_color;
                break;
                
            case Style::Outlined:
                bg_color = m_pushed ? Color(0.f, 0.f, 0.f, 0.08f)
                                   : Color(0.f, 0.f, 0.f, 0.f);
                text_color = m_text_color.w() == 0 ? m_theme->m_text_color : m_text_color;
                border_color = m_theme->m_border_medium;
                draw_border = true;
                break;
                
            case Style::Filled:
            case Style::Elevated:
                if (m_background_color.w() != 0) {
                    bg_color = m_background_color;
                } else {
                    bg_color = m_pushed ? m_theme->m_button_gradient_top_pushed
                                       : m_theme->m_button_gradient_top_focused;
                }
                text_color = m_text_color.w() == 0 ? m_theme->m_icon_color : m_text_color;
                break;
                
            case Style::Tonal:
                // Tonal uses a tinted background
                bg_color = m_pushed ? Color(0.5f, 0.5f, 0.5f, 0.24f)
                                   : Color(0.5f, 0.5f, 0.5f, 0.12f);
                text_color = m_text_color.w() == 0 ? m_theme->m_text_color : m_text_color;
                break;
        }
    }
    
    // Draw background
    if (bg_color.w() > 0) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, m_corner_radius);
        nvgFillColor(ctx, nvgRGBAf(bg_color.r(), bg_color.g(), 
                                   bg_color.b(), bg_color.w()));
        nvgFill(ctx);
    }
    
    // Draw border for outlined style
    if (draw_border) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, m_corner_radius);
        nvgStrokeColor(ctx, nvgRGBAf(border_color.r(), border_color.g(),
                                     border_color.b(), border_color.w()));
        nvgStrokeWidth(ctx, 1.f);
        nvgStroke(ctx);
    }
    
    // Draw icon and text
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans-bold");
    
    float text_width = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    
    Vector2f center = Vector2f(x, y) + Vector2f(w, h) * 0.5f;
    Vector2f text_pos(center.x() - text_width * 0.5f, center.y());
    
    if (m_icon) {
        auto icon = utf8(m_icon);
        float icon_width = font_size * 1.2f;
        
        nvgFontSize(ctx, font_size * 1.2f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(),
                                   text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, center.x() - (text_width + icon_width) * 0.5f,
                center.y(), icon.data(), nullptr);
        
        text_pos.x() += icon_width * 0.5f + 4;
    }
    
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(),
                               text_color.b(), text_color.w()));
    nvgText(ctx, text_pos.x(), text_pos.y(), m_caption.c_str(), nullptr);
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
