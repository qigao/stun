#pragma once

#include <nanogui/widget.h>
#include <chrono>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Expansion Panel
 * 
 * Collapsible container that expands to reveal more content.
 * Useful for grouping related information.
 */
class NANOGUI_EXPORT FluentExpansionPanel : public Widget {
public:
    FluentExpansionPanel(Widget *parent, const std::string &title);
    
    /// Title
    const std::string &title() const { return m_title; }
    void set_title(const std::string &title) { m_title = title; }
    
    /// Description (optional subtitle)
    const std::string &description() const { return m_description; }
    void set_description(const std::string &description) { m_description = description; }
    
    /// Expanded state
    bool expanded() const { return m_expanded; }
    void set_expanded(bool expanded);
    
    /// Toggle expansion
    void toggle();
    
    /// Content widget
    Widget *content() { return m_content; }
    
    /// Callback when expansion changes
    std::function<void(bool)> callback() const { return m_callback; }
    void set_callback(const std::function<void(bool)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    std::string m_title;
    std::string m_description;
    bool m_expanded;
    Widget *m_content;
    float m_animation_progress;
    std::chrono::steady_clock::time_point m_animation_start;
    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
