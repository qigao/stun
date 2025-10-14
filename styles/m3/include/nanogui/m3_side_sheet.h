/*
    nanogui/m3_side_sheet.h -- M3 Side Sheet

    Based on: https://m3.material.io/components/side-sheets
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3SideSheet : public Popup {
public:
    enum class Side {
        Left,
        Right
    };

    M3SideSheet(Widget *parent, const std::string &title = "", Side side = Side::Right);

    void set_title(const std::string &title) { m_title = title; }
    const std::string &title() const { return m_title; }

    void set_side(Side side) { m_side = side; }
    Side side() const { return m_side; }

    void show();
    void hide();

    void draw(NVGcontext *ctx) override;

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    M3Theme *m3_theme() const;

    std::string m_title;
    Side m_side;
};

NAMESPACE_END(nanogui)
