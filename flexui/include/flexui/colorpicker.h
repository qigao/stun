#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct ColorPickerStyle {
    float size = 200;
    float sliderHeight = 20;
    float spacing = 10;
    NVGcolor borderColor = nvgRGB(200, 200, 200);
};

class ColorPicker : public Widget {
public:
    using ChangeCallback = std::function<void(NVGcolor)>;

    ColorPicker(NVGCSSRenderer* renderer, const std::string& id,
                const ColorPickerStyle& style = ColorPickerStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;
    bool handleMouseMove(float mx, float my) override;
    bool handleMouseUp(float mx, float my) override;

    NVGcolor getColor() const { return nvgHSLA(hue_, saturation_, lightness_, 1.0f); }
    void setColor(float h, float s, float l) { hue_ = h; saturation_ = s; lightness_ = l; }
    void setChangeCallback(ChangeCallback callback) { change_callback_ = callback; }

private:
    float hue_ = 0.0f;
    float saturation_ = 1.0f;
    float lightness_ = 0.5f;
    bool dragging_ = false;
    ColorPickerStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
