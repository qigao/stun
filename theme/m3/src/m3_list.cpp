/*
    src/m3_list.cpp -- Material Design 3 List implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_list.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

// M3List implementation
M3List::M3List(Widget *parent)
    : Widget(parent) {
}

M3Theme *M3List::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3List::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    // Draw background
    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);
    nvgRestore(ctx);

    // Draw children
    Widget::draw(ctx);
}

// M3ListItem implementation
M3ListItem::M3ListItem(Widget *parent, const std::string &text,
                       const std::string &secondary_text, int icon)
    : Widget(parent), m_text(text), m_secondary_text(secondary_text), m_icon(icon) {
    int height = secondary_text.empty() ? 56 : 72;
    set_fixed_height(height);
}

M3Theme *M3ListItem::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

bool M3ListItem::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down)
        return false;

    if (m_callback) {
        m_callback();
        return true;
    }

    return false;
}

Vector2i M3ListItem::preferred_size(NVGcontext *) const {
    int height = m_secondary_text.empty() ? 56 : 72;
    return Vector2i(0, height); // Width fills parent
}

void M3ListItem::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();

    nvgSave(ctx);

    // Draw hover state
    if (m_mouse_focus && m_enabled) {
        Color state_color = theme->state_layer(theme->on_surface(), 0.08f);
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, h);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
    }

    // Calculate layout
    float content_x = x + 16.0f;
    float icon_size = 24.0f;

    // Draw icon if present
    if (m_icon) {
        nvgFontSize(ctx, icon_size);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, m_enabled ? theme->on_surface_variant() :
                     Color(theme->on_surface().r(), theme->on_surface().g(),
                          theme->on_surface().b(), 0.38f));
        nvgText(ctx, content_x, y + h * 0.5f, utf8(m_icon).data(), nullptr);
        content_x += icon_size + 16.0f;
    }

    // Draw text
    Color text_color = m_enabled ? theme->on_surface() :
                      Color(theme->on_surface().r(), theme->on_surface().g(),
                           theme->on_surface().b(), 0.38f);

    if (m_secondary_text.empty()) {
        // Single line
        nvgFontSize(ctx, 16);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, content_x, y + h * 0.5f, m_text.c_str(), nullptr);
    } else {
        // Two lines
        nvgFontSize(ctx, 16);
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, content_x, y + h * 0.5f - 2.0f, m_text.c_str(), nullptr);

        nvgFontSize(ctx, 14);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(ctx, theme->on_surface_variant());
        nvgText(ctx, content_x, y + h * 0.5f + 2.0f, m_secondary_text.c_str(), nullptr);
    }

    nvgRestore(ctx);

    // Draw children
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
