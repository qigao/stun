/*
 * tvgbox2 - RadioWidget
 *
 * Radio button using Group/Shape composition system.
 */

#ifndef TVGBOX2_RADIO_WIDGET_H
#define TVGBOX2_RADIO_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * RadioWidget - Radio button using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── CircleShape (outer circle background)
 *   ├── CircleShape (dot, animated)
 *   └── TextShape (label)
 *
 * CSS variables:
 *   --radio-size: "20"
 *   --radio-bg: "r,g,b,a"
 *   --radio-bg-checked: "r,g,b,a"
 *   --radio-border: "r,g,b,a"
 *   --radio-dot: "r,g,b,a"
 *   --transition-duration: "200"
 *   --label-spacing: "8"
 */
class RadioWidget : public Widget {
public:
    explicit RadioWidget(const std::string& label = "",
                         const std::string& value = "",
                         const std::string& group = "",
                         bool checked = false);

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "RadioWidget"; }

    // State access
    bool is_checked() const { return checked_; }
    void set_checked(bool checked);

    const std::string& label() const { return label_; }
    void set_label(const std::string& label);

    const std::string& value() const { return value_; }
    void set_value(const std::string& value) { value_ = value; }

    const std::string& group() const { return group_; }
    void set_group(const std::string& group) { group_ = group; }

    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

    // Callback
    using ChangeCallback = std::function<void(const std::string& value, const std::string& group)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    // Visual composition
    Group root_;
    CircleShape* circle_ = nullptr;
    CircleShape* dot_ = nullptr;
    TextShape* text_ = nullptr;

    // State
    bool checked_ = false;
    std::string label_;
    std::string value_;
    std::string group_;
    bool disabled_ = false;

    // Animation
    float dot_scale_ = 0.0f;
    float target_dot_scale_ = 0.0f;

    // Cached dimensions
    float cached_radio_size_ = 0;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_RADIO_WIDGET_H
