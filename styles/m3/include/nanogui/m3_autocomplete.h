/*
    nanogui/m3_autocomplete.h -- M3 Autocomplete

    Based on: https://m3.material.io/components/menus (Autocomplete pattern)
*/

#pragma once

#include <nanogui/textbox.h>
#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3Autocomplete : public TextBox {
public:
    struct Suggestion {
        std::string text;
        std::string secondary;
        int icon;
        
        Suggestion(const std::string &t, const std::string &s = "", int i = 0)
            : text(t), secondary(s), icon(i) {}
    };

    M3Autocomplete(Widget *parent, const std::string &placeholder = "");

    void set_suggestions(const std::vector<Suggestion> &suggestions);
    void set_filter_callback(const std::function<std::vector<Suggestion>(const std::string&)> &callback);
    void set_select_callback(const std::function<void(const Suggestion&)> &callback);

    bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    bool focus_event(bool focused) override;
    void draw(NVGcontext *ctx) override;

protected:
    M3Theme *m3_theme() const;
    void update_suggestions();
    void show_popup();
    void hide_popup();

    class SuggestionPopup;
    SuggestionPopup *m_popup;
    std::vector<Suggestion> m_suggestions;
    std::function<std::vector<Suggestion>(const std::string&)> m_filter_callback;
    std::function<void(const Suggestion&)> m_select_callback;
    int m_selected_index = -1;
};

NAMESPACE_END(nanogui)
