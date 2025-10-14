/*
    nanogui/m3_bottom_sheet.h -- M3 Bottom Sheet

    Based on: https://m3.material.io/components/bottom-sheets
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3BottomSheet : public Popup {
public:
    M3BottomSheet(Widget *parent, const std::string &title = "");

    void set_title(const std::string &title) { m_title = title; }
    const std::string &title() const { return m_title; }

    void show();
    void hide();

    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;

    std::string m_title;
};

NAMESPACE_END(nanogui)
