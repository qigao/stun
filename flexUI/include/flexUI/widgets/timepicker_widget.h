/*
 * flexUI - TimePickerWidget
 *
 * Time selection with hour/minute/second spinners.
 */

#ifndef FLEXUI_TIMEPICKER_WIDGET_H
#define FLEXUI_TIMEPICKER_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
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

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();

        // Background
        Color bg = focused_ ? Color{1, 1, 1, 1} : Color{0.98f, 0.98f, 0.98f, 1};
        Color border = focused_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.85f, 0.85f, 0.85f, 1};
        commands.draw_rect(0, 0, w, h, 4, Paint::solid(bg),
                           Paint::solid(border), 1);

        // Time display
        char buf[16];
        if (show_seconds_) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", time_.hour, time_.minute, time_.second);
        } else {
            std::snprintf(buf, sizeof(buf), "%02d:%02d", time_.hour, time_.minute);
        }
        draw_inline_text(commands, style, buf, 12, h * 0.65f, 14.0f, false,
                         {0.1f, 0.1f, 0.1f, 1});

        // Clock icon
        draw_inline_text(commands, style, "🕐", w - 28, h * 0.65f, 14.0f, false,
                         {0.4f, 0.4f, 0.4f, 1});
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        if (!open_) return;

        auto* style = elem.computed_style;
        const auto anchor_bounds = detail::css_render_world_bounds(&elem);
        float x = anchor_bounds.x;
        float y = anchor_bounds.y + anchor_bounds.height + 4;
        float col_w = 60;
        float w = show_seconds_ ? col_w * 3 + 20 : col_w * 2 + 16;
        float h = 180;
        commands.draw_rect(x + 2, y + 2, w, h, 6,
                           Paint::solid({0, 0, 0, 0.12f}), Paint::none(), 0);
        commands.draw_rect(x, y, w, h, 6, Paint::solid({1, 1, 1, 1}),
                           Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        // Column headers
        Color header_c{0.5f, 0.5f, 0.5f, 1};
        draw_inline_text(commands, style, "Hour", x + 8, y + 20, 11.0f, false, header_c);
        draw_inline_text(commands, style, "Min", x + 8 + col_w, y + 20, 11.0f, false,
                         header_c);
        if (show_seconds_) {
            draw_inline_text(commands, style, "Sec", x + 8 + col_w * 2, y + 20, 11.0f,
                             false, header_c);
        }

        // Spinners
        float spinner_y = y + 35;
        render_spinner(commands, x + 8, spinner_y, col_w - 8, time_.hour, 0, 23, 0, style);
        render_spinner(commands, x + 8 + col_w, spinner_y, col_w - 8, time_.minute, 0, 59, 1, style);
        if (show_seconds_) {
            render_spinner(commands, x + 8 + col_w * 2, spinner_y, col_w - 8, time_.second, 0, 59, 2, style);
        }

        // OK button
        float btn_y = y + h - 40;
        Color btn_bg = hover_ok_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.3f, 0.4f, 0.8f, 1};
        commands.draw_rect(x + w/2 - 30, btn_y, 60, 28, 4,
                           Paint::solid(btn_bg), Paint::none(), 0);
        draw_inline_text(commands, style,
                         "OK", x + w * 0.5f - text_width(style, "OK", 13.0f, true) * 0.5f,
                         btn_y + 19, 13.0f, true, {1, 1, 1, 1});

        popup_bounds_ = {x, y, w, h};
        ok_bounds_ = {x + w/2 - 30, btn_y, 60, 28};
        spinner_y_ = spinner_y;
        col_width_ = col_w;
    }

    bool has_overlay() const override { return open_; }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

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

    static float text_width(const ComputedStyle* base_style, const std::string& text,
                            float font_size, bool bold) {
        const auto text_style = make_text_style(base_style, font_size, bold);
        return approximate_segmented_text_width(&text_style, text);
    }

    static float draw_inline_text(RenderCommandList& commands, const ComputedStyle* base_style,
                                  const std::string& text, float x, float baseline_y,
                                  float font_size, bool bold, const Color& color) {
        const auto text_style = make_text_style(base_style, font_size, bold);
        return emit_segmented_text_line(commands, &text_style, text, x,
                                                       baseline_y, color, bold);
    }

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    void render_spinner(RenderCommandList& commands, float x, float y, float w,
                        int value, int min_v, int max_v, int idx,
                        const ComputedStyle* style) {
        float h = 100;
        
        // Up arrow
        Color arrow_c = (hover_spinner_ == idx && hover_up_) ? Color{0.3f, 0.4f, 0.8f, 1} : Color{0.4f, 0.4f, 0.4f, 1};
        draw_inline_text(commands, style, "▲",
                         x + w * 0.5f - text_width(style, "▲", 14.0f, false) * 0.5f, y + 16,
                         14.0f, false, arrow_c);

        // Value
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", value);
        commands.draw_rect(x, y + 25, w, 40, 4,
                           Paint::solid({0.95f, 0.95f, 0.95f, 1}),
                           Paint::none(), 0);
        draw_inline_text(commands, style, buf,
                         x + w * 0.5f - text_width(style, buf, 20.0f, true) * 0.5f, y + 52,
                         20.0f, true, {0.1f, 0.1f, 0.1f, 1});

        // Down arrow
        arrow_c = (hover_spinner_ == idx && !hover_up_) ? Color{0.3f, 0.4f, 0.8f, 1} : Color{0.4f, 0.4f, 0.4f, 1};
        draw_inline_text(commands, style, "▼",
                         x + w * 0.5f - text_width(style, "▼", 14.0f, false) * 0.5f, y + 85,
                         14.0f, false, arrow_c);

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
