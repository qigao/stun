#include <nanogui/fluent_expansion_panel.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_easing.h>
#include <nanogui/opengl.h>
#include <chrono>
NAMESPACE_BEGIN(nanogui)

FluentExpansionPanel::FluentExpansionPanel(Widget *parent, const std::string &title)
    : Widget(parent), m_title(title), m_expanded(false), m_animation_progress(0.0f) {
    
    // Create content container
    m_content = new Widget(this);
    m_content->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 16, 16));
    m_content->set_visible(false);
}

void FluentExpansionPanel::set_expanded(bool expanded) {
    if (m_expanded == expanded)
        return;
    
    m_expanded = expanded;
    m_animation_start = std::chrono::steady_clock::now();
    m_content->set_visible(expanded);
    
    if (m_callback)
        m_callback(m_expanded);
}

void FluentExpansionPanel::toggle() {
    set_expanded(!m_expanded);
}

Vector2i FluentExpansionPanel::preferred_size_impl(NVGcontext *ctx) const {
    int width = m_parent ? m_parent->width() : 400;
    int height = 64; // Header height
    
    if (m_expanded && m_content) {
        height += m_content->preferred_size(ctx).y();
    }
    
    return Vector2i(width, height);
}

bool FluentExpansionPanel::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Check if click is on header
    if (p.y() < m_pos.y() + 64) {
        toggle();
        return true;
    }
    
    return false;
}

void FluentExpansionPanel::draw(NVGcontext *ctx) {
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Update animation
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_animation_start).count();
    
    float progress = std::min(1.0f, elapsed / 200.0f);
    float eased = FluentEasing::ease(FluentEasing::Curve::Standard, progress);
    
    float target = m_expanded ? 1.0f : 0.0f;
    m_animation_progress = m_animation_progress + (target - m_animation_progress) * eased;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Header
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), 64);
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Title
    nvgFontSize(ctx, 16.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    nvgText(ctx, m_pos.x() + 16, m_pos.y() + 24, m_title.c_str(), nullptr);
    
    // Description
    if (!m_description.empty()) {
        nvgFontSize(ctx, 14.0f);
        nvgFillColor(ctx, theme->on_surface_color());
        nvgText(ctx, m_pos.x() + 16, m_pos.y() + 44, m_description.c_str(), nullptr);
    }
    
    // Expand/collapse icon
    nvgFontSize(ctx, 24.0f);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface_color());
    
    // Rotate icon based on expansion
    nvgSave(ctx);
    float icon_x = m_pos.x() + m_size.x() - 28;
    float icon_y = m_pos.y() + 32;
    nvgTranslate(ctx, icon_x, icon_y);
    nvgRotate(ctx, m_animation_progress * NVG_PI);
    nvgTranslate(ctx, -icon_x, -icon_y);
    nvgText(ctx, icon_x, icon_y, "▼", nullptr);
    nvgRestore(ctx);
    
    // Content (if expanded)
    if (m_expanded && m_animation_progress > 0.0f) {
        nvgSave(ctx);
        
        // Clip content area
        nvgScissor(ctx, m_pos.x(), m_pos.y() + 64, m_size.x(), 
                   (m_size.y() - 64) * m_animation_progress);
        
        Widget::draw(ctx);
        
        nvgResetScissor(ctx);
        nvgRestore(ctx);
    }
    
    // Border
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
    nvgStroke(ctx);
}

NAMESPACE_END(nanogui)
