#include <flexui/radiobutton.h>
#include <flexui/screen.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

RadioButton::RadioButton(NVGCSSRenderer* renderer, const std::string& id,
                         const std::string& group, const std::string& value,
                         bool checked, const RadioButtonStyle& style)
    : Widget(renderer, id, "radio"),
      group_(group),
      value_(value),
      checked_(checked),
      style_(style) {

    if (checked_) {
        addClass("checked");
    }
}

void RadioButton::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    float size = std::min(w, h);
    float radius = size / 2;
    float cx = x + w / 2;
    float cy = y + h / 2;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    float borderWidth = cssBorderWidth(style_.borderWidth);
    NVGcolor fallbackBorder = checked_ ? style_.borderColorChecked : style_.borderColor;
    NVGcolor borderColor = cssBorderColor(fallbackBorder);

    // Outer circle background
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, radius);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, borderWidth);
    nvgStroke(vg);

    // Inner dot if checked
    if (checked_) {
        NVGcolor dotColor = cssColor(style_.dotColor);
        float dotRadius = radius * style_.dotRadiusRatio;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, dotRadius);
        nvgFillColor(vg, dotColor);
        nvgFill(vg);
    }
}

void RadioButton::setChecked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        if (checked_) addClass("checked"); else removeClass("checked");

        // If being checked and part of a group, notify Screen to uncheck others
        if (checked && screen_ && !group_.empty()) {
            screen_->setRadioGroupValue(group_, value_);
        }
    }
}

bool RadioButton::onClicked() {
    // Set this radio button as checked (will uncheck others in group)
    setChecked(true);

    // Call base to trigger user click callback
    return Widget::onClicked();
}

} // namespace flexui
