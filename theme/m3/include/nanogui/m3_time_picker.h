/*
    nanogui/m3_time_picker.h -- M3 Time Picker

    Based on: https://m3.material.io/components/time-pickers
*/

#pragma once

#include <nanogui/popup.h>
#include <nanogui/m3_theme.h>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3TimePicker : public Popup {
public:
    enum class Mode {
        Hour,
        Minute
    };

    M3TimePicker(Widget *parent, bool use_24_hour = false);

    void set_time(int hour, int minute);
    void get_time(int &hour, int &minute) const;

    void set_callback(const std::function<void(int, int)> &callback) { m_callback = callback; }

    void show();
    void hide();

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
    void draw(NVGcontext *ctx) override;

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;

    M3Theme *m3_theme() const;
    int value_at_position(const Vector2i &p) const;

    int m_hour;
    int m_minute;
    bool m_use_24_hour;
    Mode m_mode;
    bool m_dragging = false;
    int m_hover_value = -1;
    std::function<void(int, int)> m_callback;
};

NAMESPACE_END(nanogui)
