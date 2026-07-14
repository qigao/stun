/*
 * flexUI - Draggable Widget
 *
 * Makes an element draggable by mouse.
 * Click-through: button clicks work normally, dragging starts after movement.
 */

#ifndef FLEXUI_DRAGGABLE_WIDGET_H
#define FLEXUI_DRAGGABLE_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../event.h"
#include "../element.h"
#include "../computed_style.h"
#include <cmath>

namespace flexUI {

class DraggableWidget : public Widget {
public:
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        // Transient drag effects belong in style/render capability handling, not here.
    }

    bool handle_event(const Event& event, Element& elem) override {
        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            potential_drag_ = true;
            drag_start_x_ = event.x;
            drag_start_y_ = event.y;

            const auto parent_pos =
                detail::css_render_to_parent_content(&elem, flex::Vec2(event.x, event.y));
            offset_x_ = parent_pos.x - elem.x();
            offset_y_ = parent_pos.y - elem.y();

            return false;
        }

        if (event.type == EventType::MouseMove && potential_drag_) {
            float dx = event.x - drag_start_x_;
            float dy = event.y - drag_start_y_;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (!dragging_ && dist > drag_threshold_) {
                dragging_ = true;
                elem.add_state("dragging");
                
                if (elem.computed_style) {
                    elem.computed_style->position = Position::Absolute;
                }
            }

            if (dragging_ && elem.computed_style) {
                const auto parent_pos =
                    detail::css_render_to_parent_content(&elem, flex::Vec2(event.x, event.y));
                elem.computed_style->left = parent_pos.x - offset_x_;
                elem.computed_style->top = parent_pos.y - offset_y_;
                elem.mark_layout_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseUp) {
            if (dragging_) {
                elem.remove_state("dragging");
            }
            potential_drag_ = false;
            dragging_ = false;
            return false;
        }

        return false;
    }

    bool has_overlay() const override { return dragging_; }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        // Overlay check ensures it's on top, 
        // but render() is already providing the "lifted" look.
    }

    bool wants_mouse_capture() const override {
        return dragging_;
    }

    const char* type_name() const override { return "DraggableWidget"; }

private:
    bool potential_drag_ = false;
    bool dragging_ = false;
    float drag_start_x_ = 0;
    float drag_start_y_ = 0;
    float offset_x_ = 0;
    float offset_y_ = 0;
    float drag_threshold_ = 3.0f;
};

} // namespace flexUI

#endif // FLEXUI_DRAGGABLE_WIDGET_H
