/*
    src/fluent_app_bar.cpp -- Fluent Design App Bar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/fluent_app_bar.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/layout.h>

NAMESPACE_BEGIN(nanogui)

FluentAppBar::FluentAppBar(Widget *parent, const std::string &title, Type type)
    : Widget(parent), m_title(title), m_type(type), m_navigation_icon(0) {
    
    // Create actions container
    m_actions_container = new Widget(this);
    m_actions_container->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
    
    set_type(type);
}

void FluentAppBar::set_type(Type type) {
    m_type = type;
    
    int height = 64; // Default to Regular
    switch (type) {
        case Type::Regular:
            height = 64;
            break;
        case Type::Medium:
            height = 112;
            break;
        case Type::Large:
            height = 152;
            break;
    }
    
    set_fixed_height(height);
}

Vector2i FluentAppBar::preferred_size_impl(NVGcontext *) const {
    int height = 64; // Default to Regular
    switch (m_type) {
        case Type::Regular:
            height = 64;
            break;
        case Type::Medium:
            height = 112;
            break;
        case Type::Large:
            height = 152;
            break;
    }
    
    return Vector2i(0, height); // Width will be filled by parent
}

bool FluentAppBar::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != GLFW_MOUSE_BUTTON_1 || !down)
        return false;
    
    // Check if navigation icon was clicked
    if (m_navigation_icon && m_navigation_callback) {
        float icon_x = 16.f;
        float icon_y = (m_size.y() - 40.f) * 0.5f;
        float icon_size = 40.f;
        
        if (p.x() >= icon_x && p.x() <= icon_x + icon_size &&
            p.y() >= icon_y && p.y() <= icon_y + icon_size) {
            m_navigation_callback();
            return true;
        }
    }
    
    return false;
}

void FluentAppBar::draw(NVGcontext *ctx) {
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());
    
    nvgSave(ctx);
    
    // Draw app bar background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(0.25f, 0.7f, 1.f, 1.f)); // Primary color
    nvgFill(ctx);
    
    // Draw elevation shadow
    NVGcolor shadow_outer = nvgRGBAf(0.f, 0.f, 0.f, 0.2f);
    NVGcolor shadow_inner = nvgRGBAf(0.f, 0.f, 0.f, 0.f);
    
    NVGpaint shadow = nvgBoxGradient(ctx, x, y + h, w, 8.f, 0.f, 8.f, shadow_outer, shadow_inner);
    
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y + h, w, 8.f);
    nvgFillPaint(ctx, shadow);
    nvgFill(ctx);
    
    float content_x = x + 16.f;
    float content_y = y + h - 16.f; // Bottom aligned for large app bars
    
    // Draw navigation icon if present
    if (m_navigation_icon) {
        auto icon = utf8(m_navigation_icon);
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        
        float icon_y = (m_type == Type::Regular) ? y + h * 0.5f : content_y - 12.f;
        nvgText(ctx, content_x, icon_y, icon.data(), nullptr);
        content_x += 40.f + 16.f;
    }
    
    // Draw title
    if (!m_title.empty()) {
        int font_size = (m_type == Type::Large) ? 32 : 
                       (m_type == Type::Medium) ? 24 : 20;
        
        nvgFontSize(ctx, font_size);
        nvgFontFace(ctx, "sans-bold");
        nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        
        float title_y = (m_type == Type::Regular) ? y + h * 0.5f : content_y - 12.f;
        nvgText(ctx, content_x, title_y, m_title.c_str(), nullptr);
    }
    
    // Position actions container
    if (m_actions_container) {
        Vector2i actions_size = m_actions_container->preferred_size(ctx);
        int actions_x = m_size.x() - actions_size.x() - 16;
        int actions_y = (m_type == Type::Regular) ? 
                        (m_size.y() - actions_size.y()) / 2 :
                        m_size.y() - actions_size.y() - 16;
        
        m_actions_container->set_position(Vector2i(actions_x, actions_y));
        m_actions_container->set_size(actions_size);
    }
    
    nvgRestore(ctx);
    
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
