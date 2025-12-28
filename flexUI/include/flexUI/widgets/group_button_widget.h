/*
 * flexUI - GroupButtonWidget
 *
 * Example widget using the new Group/Shape composition system.
 * Demonstrates how widgets can be built from basic shapes.
 */

#ifndef FLEXUI_GROUP_BUTTON_WIDGET_H
#define FLEXUI_GROUP_BUTTON_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>
#include <string>

namespace flexUI {

/**
 * GroupButtonWidget - Button built using Group/Shape composition
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape (background)
 *   └── TextShape (label)
 *
 * All internal positions are relative to the button's origin.
 * Moving the button moves all children automatically.
 */
class GroupButtonWidget : public Widget {
public:
    GroupButtonWidget();

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    const char* type_name() const override { return "GroupButtonWidget"; }

    // Button API
    const std::string& label() const { return label_; }
    void set_label(const std::string& label);

    using ClickCallback = std::function<void()>;
    void set_click_callback(ClickCallback cb) { on_click_ = std::move(cb); }

    // State
    bool is_hovered() const { return hovered_; }
    bool is_pressed() const { return pressed_; }

private:
    void rebuild_shapes(float width, float height);
    void update_colors();

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    TextShape* text_ = nullptr;

    // State
    std::string label_ = "Button";
    bool hovered_ = false;
    bool pressed_ = false;
    ClickCallback on_click_;

    // Cached dimensions
    float cached_width_ = 0;
    float cached_height_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_GROUP_BUTTON_WIDGET_H
