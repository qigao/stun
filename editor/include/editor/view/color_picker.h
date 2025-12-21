/*
 * Color Picker
 *
 * Full-featured color picker with HSV selection,
 * color sliders, and hex input.
 */

#pragma once

#include "../core/types.h"
#include <flex/flex.h>
#include <string>
#include <functional>

namespace editor {

// HSV color representation
struct HSV {
    float h = 0;  // 0-360
    float s = 0;  // 0-1
    float v = 0;  // 0-1
    float a = 1;  // 0-1

    static HSV fromRGB(const Color& rgb);
    Color toRGB() const;
};

// Color Picker widget
class ColorPicker {
public:
    ColorPicker() : width_(220), height_(280) {}

    float width() const { return width_; }
    float height() const { return height_; }

    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }

    const Color& color() const { return color_; }
    void setColor(const Color& c);

    void onChange(std::function<void(const Color&)> cb) { onChange_ = std::move(cb); }
    void onClose(std::function<void()> cb) { onClose_ = std::move(cb); }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void updateSV(float mx, float my);
    void updateHue(float mx, float my);
    void updateAlpha(float mx, float my);

    void renderSVGradient(flex::Renderer& renderer, float x, float y, float w, float h);
    void renderHueGradient(flex::Renderer& renderer, float x, float y, float w, float h);
    void renderAlphaGradient(flex::Renderer& renderer, float x, float y, float w, float h);
    void renderCheckerboard(flex::Renderer& renderer, float x, float y, float w, float h);

    float width_, height_;
    float x_ = 0, y_ = 0;
    bool visible_ = false;

    Color color_ = {1, 1, 1, 1};
    HSV hsv_;

    Rect sv_rect_;
    Rect hue_rect_;
    Rect alpha_rect_;

    bool dragging_sv_ = false;
    bool dragging_hue_ = false;
    bool dragging_alpha_ = false;

    std::function<void(const Color&)> onChange_;
    std::function<void()> onClose_;
};

} // namespace editor
