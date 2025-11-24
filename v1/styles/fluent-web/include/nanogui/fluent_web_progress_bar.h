#pragma once

#include <nanogui/progressbar.h>
#include <nanogui/fluent_web_theme.h>

#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 styled progress bar (determinate/indeterminate).
 */
class NANOGUI_EXPORT FluentWebProgressBar : public ProgressBar {
public:
    FluentWebProgressBar(Widget *parent);

    void set_indeterminate(bool indeterminate);
    bool indeterminate() const { return m_indeterminate; }

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void ensure_animation_started();

    bool m_indeterminate;
    float m_track_height;
    std::chrono::steady_clock::time_point m_start_time;
};

NAMESPACE_END(nanogui)

