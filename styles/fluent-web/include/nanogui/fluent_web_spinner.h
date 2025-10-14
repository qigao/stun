#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_theme.h>

#include <chrono>

NAMESPACE_BEGIN(nanogui)

/**
 * Fluent 2 circular activity spinner.
 */
class NANOGUI_EXPORT FluentWebSpinner : public Widget {
public:
    enum class Size {
        XSmall,
        Small,
        Medium,
        Large
    };

    FluentWebSpinner(Widget *parent,
                     Size size = Size::Medium);

    void set_size(Size size);
    Size size_category() const { return m_size_category; }

    void set_theme(Theme *theme) override;
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    void draw(NVGcontext *ctx) override;

private:
    void update_dimensions();

    Size m_size_category;
    float m_radius;
    float m_stroke_width;
    std::chrono::steady_clock::time_point m_start_time;
};

NAMESPACE_END(nanogui)

