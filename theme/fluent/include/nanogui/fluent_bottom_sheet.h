#pragma once

#include <nanogui/widget.h>
#include <chrono>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Bottom Sheet
 * 
 * Modal sheet that slides up from the bottom of the screen.
 * Used for additional content or actions.
 */
class NANOGUI_EXPORT FluentBottomSheet : public Widget {
public:
    enum class State {
        Hidden,
        Collapsed,
        Expanded
    };
    
    FluentBottomSheet(Widget *parent);
    
    /// Show/hide bottom sheet
    void show();
    void hide();
    
    /// Expand/collapse
    void expand();
    void collapse();
    
    /// Current state
    State state() const { return m_state; }
    
    /// Peek height when collapsed
    int peek_height() const { return m_peek_height; }
    void set_peek_height(int height) { m_peek_height = height; }
    
    /// Callback when state changes
    std::function<void(State)> state_callback() const { return m_state_callback; }
    void set_state_callback(const std::function<void(State)> &callback) { 
        m_state_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    State m_state;
    int m_peek_height;
    float m_animation_progress;
    std::chrono::steady_clock::time_point m_animation_start;
    bool m_dragging;
    int m_drag_start_y;
    std::function<void(State)> m_state_callback;
};

NAMESPACE_END(nanogui)
