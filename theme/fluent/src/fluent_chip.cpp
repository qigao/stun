/*
    src/fluent_chip.cpp -- Fluent Design Chip implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_chip.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentChip::FluentChip(Widget *parent, const std::string &label, Variant variant)
    : Widget(parent), m_label(label), m_variant(variant), 
      m_icon(0), m_selected(false), m_closeable(false), m_mouse_over_close(false) {
    
    // Input chips are closeable by default
    if (variant == Variant::Input)
        m_closeable = true;
}

Vector2i FluentChip::preferred_size_impl(NVGcontext *ctx) const {
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    
    float text_width = nvgTextBounds(ctx, 0, 0, m_label.c_str(), nullptr, nullptr);
    
    // Chip height: 32px (Fluent Design spec)
    int height = 32;
    
    // Calculate width: padding + icon + text + close + padding
    int width = 16; // Left padding
    
    if (m_icon)
        width += 18 + 8; // Icon + spacing
    
    width += (int)text_width;
    
    if (m_closeable)
        width += 8 + 18; // Spacing + close icon
    
    width += 16; // Right padding
    
    return Vector2i(width, height);
}

bool FluentChip::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    if (down) {
        // Check if clicking close button
        if (m_closeable) {
            float close_x = m_pos.x() + m_size.x() - 16.f - 18.f;
            float close_y = m_pos.y() + (m_size.y() - 18.f) * 0.5f;
            
            if (p.x() >= close_x && p.x() <= close_x + 18.f &&
                p.y() >= close_y && p.y() <= close_y + 18.f) {
                if (m_close_callback)
                    m_close_callback();
                return true;
            }
        }
        
        // Toggle selection for filter chips
        if (m_variant == Variant::Filter) {
            m_selected = !m_selected;
        }
        
        // Trigger callback
        if (m_callback)
            m_callback();
        
        return true;
    }
    
    return false;
}

void FluentChip::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    nvgSave(ctx);
    
    // Determine colors based on variant and state
    Color bg_color, text_color, border_color;
    bool has_border = false;
    
    if (!m_enabled) {
        bg_color = Color(0.f, 0.f, 0.f, 0.12f);
        text_color = Color(0.f, 0.f, 0.f, 0.38f);
    } else {
        switch (m_variant) {
            case Variant::Input:
                bg_color = Color(0.9f, 0.9f, 0.9f, 1.f);
                text_color = Color(0.2f, 0.2f, 0.2f, 1.f);
                break;
            
            case Variant::Filter:
                if (m_selected) {
                    bg_color = Color(0.9f, 0.95f, 1.f, 1.f);
                    text_color = Color(0.25f, 0.7f, 1.f, 1.f);
                    border_color = Color(0.25f, 0.7f, 1.f, 1.f);
                    has_border = true;
                } else {
                    bg_color = Color(0.f, 0.f, 0.f, 0.f);
                    text_color = Color(0.4f, 0.4f, 0.4f, 1.f);
                    border_color = Color(0.7f, 0.7f, 0.7f, 1.f);
                    has_border = true;
                }
                break;
            
            case Variant::Action:
                bg_color = Color(0.9f, 0.9f, 0.9f, 1.f);
                text_color = Color(0.2f, 0.2f, 0.2f, 1.f);
                break;
            
            case Variant::Suggestion:
                bg_color = Color(0.f, 0.f, 0.f, 0.f);
                text_color = Color(0.4f, 0.4f, 0.4f, 1.f);
                border_color = Color(0.7f, 0.7f, 0.7f, 1.f);
                has_border = true;
                break;
        }
    }
    
    // Draw chip background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, h * 0.5f);
    nvgFillColor(ctx, nvgRGBAf(bg_color.r(), bg_color.g(), bg_color.b(), bg_color.w()));
    nvgFill(ctx);
    
    // Draw border if needed
    if (has_border) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, h * 0.5f);
        nvgStrokeWidth(ctx, 1.f);
        nvgStrokeColor(ctx, nvgRGBAf(border_color.r(), border_color.g(), border_color.b(), border_color.w()));
        nvgStroke(ctx);
    }
    
    // Draw content
    float content_x = x + 16.f;
    float content_y = y + h * 0.5f;
    
    // Draw icon
    if (m_icon) {
        auto icon = utf8(m_icon);
        nvgFontSize(ctx, 18.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, content_y, icon.data(), nullptr);
        content_x += 18.f + 8.f;
    }
    
    // Draw checkmark for selected filter chips
    if (m_variant == Variant::Filter && m_selected) {
        auto check_icon = utf8(0xf00c); // FA_CHECK
        nvgFontSize(ctx, 18.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, content_y, check_icon.data(), nullptr);
        content_x += 18.f + 8.f;
    }
    
    // Draw label
    int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, content_x, content_y, m_label.c_str(), nullptr);
    
    // Draw close button
    if (m_closeable) {
        auto close_icon = utf8(0xf00d); // FA_TIMES
        float close_x = x + w - 16.f - 18.f;
        float close_y = content_y;
        
        nvgFontSize(ctx, 18.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, close_x, close_y, close_icon.data(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
