/*
    nanogui/m3_date_picker.h -- M3 Date Picker

    Based on: https://m3.material.io/components/date-pickers
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>
#include <functional>
#include <ctime>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3DatePicker : public Popup {
public:
    M3DatePicker(Widget *parent);

    void set_date(int year, int month, int day);
    void get_date(int &year, int &month, int &day) const;

    void set_callback(const std::function<void(int, int, int)> &callback) { m_callback = callback; }

    void show();
    void hide();

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    M3Theme *m3_theme() const;
    int day_at_position(const Vector2i &p) const;
    int days_in_month(int year, int month) const;
    int first_day_of_month(int year, int month) const;

    int m_year;
    int m_month;
    int m_day;
    int m_hover_day = -1;
    std::function<void(int, int, int)> m_callback;
};

NAMESPACE_END(nanogui)
