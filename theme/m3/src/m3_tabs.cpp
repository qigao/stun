/*
    src/m3_tabs.cpp -- Material Design 3 Tabs implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_tabs.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3Tabs::M3Tabs(Widget *parent, const std::vector<std::string> &tab_names)
    : Widget(parent), m_tab_names(tab_names) {
    set_fixed_height(48); // M3 tab height
}

M3Theme *M3Tabs::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Tabs::set_active_tab(int index) {
    if (index >= 0 && index < static_cast<int>(m_tab_names.size())) {
        m_active_tab = index;
        if (m_callback)
            m_callback(index);
    }
}

std::vector<float> M3Tabs::calculate_tab_positions(NVGcontext *ctx) const {
    std::vector<float> positions;
    if (m_tab_names.empty())
        return positions;

    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");

    float x = m_pos.x();
    positions.push_back(x);

    for (const auto &name : m_tab_names) {
        float tw = nvgTextBounds(ctx, 0, 0, name.c_str(), nullptr, nullptr);
        x += tw + 48; // 24dp padding on each side
        positions.push_back(x);
    }

    return positions;
}

int M3Tabs::tab_at_position(const Vector2i &p) const {
    // We need a context to calculate positions, but we can't get it here
    // So we'll do a simpler calculation based on equal-width tabs
    if (m_tab_names.empty())
        return -1;

    Vector2i local = p - m_pos;
    if (local.x() < 0 || local.x() >= m_size.x())
        return -1;

    // Approximate tab width (will be refined in draw)
    float approx_tab_width = m_size.x() / static_cast<float>(m_tab_names.size());
    int tab = static_cast<int>(local.x() / approx_tab_width);
    
    if (tab >= 0 && tab < static_cast<int>(m_tab_names.size()))
        return tab;

    return -1;
}

bool M3Tabs::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT)
        return false;

    if (down) {
        int tab = tab_at_position(p);
        if (tab >= 0) {
            set_active_tab(tab);
            return true;
        }
    }

    return false;
}

bool M3Tabs::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    Widget::mouse_motion_event(p, rel, button, modifiers);
    
    m_hover_tab = tab_at_position(p);
    
    return false;
}

Vector2i M3Tabs::preferred_size(NVGcontext *ctx) const {
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");

    float total_width = 0;
    for (const auto &name : m_tab_names) {
        float tw = nvgTextBounds(ctx, 0, 0, name.c_str(), nullptr, nullptr);
        total_width += tw + 48; // 24dp padding on each side
    }

    return Vector2i(static_cast<int>(total_width), 48);
}

void M3Tabs::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Widget::draw(ctx);
        return;
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float h = m_size.y();

    nvgSave(ctx);

    // Draw background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, m_size.x(), h);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Draw tabs
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");

    auto positions = calculate_tab_positions(ctx);

    for (size_t i = 0; i < m_tab_names.size(); ++i) {
        bool active = (static_cast<int>(i) == m_active_tab);
        bool hovered = (static_cast<int>(i) == m_hover_tab);

        float tab_x = positions[i];
        float tab_w = positions[i + 1] - positions[i];

        // Draw state layer if hovered
        if (hovered && m_enabled && !active) {
            Color state_color = theme->state_layer(theme->on_surface(), 0.08f);
            nvgBeginPath(ctx);
            nvgRect(ctx, tab_x, y, tab_w, h);
            nvgFillColor(ctx, state_color);
            nvgFill(ctx);
        }

        // Draw text
        Color text_color = active ? theme->primary() : theme->on_surface_variant();
        if (!m_enabled) {
            text_color = Color(theme->on_surface().r(), theme->on_surface().g(),
                              theme->on_surface().b(), 0.38f);
        }

        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, text_color);
        nvgText(ctx, tab_x + tab_w * 0.5f, y + h * 0.5f, 
                m_tab_names[i].c_str(), nullptr);

        // Draw active indicator
        if (active) {
            float indicator_height = 3.0f;
            nvgBeginPath(ctx);
            nvgRect(ctx, tab_x, y + h - indicator_height, tab_w, indicator_height);
            nvgFillColor(ctx, theme->primary());
            nvgFill(ctx);
        }
    }

    // Draw bottom divider
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y + h - 1, m_size.x(), 1);
    nvgFillColor(ctx, theme->outline_variant());
    nvgFill(ctx);

    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
