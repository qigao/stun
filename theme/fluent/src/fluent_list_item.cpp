/*
    src/fluent_list_item.cpp -- Fluent Design List Item implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_list_item.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentListItem::FluentListItem(Widget *parent, const std::string &primary_text, Type type)
    : Widget(parent), m_primary_text(primary_text), m_leading_icon(0), 
      m_trailing_icon(0), m_type(type), m_mouse_over(false) {
}

Vector2i FluentListItem::preferred_size_impl(NVGcontext *) const {
    int height = 56; // Default to OneLine
    switch (m_type) {
        case Type::OneLine:
            height = 56;
            break;
        case Type::TwoLine:
            height = 72;
            break;
        case Type::ThreeLine:
            height = 88;
            break;
    }
    
    return Vector2i(0, height); // Width will be filled by parent
}

bool FluentListItem::mouse_enter_event(const Vector2i &p, bool enter) {
    m_mouse_over = enter;
    return Widget::mouse_enter_event(p, enter);
}

bool FluentListItem::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1 || !down)
        return false;
    
    if (m_callback)
        m_callback();
    
    return true;
}

void FluentListItem::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(Widget::size().x());
    float h = static_cast<float>(Widget::size().y());
    
    nvgSave(ctx);
    
    // Draw hover background
    if (m_mouse_over && m_enabled) {
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, h);
        nvgFillColor(ctx, nvgRGBAf(0.f, 0.f, 0.f, 0.04f));
        nvgFill(ctx);
    }
    
    float content_x = x + 16.f;
    float content_y = y + h * 0.5f;
    
    // Draw leading icon
    if (m_leading_icon) {
        auto icon = utf8(m_leading_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, content_y, icon.data(), nullptr);
        content_x += 40.f + 16.f;
    }
    
    // Draw text content
    if (m_type == Type::OneLine) {
        // Single line
        nvgFontSize(ctx, 16.f);
        nvgFontFace(ctx, "sans");
        nvgFillColor(ctx, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, content_x, content_y, m_primary_text.c_str(), nullptr);
    } else {
        // Multi-line
        float text_y = y + 20.f;
        
        // Primary text
        nvgFontSize(ctx, 16.f);
        nvgFontFace(ctx, "sans");
        nvgFillColor(ctx, nvgRGBAf(0.2f, 0.2f, 0.2f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(ctx, content_x, text_y, m_primary_text.c_str(), nullptr);
        
        // Secondary text
        if (!m_secondary_text.empty()) {
            nvgFontSize(ctx, 14.f);
            nvgFontFace(ctx, "sans");
            nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgText(ctx, content_x, text_y + 24.f, m_secondary_text.c_str(), nullptr);
        }
    }
    
    // Draw trailing icon
    if (m_trailing_icon) {
        auto icon = utf8(m_trailing_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(0.4f, 0.4f, 0.4f, 1.f));
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, x + w - 16.f, content_y, icon.data(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
