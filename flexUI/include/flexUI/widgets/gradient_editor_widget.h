/*
 * flexUI - GradientEditorWidget
 *
 * Gradient editor for LinearGradient and RadialGradient
 */

#ifndef FLEXUI_GRADIENT_EDITOR_WIDGET_H
#define FLEXUI_GRADIENT_EDITOR_WIDGET_H

#include "../widget.h"
#include "../types.h"
#include <flex.h>
#include <functional>
#include <vector>

namespace flexUI {

class GradientEditorWidget : public Widget {
public:
    enum class GradientType { Linear, Radial };

    GradientEditorWidget();

    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "GradientEditorWidget"; }
    bool wants_mouse_capture() const override { return dragging_stop_ >= 0; }

    // Gradient type
    GradientType type() const { return type_; }
    void set_type(GradientType t);

    // Get/set gradient
    flex::LinearGradient linear_gradient() const { return linear_; }
    flex::RadialGradient radial_gradient() const { return radial_; }
    void set_gradient(const flex::LinearGradient& g);
    void set_gradient(const flex::RadialGradient& g);

    // Color stops
    int stop_count() const;
    flex::ColorStop get_stop(int index) const;
    void set_stop_color(int index, const Color& color);
    void add_stop(float offset, const Color& color);
    void remove_stop(int index);

    // Selected stop
    int selected_stop() const { return selected_stop_; }
    void set_selected_stop(int index);

    // Callbacks
    using ChangeCallback = std::function<void()>;
    using StopSelectCallback = std::function<void(int index, const Color& color)>;

    void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }
    void set_stop_select_callback(StopSelectCallback cb) { on_stop_select_ = std::move(cb); }

private:
    void render_gradient_bar(flex::Renderer& r, const Element& elem);
    void render_stops(flex::Renderer& r, const Element& elem);

    float stop_to_x(float offset, float bar_x, float bar_w) const;
    float x_to_offset(float x, float bar_x, float bar_w) const;
    int hit_test_stop(float x, float y, const Element& elem) const;

    void notify_change();

    GradientType type_ = GradientType::Linear;
    flex::LinearGradient linear_;
    flex::RadialGradient radial_;

    int selected_stop_ = 0;
    int dragging_stop_ = -1;
    float drag_start_offset_ = 0;
    bool dirty_ = false;

    ChangeCallback on_change_;
    StopSelectCallback on_stop_select_;
};

} // namespace flexUI

#endif // FLEXUI_GRADIENT_EDITOR_WIDGET_H
