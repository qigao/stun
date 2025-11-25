#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct SliderStyle {
    float trackHeight = 6;
    float handleRadius = 12;
    NVGcolor trackColor = nvgRGB(224, 224, 224);
    NVGcolor fillColor = nvgRGB(33, 150, 243);
    NVGcolor handleColor = nvgRGB(33, 150, 243);
    NVGcolor handleBorderColor = nvgRGB(255, 255, 255);
    float borderWidth = 2;
    bool showValue = true;
    float fontSize = 12;
    NVGcolor textColor = nvgRGB(100, 100, 100);
};

class Slider : public Widget {
public:
    Slider(NVGCSSRenderer* renderer, const std::string& id,
           float value = 0.5f, float min = 0.0f, float max = 1.0f,
           const SliderStyle& style = SliderStyle());

    void draw(NVGcontext* vg) override;

    void setValue(float value);
    float getValue() const { return value_; }

    void setRange(float min, float max) {
        min_ = min;
        max_ = max;
        setValue(value_);  // Clamp to new range
    }

    void setValueCallback(std::function<void(float)> callback) {
        value_callback_ = callback;
    }

    void setSliderStyle(const SliderStyle& style) { style_ = style; }

    // Called by Screen on mouse events
    bool handleMouseDown(float x, float y);
    bool handleMouseMove(float x, float y);
    bool handleMouseUp(float x, float y);

private:
    float value_;  // Current value (in range [min_, max_])
    float min_;
    float max_;
    bool dragging_ = false;
    SliderStyle style_;
    std::function<void(float)> value_callback_;

    float getNormalizedValue() const {
        return (value_ - min_) / (max_ - min_);
    }

    void updateValueFromPosition(float mouseX);
};

} // namespace flexui
