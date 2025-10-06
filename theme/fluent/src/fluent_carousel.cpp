#include <nanogui/fluent_carousel.h>
#include <chrono>
#include <nanogui/fluent_theme.h>
#include <nanogui/fluent_easing.h>
#include <nanogui/fluent_icon_button.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentCarousel::FluentCarousel(Widget *parent)
    : Widget(parent), m_current_index(0), m_auto_play(false), 
      m_auto_play_interval(3000), m_animation_progress(0.0f) {
}

void FluentCarousel::add_item(const std::string &title, const std::string &description) {
    Item item;
    item.title = title;
    item.description = description;
    m_items.push_back(item);
}

void FluentCarousel::set_current_index(int index) {
    if (index < 0 || index >= (int)m_items.size())
        return;
    
    if (index == m_current_index)
        return;
    
    m_previous_index = m_current_index;
    m_current_index = index;
    m_animation_start = std::chrono::steady_clock::now();
    m_animation_progress = 0.0f;
    
    if (m_callback)
        m_callback(m_current_index);
}

void FluentCarousel::next() {
    set_current_index((m_current_index + 1) % m_items.size());
}

void FluentCarousel::previous() {
    set_current_index((m_current_index - 1 + m_items.size()) % m_items.size());
}

Vector2i FluentCarousel::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(m_parent ? m_parent->width() : 600, 400);
}

bool FluentCarousel::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Check navigation buttons
    if (p.x() < m_pos.x() + 56) {
        previous();
        return true;
    }
    
    if (p.x() > m_pos.x() + m_size.x() - 56) {
        next();
        return true;
    }
    
    // Check indicator dots
    int indicator_y = m_pos.y() + m_size.y() - 40;
    int indicator_x_start = m_pos.x() + m_size.x() / 2 - (m_items.size() * 16) / 2;
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        int dot_x = indicator_x_start + i * 16;
        if (p.x() >= dot_x && p.x() < dot_x + 12 &&
            p.y() >= indicator_y && p.y() < indicator_y + 12) {
            set_current_index(i);
            return true;
        }
    }
    
    return false;
}

void FluentCarousel::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme || m_items.empty()) return;
    
    // Update animation
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_animation_start).count();
    
    float progress = std::min(1.0f, elapsed / 300.0f);
    m_animation_progress = FluentEasing::ease(FluentEasing::Curve::Emphasized, progress);
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Draw current item
    const auto &item = m_items[m_current_index];
    
    // Item content area
    float content_y = m_pos.y() + m_size.y() / 2;
    
    // Title
    nvgFontSize(ctx, 32.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    nvgText(ctx, m_pos.x() + m_size.x() / 2, content_y - 40, item.title.c_str(), nullptr);
    
    // Description
    if (!item.description.empty()) {
        nvgFontSize(ctx, 16.0f);
        nvgFontFace(ctx, "sans");
        nvgFillColor(ctx, theme->on_surface_color());
        nvgTextBox(ctx, m_pos.x() + 80, content_y, m_size.x() - 160, 
                   item.description.c_str(), nullptr);
    }
    
    // Navigation buttons
    nvgFontSize(ctx, 32.0f);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    
    // Previous button
    nvgText(ctx, m_pos.x() + 28, m_pos.y() + m_size.y() / 2, "<", nullptr);
    
    // Next button
    nvgText(ctx, m_pos.x() + m_size.x() - 28, m_pos.y() + m_size.y() / 2, ">", nullptr);
    
    // Indicator dots
    int indicator_y = m_pos.y() + m_size.y() - 40;
    int indicator_x_start = m_pos.x() + m_size.x() / 2 - (m_items.size() * 16) / 2;
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        nvgBeginPath(ctx);
        nvgCircle(ctx, indicator_x_start + i * 16 + 6, indicator_y + 6, 
                  i == (size_t)m_current_index ? 6 : 4);
        nvgFillColor(ctx, i == (size_t)m_current_index ? 
                     theme->primary_color() : theme->on_surface_color());
        nvgFill(ctx);
    }
}

NAMESPACE_END(nanogui)
