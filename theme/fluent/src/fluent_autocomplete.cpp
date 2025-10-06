#include <nanogui/fluent_autocomplete.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_list_item.h>
#include <nanogui/opengl.h>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

FluentAutocomplete::FluentAutocomplete(Widget *parent, const std::string &placeholder)
    : FluentTextField(parent, ""),  // Style removed - not in base constructor
      m_max_suggestions(5), m_suggestions_visible(false) {
    
    set_placeholder(placeholder);
    
    // Create suggestions popup
    m_suggestions_popup = new Widget(this);
    BoxLayout *layout = new BoxLayout(Orientation::Vertical, ::nanogui::Alignment::Fill, 0, 0);
    m_suggestions_popup->set_layout(layout);
    m_suggestions_popup->set_visible(false);
    
    // Override text field callback to filter suggestions
    FluentTextField::set_callback([this](const std::string &value) {
        filter_suggestions(value);
        if (m_text_callback)
            m_text_callback(value);
        return true;
    });
}

void FluentAutocomplete::set_suggestions(const std::vector<std::string> &suggestions) {
    m_all_suggestions = suggestions;
    filter_suggestions(value());
}

void FluentAutocomplete::set_callback(const std::function<void(const std::string&)> &callback) {
    m_text_callback = callback;
}

void FluentAutocomplete::set_selection_callback(const std::function<void(const std::string&)> &callback) {
    m_selection_callback = callback;
}

void FluentAutocomplete::filter_suggestions(const std::string &query) {
    m_filtered_suggestions.clear();
    
    if (query.empty()) {
        m_suggestions_visible = false;
        m_suggestions_popup->set_visible(false);
        return;
    }
    
    // Case-insensitive filtering
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    for (const auto &suggestion : m_all_suggestions) {
        std::string lower_suggestion = suggestion;
        std::transform(lower_suggestion.begin(), lower_suggestion.end(), 
                      lower_suggestion.begin(), ::tolower);
        
        if (lower_suggestion.find(lower_query) != std::string::npos) {
            m_filtered_suggestions.push_back(suggestion);
            if (m_filtered_suggestions.size() >= m_max_suggestions)
                break;
        }
    }
    
    update_suggestions_popup();
}

void FluentAutocomplete::update_suggestions_popup() {
    // Clear existing suggestions
    while (m_suggestions_popup->child_count() > 0) {
        m_suggestions_popup->remove_child(0);
    }
    
    if (m_filtered_suggestions.empty()) {
        m_suggestions_visible = false;
        m_suggestions_popup->set_visible(false);
        return;
    }
    
    // Add suggestion items
    for (const auto &suggestion : m_filtered_suggestions) {
        auto item = new FluentListItem(m_suggestions_popup, suggestion);
        item->set_callback([this, suggestion]() {
            select_suggestion(suggestion);
        });
    }
    
    // Position popup below text field
    Vector2i pos = absolute_position();
    m_suggestions_popup->set_position(Vector2i(pos.x(), pos.y() + m_size.y()));
    m_suggestions_popup->set_fixed_width(m_size.x());
    
    m_suggestions_visible = true;
    m_suggestions_popup->set_visible(true);
}

void FluentAutocomplete::select_suggestion(const std::string &suggestion) {
    set_value(suggestion);
    m_suggestions_visible = false;
    m_suggestions_popup->set_visible(false);
    
    if (m_selection_callback)
        m_selection_callback(suggestion);
}

bool FluentAutocomplete::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (action == GLFW_PRESS && m_suggestions_visible) {
        if (key == GLFW_KEY_ESCAPE) {
            m_suggestions_visible = false;
            m_suggestions_popup->set_visible(false);
            return true;
        }
        
        // Arrow key navigation could be added here
    }
    
    return FluentTextField::keyboard_event(key, scancode, action, modifiers);
}

bool FluentAutocomplete::focus_event(bool focused) {
    if (!focused && m_suggestions_visible) {
        // Delay hiding to allow click on suggestion
        m_suggestions_visible = false;
        m_suggestions_popup->set_visible(false);
    }
    
    return FluentTextField::focus_event(focused);
}

void FluentAutocomplete::draw(NVGcontext *ctx) {
    FluentTextField::draw(ctx);
    
    // Suggestions popup is drawn separately as it's a child of screen
}

NAMESPACE_END(nanogui)
