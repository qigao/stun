#include <flexui/checkbox.h>
#include <cssbox_internal.h>
#include <algorithm>

namespace flexui {

Checkbox::Checkbox(cssboxRenderer* renderer, const std::string& id, bool initialState,
                   const CheckboxStyle& style)
    : Widget(renderer, id, "checkbox"), checked_(initialState), style_(style) {

    if (checked_) {
        addClass("checked");
    }
}

void Checkbox::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float size = std::min(el->layout.width, el->layout.height);

    NVGcolor fallbackBg = checked_ ? style_.bgColorChecked : style_.bgColorUnchecked;
    NVGcolor bgColor = cssBackground(fallbackBg);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    float borderWidth = cssBorderWidth(style_.borderWidth);
    NVGcolor fallbackBorder = checked_ ? style_.borderColorChecked : style_.borderColorUnchecked;
    NVGcolor borderColor = cssBorderColor(fallbackBorder);

    // Checkbox box
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, size, size, borderRadius);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, borderWidth);
    nvgStroke(vg);

    // Checkmark if checked
    if (checked_) {
        NVGcolor checkColor = cssColor(style_.checkmarkColor);
        float checkWidth = el->style.svg_stroke.width > 0 ? el->style.svg_stroke.width : style_.checkmarkWidth;

        float margin = size * 0.25f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + margin, y + size * 0.5f);
        nvgLineTo(vg, x + size * 0.42f, y + size * 0.67f);
        nvgLineTo(vg, x + size - margin, y + size * 0.33f);
        nvgStrokeColor(vg, checkColor);
        nvgStrokeWidth(vg, checkWidth);
        nvgStroke(vg);
    }
}

bool Checkbox::onClicked() {
    // Toggle checked state
    checked_ = !checked_;
    if (checked_) addClass("checked"); else removeClass("checked");

    // Trigger change callback
    if (change_callback_) change_callback_(checked_);

    // Call base to trigger user click callback
    return Widget::onClicked();
}

} // namespace flexui
