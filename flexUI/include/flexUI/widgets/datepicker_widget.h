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
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
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
            if (on_change_) on_change_(d);
        });
    }

    void render(const Element& elem, Renderer& renderer) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();

        // Input field background
        Color bg = open_ ? Color{0.98f, 0.98f, 1.0f, 1} : Color{1, 1, 1, 1};
        Color border = open_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.8f, 0.8f, 0.8f, 1};
        renderer.draw_rect(0, 0, w, h, 4, Paint::solid(bg), Paint::solid(border), 1);

        // Date text
        std::string text = format_date(date_);
        renderer.draw_text(text, 10, h * 0.65f, style->font_family, 13, false, {0.1f, 0.1f, 0.1f, 1});

        // Calendar icon
        float icon_x = w - 28;
        renderer.draw_text("📅", icon_x, h * 0.65f, style->font_family, 14, false, {0.4f, 0.4f, 0.4f, 1});
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!open_) return;

        float x = elem.absolute_x();
        float y = elem.absolute_y() + elem.height() + 4;

        // Create a temporary element for calendar rendering
        Element temp_elem;
        temp_elem.set_layout_bounds(x, y, 280, 280);
        temp_elem.computed_style = elem.computed_style;

        // Shadow
        renderer.draw_rect(x + 2, y + 2, 280, 280, 6, Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);

        // Background
        renderer.draw_rect(x, y, 280, 280, 6, Paint::solid({1, 1, 1, 1}), Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        // Render calendar inside
        renderer.flex().save();
        renderer.flex().translate(x, y);
        calendar_.render(temp_elem, renderer);
        renderer.flex().restore();

        calendar_bounds_ = {x, y, 280, 280};
    }

    bool has_overlay() const override { return open_; }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Click on input field
            if (lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height()) {
                open_ = !open_;
                if (open_) {
                    calendar_.set_view_date(date_);
                }
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
                temp.set_layout_bounds(0, 0, 280, 280);
                temp.computed_style = elem.computed_style;
                
                if (calendar_.handle_event(cal_event, temp)) {
                    elem.mark_paint_dirty();
                    return true;
                }
            }

            // Click outside closes
            if (open_) {
                open_ = false;
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseMove && open_ && hit_calendar(event.x, event.y)) {
            Event cal_event = event;
            cal_event.x = event.x - calendar_bounds_.x;
            cal_event.y = event.y - calendar_bounds_.y;
            
            Element temp;
            temp.set_layout_bounds(0, 0, 280, 280);
            temp.computed_style = elem.computed_style;
            
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
    }

    using ChangeCallback = std::function<void(const Date&)>;
    void on_change(ChangeCallback cb) { on_change_ = std::move(cb); }

    const char* type_name() const override { return "DatePickerWidget"; }

private:
    struct Rect { float x, y, w, h; };

    bool hit_calendar(float px, float py) const {
        return px >= calendar_bounds_.x && px < calendar_bounds_.x + calendar_bounds_.w &&
               py >= calendar_bounds_.y && py < calendar_bounds_.y + calendar_bounds_.h;
    }

    std::string format_date(const Date& d) const {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day);
        return buf;
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
