/*
 * flexUI - DatePickerWidget
 *
 * Input field with dropdown calendar for date selection.
 * 
 * CSS Variables:
 *   --datepicker-width: 200px
 *   --datepicker-height: 36px
 */

#ifndef FLEXUI_DATEPICKER_WIDGET_H
#define FLEXUI_DATEPICKER_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
#include "calendar_widget.h"
#include <string>
#include <functional>
#include <cstdio>

namespace flexUI {

class DatePickerWidget : public Widget {
public:
    DatePickerWidget(const Date& initial = {2024, 1, 1}, const std::string& format = "YYYY-MM-DD")
        : date_(initial), format_(format) {
        calendar_.set_selected_date(initial);
        calendar_.set_view_date(initial);
        calendar_.set_select_callback([this](const Date& d) {
            date_ = d;
            open_ = false;
            sync_host_semantics();
            if (on_change_) on_change_(d);
        });
    }

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;
        sync_host_semantics();

        float w = elem.width();
        float h = elem.height();

        // Input field background
        Color bg = open_ ? Color{0.98f, 0.98f, 1.0f, 1} : Color{1, 1, 1, 1};
        Color border = open_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.8f, 0.8f, 0.8f, 1};
        commands.draw_rect(0, 0, w, h, 4, Paint::solid(bg), Paint::solid(border), 1);

        // Date text
        std::string text = format_date(date_);
        draw_inline_text(commands, style, text, 10, h * 0.65f, 13.0f, false,
                         {0.1f, 0.1f, 0.1f, 1});

        // Calendar icon
        float icon_x = w - 28;
        draw_inline_text(commands, style, "📅", icon_x, h * 0.65f, 14.0f, false,
                         {0.4f, 0.4f, 0.4f, 1});
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        if (!open_) return;

        const auto anchor_bounds = detail::css_render_world_bounds(&elem);
        float x = anchor_bounds.x;
        float y = anchor_bounds.y + anchor_bounds.height + 4;

        // Create a temporary element for calendar rendering
        Element temp_elem;
        temp_elem.set_layout_bounds(0.0f, 0.0f, 280.0f, 280.0f);
        ComputedStyle overlay_style = overlay_child_style(elem.computed_style);
        temp_elem.computed_style = &overlay_style;

        commands.draw_rect(x + 2, y + 2, 280, 280, 6,
                                   Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);
        commands.draw_rect(x, y, 280, 280, 6, Paint::solid({1, 1, 1, 1}),
                                   Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);
        commands.save();
        commands.translate(x, y);
        commands.push_transform_prefix(flex::make_translation(x, y));

        calendar_.emit_render_commands(temp_elem, commands);

        commands.pop_transform_prefix();
        commands.restore();

        calendar_bounds_ = {x, y, 280, 280};
    }

    bool has_overlay() const override { return open_; }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Click on input field
            if (lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height()) {
                open_ = !open_;
                if (open_) {
                    calendar_.set_view_date(date_);
                }
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }

            // Click on calendar
            if (open_ && hit_calendar(event.x, event.y)) {
                // Forward to calendar
                Event cal_event = event;
                cal_event.x = event.x - calendar_bounds_.x;
                cal_event.y = event.y - calendar_bounds_.y;
                
                Element temp;
                temp.set_layout_bounds(0.0f, 0.0f, 280.0f, 280.0f);
                ComputedStyle overlay_style = overlay_child_style(elem.computed_style);
                temp.computed_style = &overlay_style;
                
                if (calendar_.handle_event(cal_event, temp)) {
                    elem.mark_paint_dirty();
                    return true;
                }
            }

            // Click outside closes
            if (open_) {
                open_ = false;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseMove && open_ && hit_calendar(event.x, event.y)) {
            Event cal_event = event;
            cal_event.x = event.x - calendar_bounds_.x;
            cal_event.y = event.y - calendar_bounds_.y;
            
            Element temp;
            temp.set_layout_bounds(0.0f, 0.0f, 280.0f, 280.0f);
            ComputedStyle overlay_style = overlay_child_style(elem.computed_style);
            temp.computed_style = &overlay_style;
            
            calendar_.handle_event(cal_event, temp);
            elem.mark_paint_dirty();
            return true;
        }

        return false;
    }

    bool wants_mouse_capture() const override { return open_; }

    // API
    Date date() const { return date_; }
    void set_date(const Date& d) {
        date_ = d;
        calendar_.set_selected_date(d);
        calendar_.set_view_date(d);
        sync_host_semantics();
    }
    void set_open(bool open) {
        open_ = open;
        if (open_) {
            calendar_.set_view_date(date_);
        }
        sync_host_semantics();
        dirty_ = true;
    }
    bool is_open() const { return open_; }

    using ChangeCallback = std::function<void(const Date&)>;
    void on_change(ChangeCallback cb) { on_change_ = std::move(cb); }

    const char* type_name() const override { return "DatePickerWidget"; }

private:
    struct Rect { float x, y, w, h; };

    static ComputedStyle make_text_style(const ComputedStyle* base_style, float font_size,
                                         bool bold) {
        ComputedStyle style;
        if (base_style) {
            style = *base_style;
        }
        style.font_size = font_size;
        style.font_weight = bold ? FontWeight::Bold : FontWeight::Normal;
        return style;
    }

    static ComputedStyle overlay_child_style(const ComputedStyle* base_style) {
        ComputedStyle style;
        if (base_style) {
            style = *base_style;
        }
        style.transform_x = 0.0f;
        style.transform_y = 0.0f;
        style.transform_scale = 1.0f;
        style.transform_scale_x = 1.0f;
        style.transform_scale_y = 1.0f;
        style.transform_rotate = 0.0f;
        style.has_transform_matrix = false;
        style.transform_matrix = flex::Transform{};
        return style;
    }

    static float draw_inline_text(RenderCommandList& commands, const ComputedStyle* base_style,
                                  const std::string& text, float x, float baseline_y,
                                  float font_size, bool bold, const Color& color) {
        const auto text_style = make_text_style(base_style, font_size, bold);
        return emit_segmented_text_line(commands, &text_style, text, x,
                                                       baseline_y, color, bold);
    }

    bool hit_calendar(float px, float py) const {
        return px >= calendar_bounds_.x && px < calendar_bounds_.x + calendar_bounds_.w &&
               py >= calendar_bounds_.y && py < calendar_bounds_.y + calendar_bounds_.h;
    }

    std::string format_date(const Date& d) const {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day);
        return buf;
    }

    void sync_host_semantics() override {
        set_host_attribute("role", "combobox");
        set_host_attribute("aria-haspopup", "dialog");
        set_host_boolean_attribute("aria-expanded", open_);
        set_host_boolean_attribute("aria-hidden", false);
        set_host_attribute("data-state", open_ ? "open" : "closed");
        set_host_attribute("data-value", format_date(date_));
        set_host_attribute("aria-label", format_date(date_));
    }

    Date date_;
    std::string format_;
    bool open_ = false;
    CalendarWidget calendar_;
    Rect calendar_bounds_{};
    ChangeCallback on_change_;
};

} // namespace flexUI

#endif
