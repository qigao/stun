#include <nanogui/fluent_bottom_navigation.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentBottomNavigation::FluentBottomNavigation(Widget *parent)
    : Widget(parent), m_selected_index(0) {
}

void FluentBottomNavigation::add_item(int icon, const std::string &label) {
    m_items.emplace_back(icon, label);
    if (m_items.size() == 1)
        m_items[0].selected = true;
}

void FluentBottomNavigation::set_selected_index(int index) {
    if (index < 0 || index >= (int)m_items.size())
        return;
    
    if (m_selected_index >= 0 && m_selected_index < (int)m_items.size())
        m_items[m_selected_index].selected = false;
    
    m_selected_index = index;
    m_items[index].selected = true;
    
    if (m_callback)
        m_callback(index);
}

Vector2i FluentBottomNavigation::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(m_parent ? m_parent->width() : 360, 80);
}

bool FluentBottomNavigation::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    if (m_items.empty())
        return false;
    
    float item_width = (float)m_size.x() / m_items.size();
    int index = (p.x() - m_pos.x()) / item_width;
    
    if (index >= 0 && index < (int)m_items.size()) {
        set_selected_index(index);
        return true;
    }
    
    return false;
}

void FluentBottomNavigation::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    if (m_items.empty())
        return;
    
    float item_width = (float)m_size.x() / m_items.size();
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto &item = m_items[i];
        float x = m_pos.x() + i * item_width;
        float cx = x + item_width * 0.5f;
        float cy = m_pos.y() + 28;
        
        // Active indicator
        if (item.selected) {
            nvgBeginPath(ctx);
            nvgRoundedRect(ctx, cx - 32, cy - 16, 64, 32, 16);
            nvgFillColor(ctx, Color(0.9f, 0.9f, 1.0f, 1.0f));
            nvgFill(ctx);
        }
        
        // Icon
        nvgFontSize(ctx, 24.f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, item.selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
        
        char icon_str[8];
        snprintf(icon_str, sizeof(icon_str), "%c", (char)item.icon);
        nvgText(ctx, cx, cy, icon_str, nullptr);
        
        // Label
        if (!item.label.empty()) {
            nvgFontSize(ctx, 12.f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFillColor(ctx, item.selected ? theme->on_surface_color() : theme->on_surface_color());
            nvgText(ctx, cx, cy + 20, item.label.c_str(), nullptr);
        }
    }
}

NAMESPACE_END(nanogui)
