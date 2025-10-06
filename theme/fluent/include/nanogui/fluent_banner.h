#pragma once

#include <nanogui/widget.h>
#include <string>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Banner
 * 
 * Persistent notification that appears at the top of the screen.
 * Used for important, non-intrusive messages with actions.
 */
class NANOGUI_EXPORT FluentBanner : public Widget {
public:
    FluentBanner(Widget *parent, const std::string &message);
    
    /// Message text
    const std::string &message() const { return m_message; }
    void set_message(const std::string &message) { m_message = message; }
    
    /// Optional icon
    int icon() const { return m_icon; }
    void set_icon(int icon) { m_icon = icon; }
    
    /// Add action button
    void add_action(const std::string &label, const std::function<void()> &callback);
    
    /// Show/dismiss banner
    void show();
    void dismiss();
    
    /// Dismiss callback
    std::function<void()> dismiss_callback() const { return m_dismiss_callback; }
    void set_dismiss_callback(const std::function<void()> &callback) { 
        m_dismiss_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    struct Action {
        std::string label;
        std::function<void()> callback;
    };
    
    std::string m_message;
    int m_icon;
    std::vector<Action> m_actions;
    bool m_visible;
    std::function<void()> m_dismiss_callback;
};

NAMESPACE_END(nanogui)
