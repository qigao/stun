/*
 * flexUI - PanelWidget
 *
 * A container widget that can be moved and resized.
 */

#ifndef FLEXUI_PANEL_WIDGET_H
#define FLEXUI_PANEL_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../render_command.h"
#include "../text_layout.h"
#include <string>

namespace flexUI {

class PanelWidget : public Widget {
public:
    explicit PanelWidget(const std::string& title = "Panel")
        : title_(title) {}

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        // Render simple frame
        if (elem.computed_style->position == Position::Absolute) {
            commands.draw_rect(0, 0, elem.width(), elem.height(), 4,
                               Paint::solid({0.15f, 0.15f, 0.15f, 0.95f}), 
                               Paint::solid({0.3f, 0.3f, 0.3f, 1.0f}), 1.0f);
        }

        // Render header
        float header_h = 28.0f;
        Color header_col = {0.25f, 0.25f, 0.25f, 1.0f};
        if (dragging_) header_col = {0.3f, 0.35f, 0.4f, 1.0f};

        commands.draw_rect(0, 0, elem.width(), header_h, 4,
                           Paint::solid(header_col), Paint::none(), 0);
        
        draw_inline_text(commands, title_, 10, 18, 13.0f, true, {0.9f, 0.9f, 0.9f, 1.0f});
    }

    bool handle_event(const Event& event, Element& elem) override {
        float header_h = 28.0f;

        if (event.type == EventType::MouseDown) {
            const flex::Vec2 local_pos =
                detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
            float local_y = local_pos.y;

            if (local_y < header_h) {
                dragging_ = true;
                const auto parent_pos =
                    detail::css_render_to_parent_content(&elem, flex::Vec2(event.x, event.y));
                drag_offset_x_ = parent_pos.x - elem.x();
                drag_offset_y_ = parent_pos.y - elem.y();
                return true;
            }
        } else if (event.type == EventType::MouseMove) {
            if (dragging_) {
                elem.computed_style->position = Position::Absolute;
                const auto parent_pos =
                    detail::css_render_to_parent_content(&elem, flex::Vec2(event.x, event.y));
                elem.computed_style->left = parent_pos.x - drag_offset_x_;
                elem.computed_style->top = parent_pos.y - drag_offset_y_;
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
    static float draw_inline_text(RenderCommandList& commands, const std::string& text, float x,
                                  float baseline_y, float font_size, bool bold,
                                  const Color& color) {
        ComputedStyle style;
        style.font_family = "Arial";
        style.font_size = font_size;
        style.font_weight = bold ? FontWeight::Bold : FontWeight::Normal;
        return emit_segmented_text_line(commands, &style, text, x,
                                                       baseline_y, color, bold);
    }

    std::string title_;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_PANEL_WIDGET_H
