#pragma once

#include <nanogui/slider.h>
#include <nanogui/fluent_web_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 styled slider.
 */
class NANOGUI_EXPORT FluentWebSlider : public Slider {
public:
    FluentWebSlider(Widget *parent);

    void set_theme(Theme *theme) override;
    void draw(NVGcontext *ctx) override;

private:
    float m_track_height;
    float m_thumb_radius;
};

NAMESPACE_END(nanogui)

