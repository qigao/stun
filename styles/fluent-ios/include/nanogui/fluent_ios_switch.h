#pragma once

#include <nanogui/widget.h>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT FluentIOSSwitch : public Widget {
public:
    FluentIOSSwitch(Widget *parent, bool state = false);

    void set_state(bool state, bool emit = true);
    bool state() const { return m_state; }

    void set_callback(const std::function<void(bool)> &cb) { m_callback = cb; }

protected:
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;

private:
    bool m_state;
    bool m_pressed = false;
    bool m_dragging = false;
    float m_drag_fraction = 0.f;

    std::function<void(bool)> m_callback;
};

NAMESPACE_END(nanogui)
