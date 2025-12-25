/*
 * tvgbox2 - SliderWidget
 *
 * Slider control using Group/Shape composition system.
 */

#ifndef TVGBOX2_SLIDER_WIDGET_H
#define TVGBOX2_SLIDER_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>

namespace tvgbox2 {

/**
 * SliderWidget - Slider using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (track background)
 *   ├── RectShape (track fill)
 *   ├── CircleShape (thumb)
 *   └── TextShape (value label, optional)
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
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "SliderWidget"; }

    // Value access
    float value() const { return value_; }
    void set_value(float value);

    float min() const { return min_; }
    void set_min(float min) { min_ = min; update_value_position(); dirty_ = true; }

    float max() const { return max_; }
    void set_max(float max) { max_ = max; update_value_position(); dirty_ = true; }

    float step() const { return step_; }
    void set_step(float step) { step_ = step; }

    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

    // Callback
    using ChangeCallback = std::function<void(float value)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    void update_value_from_x(float x, const Element& elem);
    void update_value_position();
    float snap_to_step(float value);

    // Visual composition
    Group root_;
    RectShape* track_ = nullptr;
    RectShape* fill_ = nullptr;
    CircleShape* thumb_ = nullptr;
    TextShape* value_label_ = nullptr;

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
    float cached_width_ = 0;
    float cached_height_ = 0;
    float cached_track_height_ = 0;
    float cached_thumb_size_ = 0;
    bool cached_show_value_ = false;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_SLIDER_WIDGET_H
