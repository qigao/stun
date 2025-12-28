/*
 * flexUI - PanelWidget
 *
 * A container widget that can be moved and resized.
 */

#ifndef FLEXUI_PANEL_WIDGET_H
#define FLEXUI_PANEL_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include <string>

namespace flexUI {

class PanelWidget : public Widget {
public:
    explicit PanelWidget(const std::string& title = "Panel")
        : title_(title) {}

    void render(const Element& elem, Renderer& renderer) override {
        // Render simple frame
        if (elem.computed_style->position == Position::Absolute) {
            renderer.draw_rect(0, 0, elem.width(), elem.height(), 4,
                              Paint::solid({0.15f, 0.15f, 0.15f, 0.95f}), 
                              Paint::solid({0.3f, 0.3f, 0.3f, 1.0f}), 1.0f);
        }

        // Render header
        float header_h = 28.0f;
        Color header_col = {0.25f, 0.25f, 0.25f, 1.0f};
        if (dragging_) header_col = {0.3f, 0.35f, 0.4f, 1.0f};

        renderer.draw_rect(0, 0, elem.width(), header_h, 4,
                          Paint::solid(header_col), Paint::none(), 0);
        
        renderer.draw_text(title_, 10, 18, "Arial", 13, true, {0.9f, 0.9f, 0.9f, 1.0f});
    }

    bool handle_event(const Event& event, Element& elem) override {
        float header_h = 28.0f;

        if (event.type == EventType::MouseDown) {
            float local_x = event.x - elem.absolute_x();
            float local_y = event.y - elem.absolute_y();

            if (local_y < header_h) {
                dragging_ = true;
                drag_offset_x_ = local_x;
                drag_offset_y_ = local_y;
                return true;
            }
        } else if (event.type == EventType::MouseMove) {
            if (dragging_) {
                elem.computed_style->position = Position::Absolute;
                elem.computed_style->left = event.x - drag_offset_x_;
                elem.computed_style->top = event.y - drag_offset_y_;
                elem.mark_layout_dirty();
                return true;
            }
        } else if (event.type == EventType::MouseUp) {
            dragging_ = false;
        }
        return false;
    }

    bool wants_mouse_capture() const override { return dragging_; }

    const char* type_name() const override { return "PanelWidget"; }

private:
    std::string title_;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_PANEL_WIDGET_H
