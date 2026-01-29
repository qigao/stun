/*
 * flexUI - NotificationWidget
 *
 * System-level notification manager. Shows stacked notifications.
 */

#ifndef FLEXUI_NOTIFICATION_WIDGET_H
#define FLEXUI_NOTIFICATION_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

namespace flexUI {

class NotificationWidget : public Widget {
public:
    enum class Type { Info, Success, Warning, Error };
    enum class Position { TopRight, TopLeft, BottomRight, BottomLeft };

    struct Notification {
        int id;
        std::string title;
        std::string message;
        Type type;
        float duration;
        float elapsed = 0;
        float opacity = 0;
        bool closing = false;
    };

    NotificationWidget(Position pos = Position::TopRight) : position_(pos) {}

    int notify(const std::string& title, const std::string& message, 
               Type type = Type::Info, float duration = 5000) {
        int id = next_id_++;
        notifications_.push_back({id, title, message, type, duration, 0, 0, false});
        return id;
    }

    void dismiss(int id) {
        for (auto& n : notifications_) {
            if (n.id == id) n.closing = true;
        }
    }

    void render(const Element& elem, Renderer& renderer) override {
        // Notifications render as overlay
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (notifications_.empty()) return;

        auto* style = elem.computed_style;
        float vp_w = elem.owner_box_ ? elem.owner_box_->get_viewport_width() : 800;
        float vp_h = elem.owner_box_ ? elem.owner_box_->get_viewport_height() : 600;

        float notif_w = 320;
        float notif_h = 80;
        float gap = 8;
        float margin = 16;

        float base_x = (position_ == Position::TopRight || position_ == Position::BottomRight) 
                       ? vp_w - notif_w - margin : margin;
        float base_y = (position_ == Position::TopRight || position_ == Position::TopLeft)
                       ? margin : vp_h - margin;
        float dir = (position_ == Position::TopRight || position_ == Position::TopLeft) ? 1 : -1;

        float y = base_y;
        for (size_t i = 0; i < notifications_.size(); ++i) {
            const auto& n = notifications_[i];
            if (dir < 0) y -= notif_h;

            float alpha = n.opacity;
            if (alpha <= 0) { y += dir * (notif_h + gap); continue; }

            // Colors by type
            Color bg, accent;
            switch (n.type) {
                case Type::Success: bg = {0.92f, 0.98f, 0.92f, alpha}; accent = {0.2f, 0.7f, 0.3f, alpha}; break;
                case Type::Warning: bg = {1.0f, 0.98f, 0.9f, alpha}; accent = {0.9f, 0.7f, 0.2f, alpha}; break;
                case Type::Error:   bg = {1.0f, 0.93f, 0.93f, alpha}; accent = {0.85f, 0.25f, 0.25f, alpha}; break;
                default:            bg = {0.95f, 0.97f, 1.0f, alpha}; accent = {0.3f, 0.5f, 0.9f, alpha}; break;
            }

            // Shadow
            renderer.draw_rect(base_x + 2, y + 2, notif_w, notif_h, 8, 
                              Paint::solid({0, 0, 0, 0.1f * alpha}), Paint::none(), 0);

            // Background
            renderer.draw_rect(base_x, y, notif_w, notif_h, 8, Paint::solid(bg), Paint::none(), 0);

            // Accent bar
            renderer.draw_rect(base_x, y, 4, notif_h, 0, Paint::solid(accent), Paint::none(), 0);

            // Icon
            const char* icon = n.type == Type::Success ? "✓" : n.type == Type::Warning ? "⚠" : 
                              n.type == Type::Error ? "✕" : "ℹ";
            renderer.draw_text(icon, base_x + 16, y + 28, style->font_family, 18, false, accent);

            // Title
            Color title_c{0.1f, 0.1f, 0.1f, alpha};
            renderer.draw_text(n.title, base_x + 44, y + 26, style->font_family, 14, true, title_c);

            // Message
            Color msg_c{0.4f, 0.4f, 0.4f, alpha};
            renderer.draw_text(n.message, base_x + 44, y + 50, style->font_family, 12, false, msg_c);

            // Close button
            Color close_c{0.5f, 0.5f, 0.5f, alpha};
            renderer.draw_text("✕", base_x + notif_w - 24, y + 24, style->font_family, 14, false, close_c);

            notif_bounds_.push_back({static_cast<int>(i), base_x, y, notif_w, notif_h});

            y += dir * (notif_h + gap);
        }
    }

    bool has_overlay() const override { return !notifications_.empty(); }

    void update(float delta_ms, Element& elem) override {
        bool changed = false;

        for (auto& n : notifications_) {
            n.elapsed += delta_ms;

            // Fade in
            if (!n.closing && n.opacity < 1.0f) {
                n.opacity = std::min(1.0f, n.opacity + delta_ms / 200.0f);
                changed = true;
            }

            // Auto dismiss
            if (!n.closing && n.duration > 0 && n.elapsed > n.duration) {
                n.closing = true;
            }

            // Fade out
            if (n.closing) {
                n.opacity = std::max(0.0f, n.opacity - delta_ms / 200.0f);
                changed = true;
            }
        }

        // Remove fully faded
        notifications_.erase(
            std::remove_if(notifications_.begin(), notifications_.end(),
                          [](const Notification& n) { return n.closing && n.opacity <= 0; }),
            notifications_.end());

        notif_bounds_.clear();

        if (changed) elem.mark_paint_dirty();
    }

    bool handle_event(const Event& event, Element& elem) override {
        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            for (const auto& b : notif_bounds_) {
                if (event.x >= b.x && event.x < b.x + b.w && event.y >= b.y && event.y < b.y + b.h) {
                    // Close button area
                    if (event.x > b.x + b.w - 32) {
                        notifications_[b.idx].closing = true;
                        elem.mark_paint_dirty();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    const char* type_name() const override { return "NotificationWidget"; }

private:
    struct Bounds { int idx; float x, y, w, h; };

    Position position_;
    std::vector<Notification> notifications_;
    std::vector<Bounds> notif_bounds_;
    int next_id_ = 1;
};

} // namespace flexUI

#endif
