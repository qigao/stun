/*
 * flexUI - ProgressBarWidget
 *
 * Progress bar exposing stable widget-owned Element parts.
 */

#ifndef FLEXUI_PROGRESSBAR_WIDGET_H
#define FLEXUI_PROGRESSBAR_WIDGET_H

#include "../widget.h"
#include "../render_command.h"

namespace flexUI {

/**
 * ProgressBarWidget - Progress bar with a CSS-visible semantic subtree
 *
 * Structure:
 *   progressbar (host)
 *   |-- track
 *   `-- fill
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
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override;
    const char* type_name() const override { return "ProgressBarWidget"; }
    bool paints_host_box() const override { return true; }
    void sync_host_semantics_for_layout(Element& elem) override;

    Element* track_element() { return track_; }
    Element* fill_element() { return fill_; }
    const Element* track_element() const { return track_; }
    const Element* fill_element() const { return fill_; }

    // Value access
    float value() const { return value_; }
    void set_value(float value);

    bool is_indeterminate() const { return indeterminate_; }
    void set_indeterminate(bool indeterminate);

private:
    void build_semantic_tree() override;
    void update_part_geometry(const Element& elem);
    void sync_host_semantics() override;
    void invalidate_render_cache();

    Element* track_ = nullptr;
    Element* fill_ = nullptr;

    // State
    float value_ = 0.0f;  // 0-100
    bool indeterminate_ = false;

    // Indeterminate animation
    float animation_time_ = 0.0f;
    float animation_position_ = 0.0f;

};

} // namespace flexUI

#endif // FLEXUI_PROGRESSBAR_WIDGET_H
