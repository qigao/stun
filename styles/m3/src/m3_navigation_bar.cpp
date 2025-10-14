/*
    src/m3_navigation_bar.cpp -- Material Design 3 Navigation Bar implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_navigation_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3NavigationBar::M3NavigationBar(Widget *parent, const std::vector<NavItem> &items)
    : Widget(parent), m_items(items) {
    set_fixed_height(80); // M3 navigation bar height
}

M3Theme *M3NavigationBar::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3NavigationBar::set_active_item(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_active_item = index;
        if (m_callback)
            m_callback(index);
    }
}

int M3NavigationBar::item_at_position(const Vector2i &p) const {
    if (m_items.empty())
        return -1;

    Vector2i local = p - m_pos;
    float item_width = m_size.x() / static_cast<float>(m_items.size());
    int item = static_cast<int>(local.x() / item_width);

    if (item >= 0 && item < static_cast<int>(m_items.size()) && m_items[item].enabled)
        return item;

    return -1;
}

bool M3NavigationBar::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (button != NANOGUI_MOUSE_BUTTON_LEFT || !down)
        return false;

    int item = item_at_position(p);
    if (item >= 0) {
        set_active_item(item);
        return true;
    }

    return false;
}

bool M3NavigationBar::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    Widget::mouse_motion_event(p, rel, button, modifiers);
    
    m_hover_item = item_at_position(p);
    
    return false;
}

Vector2i M3NavigationBar::preferred_size(NVGcontext *) const {
    return Vector2i(0, 80); // Height only, width fills parent
}

void M3NavigationBar::draw(NVGcontext *ctx) {
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

    // Draw background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Draw top divider
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, 1);
    nvgFillColor(ctx, theme->outline_variant());
    nvgFill(ctx);

    // Draw items
    if (m_items.empty())
        return;

    float item_width = w / static_cast<float>(m_items.size());

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto &item = m_items[i];
        bool active = (static_cast<int>(i) == m_active_item);
        bool hovered = (static_cast<int>(i) == m_hover_item);

        float item_x = x + i * item_width;

        // Draw state layer if hovered
        if (hovered && item.enabled && !active) {
            Color state_color = theme->state_layer(theme->on_surface(), 0.08f);
            nvgBeginPath(ctx);
            nvgRect(ctx, item_x, y, item_width, h);
            nvgFillColor(ctx, state_color);
            nvgFill(ctx);
        }

        // Draw active indicator
        if (active) {
            float indicator_width = 64.0f;
            float indicator_height = 32.0f;
            float indicator_x = item_x + (item_width - indicator_width) * 0.5f;
            float indicator_y = y + 12.0f;

            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, indicator_x, indicator_y, indicator_width, indicator_height, 16.0f);
            nvgFillColor(ctx, theme->secondary_container());
            nvgFill(ctx);
        }

        // Draw icon
        Color icon_color = active ? theme->on_secondary_container() : theme->on_surface_variant();
        if (!item.enabled) {
            icon_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                              theme->on_surface().b(), 0.38f);
        }

        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(ctx, icon_color);
        nvgText(ctx, item_x + item_width * 0.5f, y + 16.0f, utf8(item.icon).data(), nullptr);

        // Draw label
        Color text_color = active ? theme->on_surface() : theme->on_surface_variant();
        if (!item.enabled) {
            text_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                              theme->on_surface().b(), 0.38f);
        }

        nvgFontSize(ctx, 12);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, item_x + item_width * 0.5f, y + 52.0f, item.label.c_str(), nullptr);
    }

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
