/*
 * flexUI - TimePickerWidget
 *
 * Time selection with hour/minute/second spinners.
 */

#ifndef FLEXUI_TIMEPICKER_WIDGET_H
#define FLEXUI_TIMEPICKER_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <functional>
#include <cstdio>

namespace flexUI {

struct Time {
    int hour = 0;    // 0-23
    int minute = 0;  // 0-59
    int second = 0;  // 0-59
    
    bool operator==(const Time& o) const { return hour == o.hour && minute == o.minute && second == o.second; }
};

class TimePickerWidget : public Widget {
public:
    TimePickerWidget(const Time& initial = {12, 0, 0}, bool show_seconds = false)
        : time_(initial), show_seconds_(show_seconds) {}

    void render(const Element& elem, Renderer& renderer) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();

        // Background
        Color bg = focused_ ? Color{1, 1, 1, 1} : Color{0.98f, 0.98f, 0.98f, 1};
        Color border = focused_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.85f, 0.85f, 0.85f, 1};
        renderer.draw_rect(0, 0, w, h, 4, Paint::solid(bg), Paint::solid(border), 1);

        // Time display
        char buf[16];
        if (show_seconds_) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", time_.hour, time_.minute, time_.second);
        } else {
            std::snprintf(buf, sizeof(buf), "%02d:%02d", time_.hour, time_.minute);
        }
        renderer.draw_text(buf, 12, h * 0.65f, style->font_family, 14, false, {0.1f, 0.1f, 0.1f, 1});

        // Clock icon
        renderer.draw_text("🕐", w - 28, h * 0.65f, style->font_family, 14, false, {0.4f, 0.4f, 0.4f, 1});
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!open_) return;

        auto* style = elem.computed_style;
        float x = elem.absolute_x();
        float y = elem.absolute_y() + elem.height() + 4;
        float col_w = 60;
        float w = show_seconds_ ? col_w * 3 + 20 : col_w * 2 + 16;
        float h = 180;

        // Shadow + background
        renderer.draw_rect(x + 2, y + 2, w, h, 6, Paint::solid({0, 0, 0, 0.12f}), Paint::none(), 0);
        renderer.draw_rect(x, y, w, h, 6, Paint::solid({1, 1, 1, 1}), Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        // Column headers
        Color header_c{0.5f, 0.5f, 0.5f, 1};
        renderer.draw_text("Hour", x + 8, y + 20, style->font_family, 11, false, header_c);
        renderer.draw_text("Min", x + 8 + col_w, y + 20, style->font_family, 11, false, header_c);
        if (show_seconds_) {
            renderer.draw_text("Sec", x + 8 + col_w * 2, y + 20, style->font_family, 11, false, header_c);
        }

        // Spinners
        float spinner_y = y + 35;
        render_spinner(x + 8, spinner_y, col_w - 8, time_.hour, 0, 23, 0, style, renderer);
        render_spinner(x + 8 + col_w, spinner_y, col_w - 8, time_.minute, 0, 59, 1, style, renderer);
        if (show_seconds_) {
            render_spinner(x + 8 + col_w * 2, spinner_y, col_w - 8, time_.second, 0, 59, 2, style, renderer);
        }

        // OK button
        float btn_y = y + h - 40;
        Color btn_bg = hover_ok_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.3f, 0.4f, 0.8f, 1};
        renderer.draw_rect(x + w/2 - 30, btn_y, 60, 28, 4, Paint::solid(btn_bg), Paint::none(), 0);
        renderer.draw_text("OK", x + w/2 - 8, btn_y + 19, style->font_family, 13, true, {1, 1, 1, 1});

        popup_bounds_ = {x, y, w, h};
        ok_bounds_ = {x + w/2 - 30, btn_y, 60, 28};
        spinner_y_ = spinner_y;
        col_width_ = col_w;
    }

    bool has_overlay() const override { return open_; }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Click input
            if (lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height()) {
                open_ = !open_;
                focused_ = open_;
                elem.mark_paint_dirty();
                return true;
            }

            // Click popup
            if (open_ && hit(event.x, event.y, popup_bounds_)) {
                // OK button
                if (hit(event.x, event.y, ok_bounds_)) {
                    open_ = false;
                    if (on_change_) on_change_(time_);
                    elem.mark_paint_dirty();
                    return true;
                }

                // Spinner clicks
                handle_spinner_click(event.x, event.y, elem);
                return true;
            }

            // Outside
            if (open_) {
                open_ = false;
                focused_ = false;
                elem.mark_paint_dirty();
            }
        }

        if (event.type == EventType::MouseMove && open_) {
            hover_ok_ = hit(event.x, event.y, ok_bounds_);
            elem.mark_paint_dirty();
        }

        return open_;
    }

    bool wants_mouse_capture() const override { return open_; }

    Time time() const { return time_; }
    void set_time(const Time& t) { time_ = t; }

    using ChangeCallback = std::function<void(const Time&)>;
    void on_change(ChangeCallback cb) { on_change_ = std::move(cb); }

    const char* type_name() const override { return "TimePickerWidget"; }

private:
    struct Rect { float x, y, w, h; };

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    void render_spinner(float x, float y, float w, int value, int min_v, int max_v, int idx,
                        const ComputedStyle* style, Renderer& renderer) {
        float h = 100;
        
        // Up arrow
        Color arrow_c = (hover_spinner_ == idx && hover_up_) ? Color{0.3f, 0.4f, 0.8f, 1} : Color{0.4f, 0.4f, 0.4f, 1};
        renderer.draw_text("▲", x + w/2 - 6, y + 16, style->font_family, 14, false, arrow_c);

        // Value
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", value);
        renderer.draw_rect(x, y + 25, w, 40, 4, Paint::solid({0.95f, 0.95f, 0.95f, 1}), Paint::none(), 0);
        renderer.draw_text(buf, x + w/2 - 10, y + 52, style->font_family, 20, true, {0.1f, 0.1f, 0.1f, 1});

        // Down arrow
        arrow_c = (hover_spinner_ == idx && !hover_up_) ? Color{0.3f, 0.4f, 0.8f, 1} : Color{0.4f, 0.4f, 0.4f, 1};
        renderer.draw_text("▼", x + w/2 - 6, y + 85, style->font_family, 14, false, arrow_c);

        spinner_bounds_[idx] = {x, y, w, h};
    }

    void handle_spinner_click(float px, float py, Element& elem) {
        for (int i = 0; i < 3; ++i) {
            if (!hit(px, py, spinner_bounds_[i])) continue;
            
            float rel_y = py - spinner_bounds_[i].y;
            int delta = rel_y < 25 ? 1 : (rel_y > 75 ? -1 : 0);
            if (delta == 0) continue;

            int* val = (i == 0) ? &time_.hour : (i == 1) ? &time_.minute : &time_.second;
            int max_v = (i == 0) ? 23 : 59;
            
            *val = (*val + delta + max_v + 1) % (max_v + 1);
            elem.mark_paint_dirty();
            return;
        }
    }

    Time time_;
    bool show_seconds_;
    bool open_ = false;
    bool focused_ = false;
    bool hover_ok_ = false;
    int hover_spinner_ = -1;
    bool hover_up_ = false;

    Rect popup_bounds_{};
    Rect ok_bounds_{};
    Rect spinner_bounds_[3]{};
    float spinner_y_ = 0;
    float col_width_ = 60;

    ChangeCallback on_change_;
};

} // namespace flexUI

#endif
