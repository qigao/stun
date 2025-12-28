/*
 * flexUI - Draggable Widget
 *
 * Makes an element draggable by mouse.
 * Click-through: button clicks work normally, dragging starts after movement.
 */

#ifndef FLEXUI_DRAGGABLE_WIDGET_H
#define FLEXUI_DRAGGABLE_WIDGET_H

#include "../widget.h"
#include "../event.h"
#include "../element.h"
#include "../computed_style.h"
#include <cmath>

namespace flexUI {

class DraggableWidget : public Widget {
public:
    void render(const Element& elem, Renderer& renderer) override {
        // No custom rendering - just makes the element draggable
    }

    bool handle_event(const Event& event, Element& elem) override {
        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Start tracking potential drag, but don't consume yet
            potential_drag_ = true;
            drag_start_x_ = event.x;
            drag_start_y_ = event.y;
            // Handle NAN (unset) values - use current element position
            if (elem.computed_style) {
                elem_start_x_ = std::isnan(elem.computed_style->left) ? elem.x() : elem.computed_style->left;
                elem_start_y_ = std::isnan(elem.computed_style->top) ? elem.y() : elem.computed_style->top;
            } else {
                elem_start_x_ = elem.x();
                elem_start_y_ = elem.y();
            }
            return false;  // Don't consume - let buttons handle clicks
        }

        if (event.type == EventType::MouseMove && potential_drag_) {
            float dx = event.x - drag_start_x_;
            float dy = event.y - drag_start_y_;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Start actual drag after threshold movement
            if (!dragging_ && dist > drag_threshold_) {
                dragging_ = true;
            }

            if (dragging_ && elem.computed_style) {
                elem.computed_style->left = elem_start_x_ + dx;
                elem.computed_style->top = elem_start_y_ + dy;
                elem.mark_layout_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseUp) {
            potential_drag_ = false;
            dragging_ = false;
            return false;
        }

        return false;
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
    float elem_start_x_ = 0;
    float elem_start_y_ = 0;
    float drag_threshold_ = 5.0f;  // Pixels before drag starts
};

} // namespace flexUI

#endif // FLEXUI_DRAGGABLE_WIDGET_H
