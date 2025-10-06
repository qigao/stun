#include <nanogui/fluent_bottom_sheet.h>
#include <chrono>
#include <nanogui/fluent_theme.h>
#include <nanogui/fluent_easing.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentBottomSheet::FluentBottomSheet(Widget *parent)
    : Widget(parent), m_state(State::Hidden), m_peek_height(80),
      m_animation_progress(0.0f), m_dragging(false), m_drag_start_y(0) {
}

void FluentBottomSheet::show() {
    if (m_state != State::Hidden)
        return;
    
    m_state = State::Collapsed;
    m_animation_start = std::chrono::steady_clock::now();
    
    if (m_state_callback)
        m_state_callback(m_state);
}

void FluentBottomSheet::hide() {
    if (m_state == State::Hidden)
        return;
    
    m_state = State::Hidden;
    m_animation_start = std::chrono::steady_clock::now();
    
    if (m_state_callback)
        m_state_callback(m_state);
}

void FluentBottomSheet::expand() {
    if (m_state != State::Collapsed)
        return;
    
    m_state = State::Expanded;
    m_animation_start = std::chrono::steady_clock::now();
    
    if (m_state_callback)
        m_state_callback(m_state);
}

void FluentBottomSheet::collapse() {
    if (m_state != State::Expanded)
        return;
    
    m_state = State::Collapsed;
    m_animation_start = std::chrono::steady_clock::now();
    
    if (m_state_callback)
        m_state_callback(m_state);
}

Vector2i FluentBottomSheet::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(m_parent ? m_parent->width() : 400, 400);
}

bool FluentBottomSheet::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    if (down) {
        // Check if clicking on scrim
        if (m_state != State::Hidden && p.y() < m_pos.y()) {
            hide();
            return true;
        }
        
        // Start drag
        m_dragging = true;
        m_drag_start_y = p.y();
        return true;
    } else {
        m_dragging = false;
    }
    
    return false;
}

bool FluentBottomSheet::mouse_drag_event(const Vector2i &p, const Vector2i &rel, 
                                           int button, int modifiers) {
    if (!m_dragging)
        return false;
    
    // Handle drag to expand/collapse/hide
    int delta = p.y() - m_drag_start_y;
    
    if (delta > 50 && m_state == State::Expanded) {
        collapse();
    } else if (delta > 50 && m_state == State::Collapsed) {
        hide();
    } else if (delta < -50 && m_state == State::Collapsed) {
        expand();
    }
    
    return true;
}

void FluentBottomSheet::draw(NVGcontext *ctx) {
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    if (m_state == State::Hidden && m_animation_progress <= 0.0f)
        return;
    
    // Update animation
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_animation_start).count();
    
    float progress = std::min(1.0f, elapsed / 300.0f);
    float eased = FluentEasing::ease(FluentEasing::Curve::Emphasized, progress);
    
    float target = 0.0f;
    if (m_state == State::Collapsed) target = 0.5f;
    if (m_state == State::Expanded) target = 1.0f;
    
    m_animation_progress = m_animation_progress + (target - m_animation_progress) * eased;
    
    // Draw scrim
    if (m_animation_progress > 0.0f) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        Color scrim = Color(0.0f, 0.0f, 0.0f, 0.4f * m_animation_progress);
        nvgFillColor(ctx, scrim);
        nvgFill(ctx);
    }
    
    // Calculate sheet position
    int sheet_height = m_state == State::Collapsed ? m_peek_height : m_size.y();
    float sheet_y = m_parent->height() - sheet_height * m_animation_progress;
    
    // Draw sheet
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, m_pos.x(), sheet_y, m_size.x(), sheet_height,
                          24, 24, 0, 0);
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Draw drag handle
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x() + m_size.x() / 2 - 16, sheet_y + 8, 32, 4, 2);
    nvgFillColor(ctx, theme->on_surface_color());
    nvgFill(ctx);
    
    // Draw children
    nvgSave(ctx);
    nvgTranslate(ctx, 0, sheet_y - m_pos.y());
    Widget::draw(ctx);
    nvgRestore(ctx);
}

NAMESPACE_END(nanogui)
