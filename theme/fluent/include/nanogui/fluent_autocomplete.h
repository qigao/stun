#pragma once

#include <nanogui/fluent_text_field.h>
#include <vector>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Autocomplete
 * 
 * Text field with filtered suggestions dropdown.
 * Provides real-time suggestions as user types.
 */
class NANOGUI_EXPORT FluentAutocomplete : public FluentTextField {
public:
    FluentAutocomplete(Widget *parent, const std::string &placeholder = "");
    
    /// Set available suggestions
    void set_suggestions(const std::vector<std::string> &suggestions);
    
    /// Get current suggestions
    const std::vector<std::string> &suggestions() const { return m_all_suggestions; }
    
    /// Maximum number of suggestions to show
    size_t max_suggestions() const { return m_max_suggestions; }
    void set_max_suggestions(size_t max) { m_max_suggestions = max; }
    
    /// Callback when text changes
    void set_callback(const std::function<void(const std::string&)> &callback);
    
    /// Callback when suggestion is selected
    std::function<void(const std::string&)> selection_callback() const { return m_selection_callback; }
    void set_selection_callback(const std::function<void(const std::string&)> &callback);
    
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    bool focus_event(bool focused) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    void filter_suggestions(const std::string &query);
    void update_suggestions_popup();
    void select_suggestion(const std::string &suggestion);
    
    std::vector<std::string> m_all_suggestions;
    std::vector<std::string> m_filtered_suggestions;
    size_t m_max_suggestions;
    bool m_suggestions_visible;
    
    Widget *m_suggestions_popup;
    std::function<void(const std::string&)> m_text_callback;
    std::function<void(const std::string&)> m_selection_callback;
};

NAMESPACE_END(nanogui)
