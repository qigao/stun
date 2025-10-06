#include <nanogui/fluent_banner.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_button.h>
#include <nanogui/fluent_icon_button.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentBanner::FluentBanner(Widget *parent, const std::string &message)
    : Widget(parent), m_message(message), m_icon(0), m_visible(true) {
    
    set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 16, 16));
}

void FluentBanner::add_action(const std::string &label, const std::function<void()> &callback) {
    Action action;
    action.label = label;
    action.callback = callback;
    m_actions.push_back(action);
}

void FluentBanner::show() {
    m_visible = true;
    set_visible(true);
}

void FluentBanner::dismiss() {
    m_visible = false;
    set_visible(false);
    
    if (m_dismiss_callback)
        m_dismiss_callback();
}

Vector2i FluentBanner::preferred_size_impl(NVGcontext *ctx) const {
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    
    float text_width = nvgTextBounds(ctx, 0, 0, m_message.c_str(), nullptr, nullptr);
    
    int width = m_parent ? m_parent->width() : 400;
    int height = 72;
    
    // Multi-line support
    if (text_width > width - 200) {
        height = 96;
    }
    
    return Vector2i(width, height);
}

void FluentBanner::draw(NVGcontext *ctx) {
    if (!m_visible)
        return;
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    float x_offset = m_pos.x() + 16;
    float y_center = m_pos.y() + m_size.y() / 2;
    
    // Icon (if set)
    if (m_icon != 0) {
        nvgFontSize(ctx, 24.0f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_color());
        
        char icon_str[8];
        snprintf(icon_str, sizeof(icon_str), "%c", (char)m_icon);
        nvgText(ctx, x_offset, y_center, icon_str, nullptr);
        
        x_offset += 40;
    }
    
    // Message
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    
    float text_width = m_size.x() - x_offset - 200;
    nvgTextBox(ctx, x_offset, y_center - 10, text_width, m_message.c_str(), nullptr);
    
    // Actions
    float action_x = m_pos.x() + m_size.x() - 16;
    
    // Close button
    nvgFontSize(ctx, 20.0f);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    nvgText(ctx, action_x, y_center, "×", nullptr);
    
    action_x -= 40;
    
    // Action buttons
    for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
        nvgFontSize(ctx, 14.0f);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->primary_color());
        
        float text_bounds[4];
        nvgTextBounds(ctx, action_x, y_center, it->label.c_str(), nullptr, text_bounds);
        nvgText(ctx, action_x, y_center, it->label.c_str(), nullptr);
        
        action_x -= (text_bounds[2] - text_bounds[0]) + 24;
    }
    
    Widget::draw(ctx);
}

bool FluentBanner::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Check close button
    if (p.x() > m_pos.x() + m_size.x() - 56) {
        dismiss();
        return true;
    }
    
    // Check action buttons
    float action_x = m_pos.x() + m_size.x() - 56;
    
    for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
        // Approximate button width
        float button_width = it->label.length() * 8 + 24;
        action_x -= button_width;
        
        if (p.x() >= action_x && p.x() <= action_x + button_width) {
            if (it->callback)
                it->callback();
            return true;
        }
    }
    
    return false;
}

NAMESPACE_END(nanogui)
