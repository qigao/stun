/*
 * flexUI - ScrollViewWidget
 *
 * A scrollable container with scrollbars.
 * 
 * CSS Variables:
 *   --scrollbar-width: 8px
 *   --scrollbar-bg: rgba(0,0,0,0.1)
 *   --scrollbar-thumb: rgba(0,0,0,0.3)
 *   --scrollbar-thumb-hover: rgba(0,0,0,0.5)
 */

#ifndef FLEXUI_SCROLLVIEW_WIDGET_H
#define FLEXUI_SCROLLVIEW_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include <algorithm>
#include <cmath>

namespace flexUI {

class ScrollViewWidget : public Widget {
public:
    enum class Policy { Auto, Always, Never };
    
    ScrollViewWidget(Policy h = Policy::Auto, Policy v = Policy::Auto)
        : h_policy_(h), v_policy_(v) {}

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;

        update_metrics(elem);
        sync_host_semantics();
        render_scrollbars(elem, commands);
    }

    void sync_host_semantics_for_layout(Element& elem) override {
        if (!elem.computed_style) {
            return;
        }
        update_metrics(elem);
        sync_host_semantics();
    }

    void begin_scroll(const Element& elem, RenderCommandList& commands) {
        if (!has_scroll()) return;
        commands.save();
        commands.clip_rect(0, 0, view_width_, view_height_);
        commands.translate(-scroll_x_, -scroll_y_);
        commands.push_transform_prefix(
            flex::make_translation(-scroll_x_, -scroll_y_));
    }

    void end_scroll(RenderCommandList& commands) {
        if (!has_scroll()) return;
        commands.pop_transform_prefix();
        commands.restore();
    }

    bool has_scroll() const { return max_scroll_x_ > 0 || max_scroll_y_ > 0; }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

        if (event.type == EventType::MouseWheel) {
            if (max_scroll_y_ > 0) {
                scroll_y_ = std::clamp(scroll_y_ - event.delta_y * 40.0f, 0.0f, max_scroll_y_);
                elem.mark_paint_dirty();
                return true;
            }
            return false;
        }

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            if (show_v_ && hit(lx, ly, v_track_)) {
                dragging_v_ = hit(lx, ly, v_thumb_);
                if (dragging_v_) { drag_start_ = ly; drag_scroll_ = scroll_y_; }
                else scroll_y_ = std::clamp(ly / v_track_.h * max_scroll_y_, 0.0f, max_scroll_y_);
                elem.mark_paint_dirty();
                return true;
            }
            if (show_h_ && hit(lx, ly, h_track_)) {
                dragging_h_ = hit(lx, ly, h_thumb_);
                if (dragging_h_) { drag_start_ = lx; drag_scroll_ = scroll_x_; }
                else scroll_x_ = std::clamp(lx / h_track_.w * max_scroll_x_, 0.0f, max_scroll_x_);
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseMove) {
            if (dragging_v_) {
                float range = v_track_.h - v_thumb_.h;
                if (range > 0) scroll_y_ = std::clamp(drag_scroll_ + (ly - drag_start_) / range * max_scroll_y_, 0.0f, max_scroll_y_);
                elem.mark_paint_dirty();
                return true;
            }
            if (dragging_h_) {
                float range = h_track_.w - h_thumb_.w;
                if (range > 0) scroll_x_ = std::clamp(drag_scroll_ + (lx - drag_start_) / range * max_scroll_x_, 0.0f, max_scroll_x_);
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseUp && (dragging_v_ || dragging_h_)) {
            dragging_v_ = dragging_h_ = false;
            elem.mark_paint_dirty();
            return true;
        }

        return false;
    }

    bool wants_mouse_capture() const override { return dragging_v_ || dragging_h_; }

    void scroll_to(float x, float y) {
        scroll_x_ = std::clamp(x, 0.0f, max_scroll_x_);
        scroll_y_ = std::clamp(y, 0.0f, max_scroll_y_);
    }
    
    float scroll_x() const { return scroll_x_; }
    float scroll_y() const { return scroll_y_; }
    float view_width() const { return view_width_; }
    float view_height() const { return view_height_; }

    const char* type_name() const override { return "ScrollViewWidget"; }

private:
    struct Rect { float x, y, w, h; };
    
    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    void sync_host_semantics() override {
        set_host_attribute("role", "region");
        set_host_attribute("data-state", has_scroll() ? "scrollable" : "idle");
        set_host_boolean_attribute("data-scroll-x", max_scroll_x_ > 0.0f);
        set_host_boolean_attribute("data-scroll-y", max_scroll_y_ > 0.0f);
    }

