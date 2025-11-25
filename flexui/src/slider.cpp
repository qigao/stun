#include <flexui/slider.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

Slider::Slider(NVGCSSRenderer* renderer, const std::string& id,
               float value, float min, float max, const SliderStyle& style)
    : Widget(renderer, id, "input"),
      value_(std::clamp(value, min, max)),
      min_(min),
      max_(max),
      style_(style) {
}

void Slider::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    // Track position (centered vertically)
    float trackY = y + (h - style_.trackHeight) / 2;
    float trackX = x + style_.handleRadius;  // Leave space for handle
    float trackWidth = w - 2 * style_.handleRadius;

    // Track background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, trackX, trackY, trackWidth, style_.trackHeight, style_.trackHeight / 2);
    nvgFillColor(vg, style_.trackColor);
    nvgFill(vg);

    // Filled track
    float normalized = getNormalizedValue();
    float fillWidth = trackWidth * normalized;
    if (fillWidth > 0) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, trackX, trackY, fillWidth, style_.trackHeight, style_.trackHeight / 2);
        nvgFillColor(vg, style_.fillColor);
        nvgFill(vg);
    }

    // Handle
    float handleX = trackX + fillWidth;
    float handleY = y + h / 2;

    // Handle border (white outline)
    nvgBeginPath(vg);
    nvgCircle(vg, handleX, handleY, style_.handleRadius);
    nvgFillColor(vg, style_.handleBorderColor);
    nvgFill(vg);

    // Handle inner circle
    nvgBeginPath(vg);
    nvgCircle(vg, handleX, handleY, style_.handleRadius - style_.borderWidth);
    nvgFillColor(vg, style_.handleColor);
    nvgFill(vg);

    // Value text (optional)
    if (style_.showValue) {
        char text[32];
        snprintf(text, sizeof(text), "%.2f", value_);

        nvgFontSize(vg, style_.fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, style_.textColor);
        nvgText(vg, handleX, y + h + 5, text, nullptr);
    }
}

void Slider::setValue(float value) {
    value_ = std::clamp(value, min_, max_);
    if (value_callback_) {
        value_callback_(value_);
    }
}

void Slider::updateValueFromPosition(float mouseX) {
    auto* el = element();
    float x = el->computed.x;
    float w = el->computed.width;

    float trackX = x + style_.handleRadius;
    float trackWidth = w - 2 * style_.handleRadius;

    float t = std::clamp((mouseX - trackX) / trackWidth, 0.0f, 1.0f);
    float newValue = min_ + t * (max_ - min_);
    setValue(newValue);
}

bool Slider::handleMouseDown(float x, float y) {
    auto* el = element();

    // Check if click is within widget bounds
    if (x >= el->computed.x && x <= el->computed.x + el->computed.width &&
        y >= el->computed.y && y <= el->computed.y + el->computed.height) {

        dragging_ = true;
        updateValueFromPosition(x);
        return true;
    }
    return false;
}

bool Slider::handleMouseMove(float x, float y) {
    if (dragging_) {
        updateValueFromPosition(x);
        return true;
    }
    return false;
}

bool Slider::handleMouseUp(float x, float y) {
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

} // namespace flexui
