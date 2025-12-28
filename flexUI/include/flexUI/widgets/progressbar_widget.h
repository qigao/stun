/*
 * flexUI - ProgressBarWidget
 *
 * Progress bar using Group/Shape composition system.
 */

#ifndef FLEXUI_PROGRESSBAR_WIDGET_H
#define FLEXUI_PROGRESSBAR_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"

namespace flexUI {

/**
 * ProgressBarWidget - Progress bar using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (background)
 *   └── RectShape (fill)
 *
 * CSS variables:
 *   --progress-height: "8"
 *   --progress-bg: "r,g,b,a"
 *   --progress-fill: "r,g,b,a"
 *   --progress-border-radius: "4"
 *   --indeterminate-animation-duration: "1500"
 */
class ProgressBarWidget : public Widget {
public:
    explicit ProgressBarWidget(float value = 0.0f, bool indeterminate = false);

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "ProgressBarWidget"; }

    // Value access
    float value() const { return value_; }
    void set_value(float value);

    bool is_indeterminate() const { return indeterminate_; }
    void set_indeterminate(bool indeterminate);

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    RectShape* fill_ = nullptr;

    // State
    float value_ = 0.0f;  // 0-100
    bool indeterminate_ = false;

    // Indeterminate animation
    float animation_time_ = 0.0f;
    float animation_position_ = 0.0f;

    // Cached dimensions
    float cached_width_ = 0;
    float cached_height_ = 0;
    float cached_bar_height_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_PROGRESSBAR_WIDGET_H
