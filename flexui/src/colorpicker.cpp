#include <flexui/colorpicker.h>
#include <cssbox_internal.h>
#include <algorithm>
#include <cmath>

namespace flexui {

// HSV to RGB conversion (h: 0-1, s: 0-1, v: 0-1)
static NVGcolor hsvToRgb(float h, float s, float v) {
    float r, g, b;

    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: r = v; g = t; b = p; break;
    }

    return nvgRGBf(r, g, b);
}

ColorPicker::ColorPicker(cssboxRenderer* renderer, const std::string& id,
                         const ColorPickerStyle& style)
    : Widget(renderer, id, "colorpicker"), style_(style) {
}

NVGcolor ColorPicker::getColor() const {
    return hsvToRgb(hue_, saturation_, lightness_);
}

void ColorPicker::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    if (w <= 0 || h <= 0) return;

    // Calculate sizes based on actual layout
    float spacing = 8.0f;
    float sliderHeight = 16.0f;
    float previewHeight = 20.0f;
    float squareSize = std::min(w, h - sliderHeight - previewHeight - spacing * 2);
    if (squareSize < 20) squareSize = std::min(w, h * 0.6f);

    // Draw saturation/value square using HSV model
    // Horizontal: white to pure hue (saturation 0->1)
    // Vertical: color to black (value 1->0)
    NVGcolor pureHue = hsvToRgb(hue_, 1.0f, 1.0f);

    // Layer 1: White to pure hue (horizontal gradient)
    NVGpaint satPaint = nvgLinearGradient(vg, x, y, x + squareSize, y,
                                           nvgRGB(255, 255, 255), pureHue);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, squareSize, squareSize);
    nvgFillPaint(vg, satPaint);
    nvgFill(vg);

    // Layer 2: Transparent to black (vertical gradient)
    NVGpaint valPaint = nvgLinearGradient(vg, x, y, x, y + squareSize,
                                           nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 255));
    nvgBeginPath(vg);
    nvgRect(vg, x, y, squareSize, squareSize);
    nvgFillPaint(vg, valPaint);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRect(vg, x, y, squareSize, squareSize);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Cursor on saturation/value square
    float cx = x + saturation_ * squareSize;
    float cy = y + (1.0f - lightness_) * squareSize;  // lightness_ is actually "value" in HSV
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 5);
    nvgStrokeColor(vg, nvgRGB(255, 255, 255));
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 4);
    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Hue slider (rainbow gradient) - draw 6 segments for full spectrum
    float sliderY = y + squareSize + spacing;
    for (int i = 0; i < 6; i++) {
        float hue1 = i / 6.0f;
        float hue2 = (i + 1) / 6.0f;
        float sx1 = x + (squareSize * i / 6.0f);
        float sx2 = x + (squareSize * (i + 1) / 6.0f);
        NVGpaint hueGrad = nvgLinearGradient(vg, sx1, sliderY, sx2, sliderY,
                                              hsvToRgb(hue1, 1.0f, 1.0f),
                                              hsvToRgb(hue2, 1.0f, 1.0f));
        nvgBeginPath(vg);
        nvgRect(vg, sx1, sliderY, sx2 - sx1 + 1, sliderHeight);  // +1 to avoid gaps
        nvgFillPaint(vg, hueGrad);
        nvgFill(vg);
    }

    // Hue slider border
    nvgBeginPath(vg);
    nvgRect(vg, x, sliderY, squareSize, sliderHeight);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Hue cursor
    float hx = x + hue_ * squareSize;
    nvgBeginPath(vg);
    nvgRect(vg, hx - 2, sliderY - 1, 4, sliderHeight + 2);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(0, 0, 0));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Color preview
    float previewY = sliderY + sliderHeight + spacing;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, previewY, squareSize, previewHeight, 4);
    nvgFillColor(vg, hsvToRgb(hue_, saturation_, lightness_));
    nvgFill(vg);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Store squareSize for mouse handling
    style_.size = squareSize;
}

bool ColorPicker::handleMouseDown(float mx, float my) {
    float x, y;
    getVisualPosition(x, y);
    float spacing = 8.0f;
    float sliderHeight = 16.0f;
    float sliderY = y + style_.size + spacing;

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
        my >= sliderY && my <= sliderY + sliderHeight) {
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

    float x, y;
    getVisualPosition(x, y);

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
