#pragma once

#include <nanovg.h>
#include <functional>

namespace flexui {

struct ToggleStyle {
    float width = 60;
    float height = 32;
    float borderRadius = 16;
    NVGcolor colorOff = nvgRGB(204, 204, 204);
    NVGcolor colorOn = nvgRGB(76, 175, 80);
    NVGcolor knobColor = nvgRGB(255, 255, 255);
    float knobSize = 24;
};

class Toggle {
public:
    using ChangeCallback = std::function<void(bool)>;

    Toggle(float x, float y, bool initialState = false, const ToggleStyle& style = ToggleStyle());
    
    void draw(NVGcontext* vg);
    bool handleClick(float mx, float my);
    
    void setState(bool on) { on_ = on; }
    bool getState() const { return on_; }
    
    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setStyle(const ToggleStyle& style) { style_ = style; }

private:
    float x_, y_;
    bool on_;
    ToggleStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
