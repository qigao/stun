/*
    src/m3_autocomplete.cpp -- M3 Autocomplete implementation
*/

#include <nanogui/m3_autocomplete.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

class M3Autocomplete::SuggestionPopup : public Popup {
public:
    SuggestionPopup(Widget *parent, M3Autocomplete *autocomplete)
        : Popup(parent), m_autocomplete(autocomplete) {
        set_modal(false);
    }

    void draw(NVGcontext *ctx) override {
        if (m_autocomplete->m_suggestions.empty()) return;

        M3Theme *theme = dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
        if (!theme) return;

        float x = m_pos.x(), y = m_pos.y(), w = m_size.x();
        float item_height = 56;
        float corner = theme->corner_radius(M3Theme::ShapeFamily::Small);

        nvgSave(ctx);

        // Background
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, m_size.y(), corner);
        nvgFillColor(ctx, theme->surface());
        nvgFill(ctx);

        // Elevation
        Color tint = theme->elevation_tint(M3Theme::Elevation::Level2);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, m_size.y(), corner);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);

        // Items
        for (size_t i = 0; i < m_autocomplete->m_suggestions.size() && i < 8; ++i) {
            const auto &item = m_autocomplete->m_suggestions[i];
            float item_y = y + i * item_height;
            bool selected = (static_cast<int>(i) == m_autocomplete->m_selected_index);

            if (selected) {
                nvgBeginPath(ctx);
                nvgRect(ctx, x, item_y, w, item_height);
                nvgFillColor(ctx, theme->state_layer(theme->on_surface(), 0.12f));
                nvgFill(ctx);
            }

            float content_x = x + 16;

            // Icon
            if (item.icon) {
                nvgFontSize(ctx, 24);
                nvgFontFace(ctx, "icons");
                nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor(ctx, theme->on_surface_variant());
                nvgText(ctx, content_x, item_y + item_height * 0.5f, utf8(item.icon).data(), nullptr);
                content_x += 40;
            }

            // Text
            nvgFontSize(ctx, 16);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, theme->on_surface());
            
            if (item.secondary.empty()) {
                nvgText(ctx, content_x, item_y + item_height * 0.5f, item.text.c_str(), nullptr);
            } else {
                nvgText(ctx, content_x, item_y + 20, item.text.c_str(), nullptr);
                nvgFontSize(ctx, 14);
                nvgFillColor(ctx, theme->on_surface_variant());
                nvgText(ctx, content_x, item_y + 38, item.secondary.c_str(), nullptr);
            }
        }

        nvgRestore(ctx);
    }

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override {
        if (!down || button != NANOGUI_MOUSE_BUTTON_LEFT) return false;

        Vector2i local = p - m_pos;
        int idx = static_cast<int>(local.y() / 56);
        
        if (idx >= 0 && idx < static_cast<int>(m_autocomplete->m_suggestions.size()) && idx < 8) {
            if (m_autocomplete->m_select_callback) {
                m_autocomplete->m_select_callback(m_autocomplete->m_suggestions[idx]);
            }
            m_autocomplete->set_value(m_autocomplete->m_suggestions[idx].text);
            m_autocomplete->hide_popup();
            return true;
        }
        return false;
    }

private:
    M3Autocomplete *m_autocomplete;
};

M3Autocomplete::M3Autocomplete(Widget *parent, const std::string &placeholder)
    : TextBox(parent, placeholder) {
    m_popup = new SuggestionPopup(screen(), this);
    m_popup->set_visible(false);
}

M3Theme *M3Autocomplete::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3Autocomplete::set_suggestions(const std::vector<Suggestion> &suggestions) {
    m_suggestions = suggestions;
}

void M3Autocomplete::set_filter_callback(const std::function<std::vector<Suggestion>(const std::string&)> &callback) {
    m_filter_callback = callback;
}

void M3Autocomplete::set_select_callback(const std::function<void(const Suggestion&)> &callback) {
    m_select_callback = callback;
}

void M3Autocomplete::update_suggestions() {
    if (m_filter_callback) {
        m_suggestions = m_filter_callback(m_value);
    }
    
    if (m_suggestions.empty()) {
        hide_popup();
    } else {
        show_popup();
    }
}

void M3Autocomplete::show_popup() {
    if (m_suggestions.empty()) return;

    Vector2i pos = absolute_position();
    int max_items = std::min(static_cast<int>(m_suggestions.size()), 8);
    m_popup->set_position(Vector2i(pos.x(), pos.y() + m_size.y()));
    m_popup->set_size(Vector2i(m_size.x(), max_items * 56));
    m_popup->set_visible(true);
}

void M3Autocomplete::hide_popup() {
    m_popup->set_visible(false);
    m_selected_index = -1;
}

bool M3Autocomplete::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (m_popup->visible()) {
            if (key == GLFW_KEY_DOWN) {
                m_selected_index = std::min(m_selected_index + 1, 
                                           static_cast<int>(m_suggestions.size()) - 1);
                return true;
            } else if (key == GLFW_KEY_UP) {
                m_selected_index = std::max(m_selected_index - 1, 0);
                return true;
            } else if (key == GLFW_KEY_ENTER && m_selected_index >= 0) {
                if (m_select_callback) {
                    m_select_callback(m_suggestions[m_selected_index]);
                }
                set_value(m_suggestions[m_selected_index].text);
                hide_popup();
                return true;
            } else if (key == GLFW_KEY_ESCAPE) {
                hide_popup();
                return true;
            }
        }
    }

    bool result = TextBox::keyboard_event(key, scancode, action, modifiers);
    update_suggestions();
    return result;
}

bool M3Autocomplete::focus_event(bool focused) {
    if (!focused) {
        hide_popup();
    }
    return TextBox::focus_event(focused);
}

void M3Autocomplete::draw(NVGcontext *ctx) {
    TextBox::draw(ctx);
}

NAMESPACE_END(nanogui)
