#include <flexui/toggle.h>

namespace flexui {

Toggle::Toggle(float x, float y, bool initialState, const ToggleStyle& style)
    : x_(x), y_(y), on_(initialState), style_(style) {
}

void Toggle::draw(NVGcontext* vg) {
    // Toggle background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, y_, style_.width, style_.height, style_.borderRadius);
    nvgFillColor(vg, on_ ? style_.colorOn : style_.colorOff);
    nvgFill(vg);
    
    // Knob position based on state
    float padding = (style_.height - style_.knobSize) / 2;
    float knobX = on_ ? x_ + style_.width - style_.knobSize - padding : x_ + padding;
    float knobY = y_ + padding;
    
    // Draw knob circle
    nvgBeginPath(vg);
    nvgCircle(vg, knobX + style_.knobSize / 2, knobY + style_.knobSize / 2, style_.knobSize / 2);
    nvgFillColor(vg, style_.knobColor);
    nvgFill(vg);
}

bool Toggle::handleClick(float mx, float my) {
    if (mx >= x_ && mx <= x_ + style_.width && my >= y_ && my <= y_ + style_.height) {
        on_ = !on_;
        if (change_callback_) {
            change_callback_(on_);
        }
        return true;
    }
    return false;
}

} // namespace flexui
