#pragma once

#include <nanogui/fluent_text_field.h>
#include <vector>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Search Bar
 * 
 * Prominent search field with suggestions and voice search support.
 * Follows Fluent Design 3 specifications.
 */
class NANOGUI_EXPORT FluentSearchBar : public Widget {
public:
    FluentSearchBar(Widget *parent);
    
    /// Search query
    std::string query() const;
    void set_query(const std::string &query);
    
    /// Placeholder text
    std::string placeholder() const { return m_placeholder; }
    void set_placeholder(const std::string &placeholder);
    
    /// Search suggestions
    void set_suggestions(const std::vector<std::string> &suggestions);
    
    /// Search callback
    std::function<void(const std::string&)> callback() const { return m_callback; }
    void set_callback(const std::function<void(const std::string&)> &callback) { 
        m_callback = callback; 
    }
    
    /// Show/hide voice search button
    bool voice_search_enabled() const { return m_voice_search_enabled; }
    void set_voice_search_enabled(bool enabled) { m_voice_search_enabled = enabled; }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    void filter_suggestions();
    void show_suggestions();
    void hide_suggestions();
    
    FluentTextField *m_text_field;
    std::string m_placeholder;
    std::vector<std::string> m_all_suggestions;
    std::vector<std::string> m_filtered_suggestions;
    bool m_voice_search_enabled;
    bool m_suggestions_visible;
    Widget *m_suggestions_popup;
    std::function<void(const std::string&)> m_callback;
};

NAMESPACE_END(nanogui)
