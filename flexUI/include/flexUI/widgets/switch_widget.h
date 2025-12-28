/*
 * flexUI - SwitchWidget
 *
 * Toggle switch using Group/Shape composition system.
 */

#ifndef FLEXUI_SWITCH_WIDGET_H
#define FLEXUI_SWITCH_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace flexUI {

/**
 * SwitchWidget - Toggle switch using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (track background)
 *   ├── CircleShape (thumb)
 *   └── TextShape (label, optional)
 *
 * CSS variables:
 *   --switch-width: "50"
 *   --switch-height: "28"
 *   --switch-bg-off: "r,g,b,a"
 *   --switch-bg-on: "r,g,b,a"
 *   --switch-thumb: "r,g,b,a"
 *   --transition-duration: "200"
 *   --label-spacing: "8"
 */
class SwitchWidget : public Widget {
public:
    explicit SwitchWidget(const std::string& label = "", bool checked = false);

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "SwitchWidget"; }

    // State access
    bool is_checked() const { return checked_; }
    void set_checked(bool checked);

    const std::string& label() const { return label_; }
    void set_label(const std::string& label);

    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

    // Callback
    using ChangeCallback = std::function<void(bool checked)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    // Visual composition
    Group root_;
    RectShape* track_ = nullptr;
    CircleShape* thumb_ = nullptr;
    TextShape* text_ = nullptr;

    // State
    bool checked_ = false;
    std::string label_;
    bool disabled_ = false;

    // Animation
    float thumb_position_ = 0.0f;
    float target_thumb_position_ = 0.0f;

    // Cached dimensions
    float cached_switch_width_ = 0;
    float cached_switch_height_ = 0;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_SWITCH_WIDGET_H
