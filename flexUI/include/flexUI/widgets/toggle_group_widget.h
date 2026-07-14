/*
 * flexUI - ToggleGroupWidget
 *
 * Toggle button group using Group/Shape composition system.
 */

#ifndef FLEXUI_TOGGLE_GROUP_WIDGET_H
#define FLEXUI_TOGGLE_GROUP_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>
#include <vector>
#include <string>

namespace flexUI {

/**
 * ToggleGroupWidget - Button group for single/multi selection
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (button 1 bg)
 *   ├── TextShape (button 1 label)
 *   ├── RectShape (button 2 bg)
 *   ├── TextShape (button 2 label)
 *   └── ...
 *
 * CSS variables:
 *   --toggle-bg: "r,g,b,a"
 *   --toggle-bg-active: "r,g,b,a"
 *   --toggle-text: "r,g,b,a"
 *   --toggle-text-active: "r,g,b,a"
 *   --toggle-border: "r,g,b,a"
 *   --toggle-gap: "0"
 *   --toggle-radius: "4"
 *   --multi-select: "false"
 */
class ToggleGroupWidget : public Widget {
public:
    struct Option {
        std::string id;
        std::string label;
    };

    explicit ToggleGroupWidget(const std::vector<Option>& options = {});

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "ToggleGroupWidget"; }

    // Options
    void set_options(const std::vector<Option>& options);
    const std::vector<Option>& options() const { return options_; }

    // Selection (single select mode)
    int selected_index() const { return selected_index_; }
    void set_selected_index(int index);
    std::string selected_id() const;

    // Selection (multi select mode)
    const std::vector<int>& selected_indices() const { return selected_indices_; }
    void set_selected_indices(const std::vector<int>& indices);
    bool is_selected(int index) const;

    // Multi-select mode
    bool multi_select() const { return multi_select_; }
    void set_multi_select(bool multi) { multi_select_ = multi; dirty_ = true; }

    // Callback
    using ChangeCallback = std::function<void(int index, const std::string& id)>;
    void set_change_callback(ChangeCallback callback) { change_callback_ = std::move(callback); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    int hit_test(float x, float y, const Element& elem);

    // Visual composition
    Group root_;
    std::vector<RectShape*> backgrounds_;
    std::vector<TextShape*> labels_;

    // State
    std::vector<Option> options_;
    int selected_index_ = 0;
    std::vector<int> selected_indices_;
    bool multi_select_ = false;
    int hovered_index_ = -1;

    // Cached
    float cached_width_ = 0;
    float cached_height_ = 0;

    ChangeCallback change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_TOGGLE_GROUP_WIDGET_H
