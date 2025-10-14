#pragma once

#include <nanogui/checkbox.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 style toggle switch.
 *
 * Inherits CheckBox behaviour (callbacks, state) but renders a Fluent web
 * track/thumb with appropriate tokens for hover/pressed/disabled states.
 */
class NANOGUI_EXPORT FluentWebSwitch : public CheckBox {
public:
    FluentWebSwitch(Widget *parent,
                    const std::string &caption = "",
                    const std::function<void(bool)> &callback = {});

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_metrics();

    float m_track_width = 36.f;
    float m_track_height = 20.f;
    float m_thumb_diameter = 16.f;
    int m_font_override = -1;
};

NAMESPACE_END(nanogui)

