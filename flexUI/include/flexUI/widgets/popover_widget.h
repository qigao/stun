/*
 * flexUI - PopoverWidget
 *
 * Floating content panel anchored to a target element.
 */

#ifndef FLEXUI_POPOVER_WIDGET_H
#define FLEXUI_POPOVER_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
#include <string>
#include <functional>

namespace flexUI {

class PopoverWidget : public Widget {
public:
    enum class Position { Top, Bottom, Left, Right };
    enum class Trigger { Click, Hover };

    PopoverWidget(const std::string& content = "", Position pos = Position::Bottom, Trigger trigger = Trigger::Click)
        : content_(content), position_(pos), trigger_(trigger) {}

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        // Popover renders as overlay
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        sync_host_semantics();
        if (!visible_) return;

        auto* style = elem.computed_style;
        const auto anchor_bounds = detail::css_render_world_bounds(&elem);
        float anchor_x = anchor_bounds.x;
        float anchor_y = anchor_bounds.y;
        float anchor_w = anchor_bounds.width;
        float anchor_h = anchor_bounds.height;

        float w = width_ > 0 ? width_ : std::max(120.0f, content_width(style) + 24.0f);
        float h = height_ > 0 ? height_ : 80;

        float x = 0.0f;
        float y = 0.0f;
        switch (position_) {
            case Position::Top:
                x = anchor_x + anchor_w/2 - w/2;
                y = anchor_y - h - 8;
                break;
            case Position::Bottom:
                x = anchor_x + anchor_w/2 - w/2;
                y = anchor_y + anchor_h + 8;
                break;
            case Position::Left:
                x = anchor_x - w - 8;
                y = anchor_y + anchor_h/2 - h/2;
                break;
            case Position::Right:
                x = anchor_x + anchor_w + 8;
                y = anchor_y + anchor_h/2 - h/2;
                break;
        }
        commands.draw_rect(x + 2, y + 2, w, h, 6,
                           Paint::solid({0, 0, 0, 0.15f}),
                           Paint::none(), 0);
        commands.draw_rect(x, y, w, h, 6, Paint::solid({1, 1, 1, 1}),
                           Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        // Arrow
        render_arrow(commands, x, y, w, h, anchor_x, anchor_y, anchor_w, anchor_h);

        // Content
        if (!title_.empty()) {
            draw_inline_text(commands, style, title_, x + 12, y + 22, 14.0f, true,
                             {0.1f, 0.1f, 0.1f, 1});
            draw_inline_text(commands, style, content_, x + 12, y + 44, 12.0f, false,
                             {0.3f, 0.3f, 0.3f, 1});
        } else {
            draw_inline_text(commands, style, content_, x + 12, y + h/2 + 5, 13.0f,
                             false, {0.2f, 0.2f, 0.2f, 1});
        }

        bounds_ = {x, y, w, h};
    }

    bool has_overlay() const override { return visible_; }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;
        bool in_anchor = lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height();

        if (trigger_ == Trigger::Hover) {
            if (event.type == EventType::MouseMove) {
                bool in_popover = visible_ && hit(event.x, event.y, bounds_);
                bool should_show = in_anchor || in_popover;
                
                if (should_show != visible_) {
                    visible_ = should_show;
                    sync_host_semantics();
                    elem.mark_paint_dirty();
                }
            }
            return visible_;
        }

        // Click trigger
        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            if (in_anchor) {
                visible_ = !visible_;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }

            if (visible_ && !hit(event.x, event.y, bounds_)) {
                visible_ = false;
                sync_host_semantics();
                elem.mark_paint_dirty();
            }
        }

        return visible_;
    }

    bool wants_mouse_capture() const override { return visible_ && trigger_ == Trigger::Click; }

    // API
    void show() { visible_ = true; sync_host_semantics(); }
    void hide() { visible_ = false; sync_host_semantics(); }
    bool is_visible() const { return visible_; }

    void set_content(const std::string& c) { content_ = c; sync_host_semantics(); }
    void set_title(const std::string& t) { title_ = t; sync_host_semantics(); }
    void set_size(float w, float h) { width_ = w; height_ = h; }
    void set_position(Position p) { position_ = p; sync_host_semantics(); }

    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override {
        (void)available_width;
        (void)available_height;
        const auto* style = elem.computed_style;
        out_width = width_ > 0.0f ? width_ : std::max(120.0f, content_width(style) + 24.0f);
        out_height = height_ > 0.0f ? height_ : (!title_.empty() ? 80.0f : 44.0f);
        return true;
    }

    const char* type_name() const override { return "PopoverWidget"; }

private:
    struct Rect { float x, y, w, h; };

    const char* position_name() const {
        switch (position_) {
            case Position::Top: return "top";
            case Position::Bottom: return "bottom";
            case Position::Left: return "left";
            case Position::Right: return "right";
        }
        return "bottom";
    }

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

    float content_width(const ComputedStyle* base_style) const {
        const auto content_style = make_text_style(base_style, title_.empty() ? 13.0f : 12.0f,
                                                   false);
        float width = approximate_segmented_text_width(&content_style, content_);
        if (!title_.empty()) {
            const auto title_style = make_text_style(base_style, 14.0f, true);
            width = std::max(width, approximate_segmented_text_width(&title_style, title_));
        }
        return width;
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

    void sync_host_semantics() override {
        set_host_attribute("data-state", visible_ ? "open" : "closed");
        set_host_attribute("data-side", position_name());
        set_host_boolean_attribute("aria-expanded", visible_);
        set_host_boolean_attribute("aria-hidden", !visible_);
        set_host_attribute("aria-haspopup", "dialog");
        if (!title_.empty()) {
            set_host_attribute("aria-label", title_);
        } else if (!content_.empty()) {
            set_host_attribute("aria-label", content_);
        } else {
            clear_host_attribute("aria-label");
        }
    }

    void render_arrow(RenderCommandList& commands, float x, float y, float w, float h,
                      float ax, float ay, float aw, float ah) {
        // Simple arrow using triangle approximation
        (void)ax;
        (void)ay;
        (void)aw;
        (void)ah;
        Color c{1, 1, 1, 1};
        float size = 8;

        switch (position_) {
            case Position::Top:
                commands.draw_rect(x + w/2 - size/2, y + h - 1, size, size, 0,
                                   Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Bottom:
                commands.draw_rect(x + w/2 - size/2, y - size + 1, size, size, 0,
                                   Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Left:
                commands.draw_rect(x + w - 1, y + h/2 - size/2, size, size, 0,
                                   Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Right:
                commands.draw_rect(x - size + 1, y + h/2 - size/2, size, size, 0,
                                   Paint::solid(c), Paint::none(), 0);
                break;
        }
    }

    std::string content_;
    std::string title_;
    Position position_;
    Trigger trigger_;
    float width_ = 0;
    float height_ = 0;
    bool visible_ = false;
    Rect bounds_{};
};

} // namespace flexUI

#endif
