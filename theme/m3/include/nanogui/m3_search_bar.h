/*
    nanogui/m3_search_bar.h -- M3 Search Bar

    Based on: https://m3.material.io/components/search
*/

#pragma once

#include <nanogui/textbox.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3SearchBar : public TextBox {
public:
    M3SearchBar(Widget *parent, const std::string &placeholder = "Search");

    void set_placeholder(const std::string &placeholder) { m_placeholder = placeholder; }
    const std::string &placeholder() const { return m_placeholder; }

    void set_leading_icon(int icon) { m_leading_icon = icon; }
    void set_trailing_icon(int icon) { m_trailing_icon = icon; }

    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;

    std::string m_placeholder;
    int m_leading_icon = 0xf002; // Search icon
    int m_trailing_icon = 0;
};

NAMESPACE_END(nanogui)
