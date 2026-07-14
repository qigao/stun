/*
 * flexUI - SwitchWidget
 *
 * Toggle switch exposing stable widget-owned Element parts.
 */

#ifndef FLEXUI_SWITCH_WIDGET_H
#define FLEXUI_SWITCH_WIDGET_H

#include "../widget.h"
#include <functional>
#include <string>

namespace flexUI {

/**
 * SwitchWidget - Toggle switch with a CSS-visible semantic subtree
 *
 * Structure:
 *   switch (host)
 *   |-- track
 *   |-- thumb
 *   `-- label
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
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override;
    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override;
    const char* type_name() const override { return "SwitchWidget"; }
    bool paints_host_box() const override { return true; }
    void sync_host_semantics_for_layout(Element& elem) override;

    Element* track_element() { return track_; }
    Element* thumb_element() { return thumb_; }
    Element* label_element() { return label_element_; }
    const Element* track_element() const { return track_; }
    const Element* thumb_element() const { return thumb_; }
    const Element* label_element() const { return label_element_; }

    // State access
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

    Element* track_ = nullptr;
    Element* thumb_ = nullptr;
    Element* label_element_ = nullptr;

    // State
    bool checked_ = false;
    std::string label_;
    bool disabled_ = false;

    // Animation
    float thumb_position_ = 0.0f;
    float target_thumb_position_ = 0.0f;

    // Callback
    ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_SWITCH_WIDGET_H
