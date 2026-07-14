/*
 * flexUI - CheckboxWidget
 *
 * Checkbox control exposing stable widget-owned Element parts.
 */

#ifndef FLEXUI_CHECKBOX_WIDGET_H
#define FLEXUI_CHECKBOX_WIDGET_H

#include "../widget.h"
#include <functional>
#include <string>

namespace flexUI {

class RenderCommandList;

/**
 * CheckboxWidget - Checkbox with a CSS-visible semantic subtree
 *
 * Structure:
 *   checkbox (host)
 *   |-- box
 *   |-- indicator
 *   `-- label
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
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override;
    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override;
    const char* type_name() const override { return "CheckboxWidget"; }
    bool paints_host_box() const override { return true; }
    bool paints_part_box(std::string_view part_name) const override;
    bool emit_part_render_commands(const Element& host, const Element& part,
                                   std::string_view part_name,
                                   RenderCommandList& commands) override;
    void sync_host_semantics_for_layout(Element& elem) override;

    Element* box_element() { return box_; }
    Element* indicator_element() { return indicator_; }
    Element* label_element() { return label_element_; }
    const Element* box_element() const { return box_; }
    const Element* indicator_element() const { return indicator_; }
    const Element* label_element() const { return label_element_; }

    // State
    bool is_checked() const { return checked_; }
    void set_checked(bool checked);

    const std::string& label() const { return label_; }
    void set_label(const std::string& label);

    bool is_disabled() const { return disabled_; }
    void set_disabled(bool disabled);

    // Callback
    using ChangeCallback = std::function<void(bool checked)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void build_semantic_tree() override;
    void sync_host_semantics() override;
    void update_part_geometry(const Element& elem);
    void invalidate_render_cache();

    Element* box_ = nullptr;
    Element* indicator_ = nullptr;
    Element* label_element_ = nullptr;

    // State
    bool checked_ = false;
    std::string label_;
    bool disabled_ = false;

    // Animation
    float checkmark_scale_ = 0.0f;
    float target_checkmark_scale_ = 0.0f;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_CHECKBOX_WIDGET_H
