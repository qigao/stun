#include <nanogui/fluent_search_bar.h>
#include <nanogui/icons.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/layout.h>
#include <nanogui/fluent_icon_button.h>
#include <nanogui/fluent_list_item.h>
#include <nanogui/opengl.h>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

FluentSearchBar::FluentSearchBar(Widget *parent)
    : Widget(parent), m_placeholder("Search"), 
      m_voice_search_enabled(false), m_suggestions_visible(false) {
    
    set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 8));
    
    // Search icon
    auto search_icon = new FluentIconButton(this, FA_SEARCH);
    (void)search_icon;  // Suppress unused warning
    
    // Text field
    m_text_field = new FluentTextField(this, "");  // Style removed
    m_text_field->set_placeholder(m_placeholder);
    m_text_field->set_callback([this](const std::string &value) {
        filter_suggestions();
        if (m_callback)
            m_callback(value);
        return true;
    });
    
    // Voice search button (optional)
    if (m_voice_search_enabled) {
        auto voice_btn = new FluentIconButton(this, FA_MICROPHONE);
        (void)voice_btn;  // Suppress unused warning
    }
    
    // Suggestions popup
    m_suggestions_popup = new Widget(this);
    m_suggestions_popup->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 0));
    m_suggestions_popup->set_visible(false);
}

std::string FluentSearchBar::query() const {
    return m_text_field->value();
}

void FluentSearchBar::set_query(const std::string &query) {
    m_text_field->set_value(query);
    filter_suggestions();
}

void FluentSearchBar::set_placeholder(const std::string &placeholder) {
    m_placeholder = placeholder;
    m_text_field->set_placeholder(placeholder);
}

void FluentSearchBar::set_suggestions(const std::vector<std::string> &suggestions) {
    m_all_suggestions = suggestions;
    filter_suggestions();
}

void FluentSearchBar::filter_suggestions() {
    m_filtered_suggestions.clear();
    
    std::string query_lower = m_text_field->value();
    if (query_lower.empty()) {
        hide_suggestions();
        return;
    }
    
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    
    for (const auto &suggestion : m_all_suggestions) {
        std::string suggestion_lower = suggestion;
        std::transform(suggestion_lower.begin(), suggestion_lower.end(), 
                      suggestion_lower.begin(), ::tolower);
        
        if (suggestion_lower.find(query_lower) != std::string::npos) {
            m_filtered_suggestions.push_back(suggestion);
            if (m_filtered_suggestions.size() >= 5)
                break;
        }
    }
    
    if (!m_filtered_suggestions.empty()) {
        show_suggestions();
    } else {
        hide_suggestions();
    }
}

void FluentSearchBar::show_suggestions() {
    // Clear existing
    while (m_suggestions_popup->child_count() > 0) {
        m_suggestions_popup->remove_child(0);
    }
    
    // Add suggestions
    for (const auto &suggestion : m_filtered_suggestions) {
        auto item = new FluentListItem(m_suggestions_popup, suggestion);
        item->set_leading_icon(FA_SEARCH);
        item->set_callback([this, suggestion]() {
            set_query(suggestion);
            hide_suggestions();
            if (m_callback)
                m_callback(suggestion);
        });
    }
    
    // Position popup
    Vector2i pos = absolute_position();
    m_suggestions_popup->set_position(Vector2i(pos.x(), pos.y() + m_size.y()));
    m_suggestions_popup->set_fixed_width(m_size.x());
    
    m_suggestions_visible = true;
    m_suggestions_popup->set_visible(true);
}

void FluentSearchBar::hide_suggestions() {
    m_suggestions_visible = false;
    m_suggestions_popup->set_visible(false);
}

Vector2i FluentSearchBar::preferred_size_impl(NVGcontext *ctx) const {
    return Vector2i(m_parent ? m_parent->width() - 32 : 400, 56);
}

bool FluentSearchBar::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE && m_suggestions_visible) {
            hide_suggestions();
            return true;
        }
        
        if (key == GLFW_KEY_ENTER) {
            hide_suggestions();
            if (m_callback)
                m_callback(m_text_field->value());
            return true;
        }
    }
    
    return Widget::keyboard_event(key, scancode, action, modifiers);
}

void FluentSearchBar::draw(NVGcontext *ctx) {
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), 28);
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
