/*
 * flexUI - SliderWidget
 *
 * Slider control exposing stable widget-owned Element parts.
 */

#ifndef FLEXUI_SLIDER_WIDGET_H
#define FLEXUI_SLIDER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>

namespace flexUI {

/**
 * SliderWidget - Slider with a CSS-visible semantic subtree
 *
 * Structure:
 *   slider (host)
 *   |-- track
 *   |-- fill
 *   |-- thumb
 *   `-- value
 *
 * CSS variables:
 *   --track-height: "4"
 *   --track-bg: "r,g,b,a"
 *   --track-fill: "r,g,b,a"
 *   --thumb-size: "20"
 *   --thumb-bg: "r,g,b,a"
 *   --thumb-border: "r,g,b,a"
 *   --transition-duration: "100"
 *   --show-value: "true" | "false"
 */
class SliderWidget : public Widget {
public:
    explicit SliderWidget(float min = 0.0f, float max = 100.0f,
                          float value = 50.0f, float step = 0.0f);

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "SliderWidget"; }
    bool wants_mouse_capture() const override { return is_dragging_; }
    void sync_host_semantics_for_layout(Element& elem) override;

    Element* track_element() { return track_; }
    Element* fill_element() { return fill_; }
    Element* thumb_element() { return thumb_; }
    Element* value_element() { return value_label_; }
    const Element* track_element() const { return track_; }
    const Element* fill_element() const { return fill_; }
    const Element* thumb_element() const { return thumb_; }
    const Element* value_element() const { return value_label_; }

    // Value access
    float value() const { return value_; }
    void set_value(float value);

    float min() const { return min_; }
    void set_min(float min);

    float max() const { return max_; }
    void set_max(float max);

    float step() const { return step_; }
    void set_step(float step) { step_ = step; }

    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled);

    // Callback
    using ChangeCallback = std::function<void(float value)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void build_semantic_tree() override;
    void update_part_geometry(const Element& elem);
    void sync_host_semantics() override;

    void update_value_from_position(float x, float y, const Element& elem);
    void update_value_position();
    void invalidate_parts();
    float snap_to_step(float value);

    Element* track_ = nullptr;
    Element* fill_ = nullptr;
    Element* thumb_ = nullptr;
    Element* value_label_ = nullptr;

    // State
    float min_ = 0.0f;
    float max_ = 100.0f;
    float value_ = 50.0f;
    float step_ = 0.0f;

    bool disabled_ = false;
    bool is_dragging_ = false;

    // Animation
    float value_position_ = 0.5f;
    float current_thumb_scale_ = 1.0f;
    float target_thumb_scale_ = 1.0f;

    // Cached dimensions
    // Callback
    ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_SLIDER_WIDGET_H
