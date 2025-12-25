/*
 * tvgbox2 - CheckboxWidget
 *
 * Checkbox control using Group/Shape composition system.
 */

#ifndef TVGBOX2_CHECKBOX_WIDGET_H
#define TVGBOX2_CHECKBOX_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * CheckboxWidget - Checkbox using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (box background + border)
 *   ├── PathShape (checkmark)
 *   └── TextShape (label)
 *
 * CSS variables:
 *   --checkbox-size: "20"
 *   --checkbox-bg: "r,g,b,a"
 *   --checkbox-bg-checked: "r,g,b,a"
 *   --checkbox-border: "r,g,b,a"
 *   --checkbox-checkmark: "r,g,b,a"
 *   --transition-duration: "200"
 *   --label-spacing: "8"
 */
class CheckboxWidget : public Widget {
public:
    explicit CheckboxWidget(const std::string& label = "", bool checked = false);

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "CheckboxWidget"; }

    // State
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
    void rebuild_shapes(float checkbox_size, float label_spacing, const std::string& font_family, float font_size);
    void update_colors(const Element& elem);
    void update_checkmark_path(float checkbox_size);

    // Visual composition
    Group root_;
    RectShape* box_ = nullptr;
    PathShape* checkmark_ = nullptr;
    TextShape* text_ = nullptr;

    // State
    bool checked_ = false;
    std::string label_;
    bool disabled_ = false;

    // Animation
    float checkmark_scale_ = 0.0f;
    float target_checkmark_scale_ = 0.0f;

    // Cached dimensions
    float cached_checkbox_size_ = 0;
    float cached_label_spacing_ = 0;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_CHECKBOX_WIDGET_H
