/*
    nanogui/m3_top_app_bar.h -- M3 Top App Bar

    Based on: https://m3.material.io/components/top-app-bar
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3TopAppBar : public Widget {
public:
    enum class Type {
        Small,   ///< 64dp height
        Medium,  ///< 112dp height
        Large    ///< 152dp height
    };

    M3TopAppBar(Widget *parent, const std::string &title = "", Type type = Type::Small);

    void set_title(const std::string &title) { m_title = title; }
    const std::string &title() const { return m_title; }

    void set_type(Type type);
    Type type() const { return m_type; }

    void set_leading_icon(int icon) { m_leading_icon = icon; }
    void set_trailing_icon(int icon) { m_trailing_icon = icon; }

    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;

    std::string m_title;
    Type m_type;
    int m_leading_icon = 0;
    int m_trailing_icon = 0;
};

NAMESPACE_END(nanogui)
