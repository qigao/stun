/*
    src/fluent_avatar.cpp -- Fluent Design Avatar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_avatar.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentAvatar::FluentAvatar(Widget *parent, const std::string &initials, Size size)
    : Widget(parent), m_initials(initials), m_icon(0), m_avatar_size(size),
      m_background_color(0.6f, 0.6f, 0.6f, 1.f), m_text_color(1.f, 1.f, 1.f, 1.f) {
    set_avatar_size(size);
}

void FluentAvatar::set_avatar_size(Size size) {
    m_avatar_size = size;
    
    int avatar_size = 40; // Default to Medium
    switch (size) {
        case Size::Small:
            avatar_size = 24;
            break;
        case Size::Medium:
            avatar_size = 40;
            break;
        case Size::Large:
            avatar_size = 56;
            break;
    }
    
    set_fixed_size(Vector2i(avatar_size, avatar_size));
}

Vector2i FluentAvatar::preferred_size_impl(NVGcontext *) const {
    switch (m_avatar_size) {
        case Size::Small:
            return Vector2i(24, 24);
        case Size::Medium:
            return Vector2i(40, 40);
        case Size::Large:
            return Vector2i(56, 56);
    }
    return Vector2i(40, 40);
}

void FluentAvatar::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float size = static_cast<float>(m_size.x());
    
    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;
    float radius = size * 0.5f;
    
    nvgSave(ctx);
    
    // Draw background circle
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    nvgFillColor(ctx, nvgRGBAf(m_background_color.r(), m_background_color.g(), 
                               m_background_color.b(), m_background_color.w()));
    nvgFill(ctx);
    
    // Draw content (icon or initials)
    if (m_icon) {
        // Draw icon
        auto icon = utf8(m_icon);
        
        float icon_size = 20.f; // Default to Medium
        switch (m_avatar_size) {
            case Size::Small:
                icon_size = 14.f;
                break;
            case Size::Medium:
                icon_size = 20.f;
                break;
            case Size::Large:
                icon_size = 28.f;
                break;
        }
        
        nvgFontSize(ctx, icon_size);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBAf(m_text_color.r(), m_text_color.g(), 
                                   m_text_color.b(), m_text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cx, cy, icon.data(), nullptr);
    } else if (!m_initials.empty()) {
        // Draw initials
        float font_size = 16.f; // Default to Medium
        switch (m_avatar_size) {
            case Size::Small:
                font_size = 10.f;
                break;
            case Size::Medium:
                font_size = 16.f;
                break;
            case Size::Large:
                font_size = 20.f;
                break;
        }
        
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans-bold");
        nvgFillColor(ctx, nvgRGBAf(m_text_color.r(), m_text_color.g(), 
                                   m_text_color.b(), m_text_color.w()));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, cx, cy, m_initials.c_str(), nullptr);
    }
    
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
