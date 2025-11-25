#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <chrono>

namespace flexui {

struct SpinnerStyle {
    float size = 32;
    float thickness = 3;
    NVGcolor color = nvgRGB(33, 150, 243);
    float speed = 2.0f;
};

class Spinner : public Widget {
public:
    Spinner(NVGCSSRenderer* renderer, const std::string& id,
            const SpinnerStyle& style = SpinnerStyle());

    void draw(NVGcontext* vg) override;
    void setSpinnerStyle(const SpinnerStyle& style) { style_ = style; }

private:
    SpinnerStyle style_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace flexui
