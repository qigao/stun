#include <nanogui/fluent_navigation_rail.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentNavigationRail::FluentNavigationRail(Widget *parent)
    : Widget(parent), m_selected_index(0), m_show_labels(true) {
}

void FluentNavigationRail::add_destination(int icon, const std::string &label) {
    m_destinations.emplace_back(icon, label);
    if (m_destinations.size() == 1)
        m_destinations[0].selected = true;
}

void FluentNavigationRail::set_selected_index(int index) {
    if (index < 0 || index >= (int)m_destinations.size())
        return;
    
    if (m_selected_index >= 0 && m_selected_index < (int)m_destinations.size())
        m_destinations[m_selected_index].selected = false;
    
    m_selected_index = index;
    m_destinations[index].selected = true;
    
    if (m_callback)
        m_callback(index);
}

Vector2i FluentNavigationRail::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(80, m_parent ? m_parent->height() : 400);
}

bool FluentNavigationRail::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    int item_height = m_show_labels ? 72 : 56;
    int index = (p.y() - m_pos.y()) / item_height;
    
    if (index >= 0 && index < (int)m_destinations.size()) {
        set_selected_index(index);
        return true;
    }
    
    return false;
}

void FluentNavigationRail::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    int item_height = m_show_labels ? 72 : 56;
    
    for (size_t i = 0; i < m_destinations.size(); ++i) {
        const auto &dest = m_destinations[i];
        float y = m_pos.y() + i * item_height;
        float cx = m_pos.x() + m_size.x() * 0.5f;
        float cy = y + (m_show_labels ? 32 : 28);
        
        // Indicator (selected state)
        if (dest.selected) {
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, cx - 28, cy - 16, 56, 32, 16);
            nvgFillColor(ctx, Color(0.9f, 0.9f, 1.0f, 1.0f));
            nvgFill(ctx);
        }
        
        // Icon (simplified - would use actual icon rendering)
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, dest.selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
        
        char icon_str[8];
        snprintf(icon_str, sizeof(icon_str), "%c", (char)dest.icon);
        nvgText(ctx, cx, cy, icon_str, nullptr);
        
        // Label
        if (m_show_labels && !dest.label.empty()) {
            nvgFontSize(ctx, 12.f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(ctx, dest.selected ? theme->on_surface_color() : theme->on_surface_color());
            nvgText(ctx, cx, cy + 20, dest.label.c_str(), nullptr);
        }
    }
}

NAMESPACE_END(nanogui)
