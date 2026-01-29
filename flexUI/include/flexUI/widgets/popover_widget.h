/*
 * flexUI - PopoverWidget
 *
 * Floating content panel anchored to a target element.
 */

#ifndef FLEXUI_POPOVER_WIDGET_H
#define FLEXUI_POPOVER_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <functional>

namespace flexUI {

class PopoverWidget : public Widget {
public:
    enum class Position { Top, Bottom, Left, Right };
    enum class Trigger { Click, Hover };

    PopoverWidget(const std::string& content = "", Position pos = Position::Bottom, Trigger trigger = Trigger::Click)
        : content_(content), position_(pos), trigger_(trigger) {}

    void render(const Element& elem, Renderer& renderer) override {
        // Popover renders as overlay
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!visible_) return;

        auto* style = elem.computed_style;
        float anchor_x = elem.absolute_x();
        float anchor_y = elem.absolute_y();
        float anchor_w = elem.width();
        float anchor_h = elem.height();

        float w = width_ > 0 ? width_ : std::max(120.0f, content_.size() * 8.0f + 24);
        float h = height_ > 0 ? height_ : 80;

        float x, y;
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

        // Shadow
        renderer.draw_rect(x + 2, y + 2, w, h, 6, Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);

        // Background
        renderer.draw_rect(x, y, w, h, 6, Paint::solid({1, 1, 1, 1}), Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        // Arrow
        render_arrow(x, y, w, h, anchor_x, anchor_y, anchor_w, anchor_h, renderer);

        // Content
        if (!title_.empty()) {
            renderer.draw_text(title_, x + 12, y + 22, style->font_family, 14, true, {0.1f, 0.1f, 0.1f, 1});
            renderer.draw_text(content_, x + 12, y + 44, style->font_family, 12, false, {0.3f, 0.3f, 0.3f, 1});
        } else {
            renderer.draw_text(content_, x + 12, y + h/2 + 5, style->font_family, 13, false, {0.2f, 0.2f, 0.2f, 1});
        }

        bounds_ = {x, y, w, h};
    }

    bool has_overlay() const override { return visible_; }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();
        bool in_anchor = lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height();

        if (trigger_ == Trigger::Hover) {
            if (event.type == EventType::MouseMove) {
                bool in_popover = visible_ && hit(event.x, event.y, bounds_);
                bool should_show = in_anchor || in_popover;
                
                if (should_show != visible_) {
                    visible_ = should_show;
                    elem.mark_paint_dirty();
                }
            }
            return visible_;
        }

        // Click trigger
        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            if (in_anchor) {
                visible_ = !visible_;
                elem.mark_paint_dirty();
                return true;
            }

            if (visible_ && !hit(event.x, event.y, bounds_)) {
                visible_ = false;
                elem.mark_paint_dirty();
            }
        }

        return visible_;
    }

    bool wants_mouse_capture() const override { return visible_ && trigger_ == Trigger::Click; }

    // API
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool is_visible() const { return visible_; }

    void set_content(const std::string& c) { content_ = c; }
    void set_title(const std::string& t) { title_ = t; }
    void set_size(float w, float h) { width_ = w; height_ = h; }
    void set_position(Position p) { position_ = p; }

    const char* type_name() const override { return "PopoverWidget"; }

private:
    struct Rect { float x, y, w, h; };

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    void render_arrow(float x, float y, float w, float h, 
                      float ax, float ay, float aw, float ah, Renderer& renderer) {
        // Simple arrow using triangle approximation
        Color c{1, 1, 1, 1};
        float size = 8;

        switch (position_) {
            case Position::Top:
                renderer.draw_rect(x + w/2 - size/2, y + h - 1, size, size, 0, Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Bottom:
                renderer.draw_rect(x + w/2 - size/2, y - size + 1, size, size, 0, Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Left:
                renderer.draw_rect(x + w - 1, y + h/2 - size/2, size, size, 0, Paint::solid(c), Paint::none(), 0);
                break;
            case Position::Right:
                renderer.draw_rect(x - size + 1, y + h/2 - size/2, size, size, 0, Paint::solid(c), Paint::none(), 0);
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
