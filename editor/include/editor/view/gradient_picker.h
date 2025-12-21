/*
 * Gradient Picker
 *
 * UI component for editing linear and radial gradients.
 * Supports color stop editing with drag handles.
 */

#pragma once

#include "../core/types.h"
#include "color_picker.h"
#include <flex/flex.h>
#include <functional>
#include <vector>

namespace editor {

enum class GradientType {
    Linear,
    Radial
};

class GradientPicker {
public:
    GradientPicker() : width_(280), height_(320) {}

    float width() const { return width_; }
    float height() const { return height_; }

    bool visible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }

    GradientType gradientType() const { return type_; }
    void setGradientType(GradientType t);

    const flex::LinearGradient& linearGradient() const { return linear_; }
    void setLinearGradient(const flex::LinearGradient& g);

    const flex::RadialGradient& radialGradient() const { return radial_; }
    void setRadialGradient(const flex::RadialGradient& g);

    void onChange(std::function<void()> cb) { onChange_ = std::move(cb); }
    void onClose(std::function<void()> cb) { onClose_ = std::move(cb); }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    void renderGradientBar(flex::Renderer& renderer, float x, float y, float w, float h);
    void renderColorStops(flex::Renderer& renderer, float x, float y, float w, float h);
    void renderDirectionControl(flex::Renderer& renderer, float x, float y, float size);
    void renderTypeSelector(flex::Renderer& renderer, float x, float y);

    int hitTestStop(float mx, float my) const;
    void updateSelectedStopColor(const Color& c);
    void notifyChange();

    float width_, height_;
    float x_ = 0, y_ = 0;
    bool visible_ = false;

    GradientType type_ = GradientType::Linear;
    flex::LinearGradient linear_;
    flex::RadialGradient radial_;

    Rect bar_rect_;
    Rect direction_rect_;
    std::vector<Rect> stop_rects_;

    int selected_stop_ = 0;
    bool dragging_stop_ = false;
    bool dragging_direction_ = false;
    bool editing_color_ = false;

    ColorPicker color_picker_;

    std::function<void()> onChange_;
    std::function<void()> onClose_;
};

} // namespace editor
