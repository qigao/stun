#include <flexui/checkbox.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

Checkbox::Checkbox(NVGCSSRenderer* renderer, const std::string& id, bool initialState,
                   const CheckboxStyle& style)
    : Widget(renderer, id, "checkbox"), checked_(initialState), style_(style) {

    // Set up click handler to toggle state
    setClickCallback([this](Widget*) {
        checked_ = !checked_;
        if (change_callback_) {
            change_callback_(checked_);
        }
        return true;
    });
}

void Checkbox::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float size = std::min(el->computed.width, el->computed.height); // Use smaller dimension

    // Checkbox box
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, style_.borderRadius);
    nvgFillColor(vg, checked_ ? style_.bgColorChecked : style_.bgColorUnchecked);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, style_.borderRadius);
    nvgStrokeColor(vg, checked_ ? style_.borderColorChecked : style_.borderColorUnchecked);
    nvgStrokeWidth(vg, style_.borderWidth);
    nvgStroke(vg);

    // Checkmark if checked
    if (checked_) {
        float margin = size * 0.25f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + margin, y + size * 0.5f);
        nvgLineTo(vg, x + size * 0.42f, y + size * 0.67f);
        nvgLineTo(vg, x + size - margin, y + size * 0.33f);
        nvgStrokeColor(vg, style_.checkmarkColor);
        nvgStrokeWidth(vg, style_.checkmarkWidth);
        nvgStroke(vg);
    }
}

bool Checkbox::handleCheckboxClick(float mx, float my) {
    return handleClick(mx, my);
}

} // namespace flexui
