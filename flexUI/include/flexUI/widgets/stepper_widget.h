/*
 * flexUI - StepperWidget
 *
 * A numeric stepper with increment/decrement buttons.
 *
 * CSS variables:
 *   --stepper-height: "32"
 *   --stepper-button-width: "32"
 *   --stepper-radius: "4"
 *   --stepper-bg: "r,g,b,a"
 *   --stepper-button-bg: "r,g,b,a"
 *   --stepper-button-bg-hover: "r,g,b,a"
 *   --stepper-text: "r,g,b,a"
 *   --stepper-border: "r,g,b,a"
 */

#ifndef FLEXUI_STEPPER_WIDGET_H
#define FLEXUI_STEPPER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>

namespace flexUI {

class StepperWidget : public Widget {
public:
    explicit StepperWidget(int value = 0, int min_value = 0, int max_value = 100, int step = 1);

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "StepperWidget"; }

    // Value access
    int value() const { return value_; }
    void set_value(int v);

    int min_value() const { return min_value_; }
    void set_min_value(int v) { min_value_ = v; dirty_ = true; }

    int max_value() const { return max_value_; }
    void set_max_value(int v) { max_value_ = v; dirty_ = true; }

    int step() const { return step_; }
    void set_step(int s) { step_ = s > 0 ? s : 1; }

    // Callback
    using ChangeCallback = std::function<void(int value)>;
    void set_change_callback(ChangeCallback cb) { callback_ = std::move(cb); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    int hit_test(float x, float y, const Element& elem);

    // Visual composition
    Group root_;
    RectShape* minus_bg_ = nullptr;
    TextShape* minus_text_ = nullptr;
    RectShape* value_bg_ = nullptr;
    TextShape* value_text_ = nullptr;
    RectShape* plus_bg_ = nullptr;
    TextShape* plus_text_ = nullptr;

    // State
    int value_;
    int min_value_;
    int max_value_;
    int step_;
    int hovered_button_ = -1;  // -1: none, 0: minus, 1: plus
    bool pressed_ = false;

    // Cached dimensions
    float cached_width_ = 0;
    float cached_height_ = 0;

    ChangeCallback callback_;
};

} // namespace flexUI

#endif
