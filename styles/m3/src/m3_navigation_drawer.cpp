/*
    src/m3_navigation_drawer.cpp -- M3 Navigation Drawer implementation
*/

#include <nanogui/m3_navigation_drawer.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3NavigationDrawer::M3NavigationDrawer(Widget *parent, Type type)
    : Popup(parent), m_type(type) {
    set_modal(type == Type::Modal);
    set_fixed_width(360);
}

M3Theme *M3NavigationDrawer::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3NavigationDrawer::add_item(int icon, const std::string &label, bool selected,
                                   std::function<void()> callback) {
    m_items.emplace_back(icon, label, selected, callback);
    if (selected) {
        m_selected_index = static_cast<int>(m_items.size()) - 1;
    }
}

void M3NavigationDrawer::set_selected(int index) {
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        for (size_t i = 0; i < m_items.size(); ++i) {
            m_items[i].selected = (static_cast<int>(i) == index);
        }
        m_selected_index = index;
    }
}

void M3NavigationDrawer::set_header(const std::string &title, const std::string &subtitle) {
    m_header_title = title;
    m_header_subtitle = subtitle;
}

void M3NavigationDrawer::show() {
    if (m_parent) {
        set_position(Vector2i(0, 0));
        set_size(Vector2i(360, m_parent->height()));
    }
    set_visible(true);
}

void M3NavigationDrawer::hide() {
    set_visible(false);
}

Vector2i M3NavigationDrawer::preferred_size(NVGcontext *) const {
    int height = 64; // Header
    height += static_cast<int>(m_items.size()) * 56; // Items
    return Vector2i(360, height);
}

int M3NavigationDrawer::item_at_position(const Vector2i &p) const {
    Vector2i local = p - m_pos;
    int y_offset = m_header_title.empty() ? 0 : 64;
    
    if (local.y() < y_offset) return -1;
    
    int item_y = local.y() - y_offset;
    int idx = item_y / 56;
    
    return (idx >= 0 && idx < static_cast<int>(m_items.size())) ? idx : -1;
}

bool M3NavigationDrawer::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Popup::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down) return false;
    
    int idx = item_at_position(p);
    if (idx >= 0) {
        set_selected(idx);
        if (m_items[idx].callback) {
            m_items[idx].callback();
        }
        if (m_type == Type::Modal) {
            hide();
        }
        return true;
    }
    
    return false;
}

void M3NavigationDrawer::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Popup::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x(), h = m_size.y();

    nvgSave(ctx);

    // Scrim for modal
    if (m_modal && m_type == Type::Modal) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        nvgFillColor(ctx, Color(theme->scrim().r(), theme->scrim().g(), theme->scrim().b(), 0.32f));
        nvgFill(ctx);
    }

    // Drawer background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Elevation tint
    Color tint = theme->elevation_tint(M3Theme::Elevation::Level1);
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, tint);
    nvgFill(ctx);

    float content_y = y;

    // Header
    if (!m_header_title.empty()) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, x + 16, content_y + 16, m_header_title.c_str(), nullptr);

        if (!m_header_subtitle.empty()) {
            nvgFontSize(ctx, 14);
            nvgFontFace(ctx, "sans");
            nvgFillColor(ctx, theme->on_surface_variant());
            nvgText(ctx, x + 16, content_y + 44, m_header_subtitle.c_str(), nullptr);
        }
        content_y += 64;
    }

    // Items
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto &item = m_items[i];
        float item_y = content_y + i * 56;
        bool hovered = (static_cast<int>(i) == m_hover_index);

        // Background for selected
        if (item.selected) {
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, x + 12, item_y + 4, w - 24, 48, 24);
            nvgFillColor(ctx, theme->secondary_container());
            nvgFill(ctx);
        }

        // Hover state
        if (hovered && m_enabled && !item.selected) {
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, x + 12, item_y + 4, w - 24, 48, 24);
            nvgFillColor(ctx, theme->state_layer(theme->on_surface(), 0.08f));
            nvgFill(ctx);
        }

        Color fg = item.selected ? theme->on_secondary_container() : theme->on_surface_variant();

        // Icon
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, x + 28, item_y + 28, utf8(item.icon).data(), nullptr);

        // Label
        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, item.selected ? "sans-bold" : "sans");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, fg);
        nvgText(ctx, x + 68, item_y + 28, item.label.c_str(), nullptr);
    }

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
