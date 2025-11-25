#include <flexui/colorpicker.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

ColorPicker::ColorPicker(NVGCSSRenderer* renderer, const std::string& id,
                         const ColorPickerStyle& style)
    : Widget(renderer, id, "colorpicker"), style_(style) {
}

void ColorPicker::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    // Draw saturation/value square
    NVGcolor pureColor = nvgHSLA(hue_, 1.0f, 0.5f, 1.0f);
    NVGpaint satPaint = nvgLinearGradient(vg, x, y, x + style_.size, y,
                                           nvgRGBA(255, 255, 255, 255), pureColor);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, style_.size, style_.size);
    nvgFillPaint(vg, satPaint);
    nvgFill(vg);

    // Second layer: transparent to black (vertical)
    NVGpaint valPaint = nvgLinearGradient(vg, x, y, x, y + style_.size,
                                           nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 255));
    nvgBeginPath(vg);
    nvgRect(vg, x, y, style_.size, style_.size);
    nvgFillPaint(vg, valPaint);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRect(vg, x, y, style_.size, style_.size);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Cursor
    float cx = x + saturation_ * style_.size;
    float cy = y + (1.0f - lightness_) * style_.size;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 5);
    nvgStrokeColor(vg, nvgRGB(255, 255, 255));
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);

    // Hue slider
    float sliderY = y + style_.size + style_.spacing;
    NVGpaint hueGrad = nvgLinearGradient(vg, x, sliderY, x + style_.size, sliderY,
                                          nvgHSLA(0.0f, 1.0f, 0.5f, 1.0f),
                                          nvgHSLA(1.0f, 1.0f, 0.5f, 1.0f));
    nvgBeginPath(vg);
    nvgRect(vg, x, sliderY, style_.size, style_.sliderHeight);
    nvgFillPaint(vg, hueGrad);
    nvgFill(vg);

    // Hue slider border
    nvgBeginPath(vg);
    nvgRect(vg, x, sliderY, style_.size, style_.sliderHeight);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Hue cursor
    float hx = x + hue_ * style_.size;
    nvgBeginPath(vg);
    nvgRect(vg, hx - 2, sliderY, 4, style_.sliderHeight);
    nvgStrokeColor(vg, nvgRGB(255, 255, 255));
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);

    // Color preview
    float previewY = sliderY + style_.sliderHeight + style_.spacing;
    nvgBeginPath(vg);
    nvgRect(vg, x, previewY, style_.size, 30);
    nvgFillColor(vg, getColor());
    nvgFill(vg);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
}

bool ColorPicker::handleMouseDown(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float sliderY = y + style_.size + style_.spacing;

    // Check saturation/lightness square
    if (mx >= x && mx <= x + style_.size &&
        my >= y && my <= y + style_.size) {
        saturation_ = std::clamp((mx - x) / style_.size, 0.0f, 1.0f);
        lightness_ = std::clamp(1.0f - (my - y) / style_.size, 0.0f, 1.0f);
        dragging_ = true;
        if (change_callback_) {
            change_callback_(getColor());
        }
        return true;
    }

    // Check hue slider
    if (mx >= x && mx <= x + style_.size &&
        my >= sliderY && my <= sliderY + style_.sliderHeight) {
        hue_ = std::clamp((mx - x) / style_.size, 0.0f, 1.0f);
        if (change_callback_) {
            change_callback_(getColor());
        }
        return true;
    }

    return false;
}

bool ColorPicker::handleMouseMove(float mx, float my) {
    if (!dragging_) return false;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;

    saturation_ = std::clamp((mx - x) / style_.size, 0.0f, 1.0f);
    lightness_ = std::clamp(1.0f - (my - y) / style_.size, 0.0f, 1.0f);

    if (change_callback_) {
        change_callback_(getColor());
    }
    return true;
}

bool ColorPicker::handleMouseUp(float mx, float my) {
    dragging_ = false;
    return false;
}

} // namespace flexui