    void update_metrics(const Element& elem) {
        auto* style = elem.computed_style;
        float w = elem.width(), h = elem.height();
        bar_width_ = style->get_variable_float("--scrollbar-width", 8.0f);

        content_width_ = content_height_ = 0;
        for (size_t i = 0; i < elem.child_count(); ++i) {
            auto* child = elem.child_at(i);
            if (!child || !child->computed_style) continue;
            float child_x = child->x();
            float child_y = child->y();
            float child_width = child->width();
            float child_height = child->height();
            if (child_width <= 0.0f && child->computed_style->width > 0.0f) {
                child_width = child->computed_style->width;
            }
            if (child_height <= 0.0f && child->computed_style->height > 0.0f) {
                child_height = child->computed_style->height;
            }
            content_width_ = std::max(content_width_, child_x + child_width);
            content_height_ = std::max(content_height_, child_y + child_height);
        }

        show_v_ = (v_policy_ == Policy::Always) || (v_policy_ == Policy::Auto && content_height_ > h);
        show_h_ = (h_policy_ == Policy::Always) || (h_policy_ == Policy::Auto && content_width_ > w);

        view_width_ = w;
        view_height_ = h;
        max_scroll_x_ = std::max(0.0f, content_width_ - view_width_);
        max_scroll_y_ = std::max(0.0f, content_height_ - view_height_);
        scroll_x_ = std::clamp(scroll_x_, 0.0f, max_scroll_x_);
        scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll_y_);
    }

    void render_scrollbars(const Element& elem, RenderCommandList& commands) {
        auto* style = elem.computed_style;
        Color bg = style->get_variable_color("--scrollbar-bg", {0, 0, 0, 0.1f});
        Color thumb = style->get_variable_color("--scrollbar-thumb", {0, 0, 0, 0.3f});
        Color hover = style->get_variable_color("--scrollbar-thumb-hover", {0, 0, 0, 0.5f});
        float w = elem.width(), h = elem.height();

        if (show_v_ && content_height_ > 0 && bar_width_ > 0.0f) {
            float tx = w - bar_width_, th = h;
            commands.draw_rect(tx, 0, bar_width_, th, bar_width_/2,
                               Paint::solid(bg), Paint::none(), 0);
            
            float thumbH = std::max(20.0f, th * view_height_ / content_height_);
            float thumbY = max_scroll_y_ > 0 ? (th - thumbH) * scroll_y_ / max_scroll_y_ : 0;
            commands.draw_rect(tx, thumbY, bar_width_, thumbH, bar_width_/2,
                               Paint::solid(dragging_v_ ? hover : thumb),
                               Paint::none(), 0);
            
            v_track_ = {tx, 0, bar_width_, th};
            v_thumb_ = {tx, thumbY, bar_width_, thumbH};
        }

        if (show_h_ && content_width_ > 0 && bar_width_ > 0.0f) {
            float ty = h - bar_width_, tw = w;
            commands.draw_rect(0, ty, tw, bar_width_, bar_width_/2,
                               Paint::solid(bg), Paint::none(), 0);
            
            float thumbW = std::max(20.0f, tw * view_width_ / content_width_);
            float thumbX = max_scroll_x_ > 0 ? (tw - thumbW) * scroll_x_ / max_scroll_x_ : 0;
            commands.draw_rect(thumbX, ty, thumbW, bar_width_, bar_width_/2,
                               Paint::solid(dragging_h_ ? hover : thumb),
                               Paint::none(), 0);
            
            h_track_ = {0, ty, tw, bar_width_};
            h_thumb_ = {thumbX, ty, thumbW, bar_width_};
        }
    }

    Policy h_policy_, v_policy_;
    float scroll_x_ = 0, scroll_y_ = 0;
    float max_scroll_x_ = 0, max_scroll_y_ = 0;
    float content_width_ = 0, content_height_ = 0;
    float view_width_ = 0, view_height_ = 0;
    float bar_width_ = 8;
    bool show_v_ = false, show_h_ = false;
    Rect v_thumb_{}, v_track_{}, h_thumb_{}, h_track_{};
    bool dragging_v_ = false, dragging_h_ = false;
    float drag_start_ = 0, drag_scroll_ = 0;
};

} // namespace flexUI

#endif
