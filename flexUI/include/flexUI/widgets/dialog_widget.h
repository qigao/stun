/*
 * flexUI - DialogWidget
 *
 * Modal dialog with customizable buttons.
 */

#ifndef FLEXUI_DIALOG_WIDGET_H
#define FLEXUI_DIALOG_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

class DialogWidget : public Widget {
public:
    enum class Type { Info, Confirm, Warning, Error, Custom };

    struct Button {
        std::string id;
        std::string label;
        bool primary = false;
    };

    DialogWidget(const std::string& title = "Dialog", Type type = Type::Info)
        : title_(title), type_(type) {
        setup_default_buttons();
    }

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool is_visible() const { return visible_; }

    void render(const Element& elem, Renderer& renderer) override {
        // Dialog renders as overlay
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!visible_) return;

        auto* style = elem.computed_style;
        float vp_w = elem.owner_box_ ? elem.owner_box_->get_viewport_width() : 800;
        float vp_h = elem.owner_box_ ? elem.owner_box_->get_viewport_height() : 600;

        // Backdrop
        renderer.draw_rect(0, 0, vp_w, vp_h, 0, Paint::solid({0, 0, 0, 0.4f}), Paint::none(), 0);

        // Dialog
        float w = width_ > 0 ? width_ : 400;
        float h = height_ > 0 ? height_ : 180;
        float x = (vp_w - w) / 2;
        float y = (vp_h - h) / 2;

        // Shadow
        renderer.draw_rect(x + 4, y + 4, w, h, 8, Paint::solid({0, 0, 0, 0.2f}), Paint::none(), 0);

        // Background
        renderer.draw_rect(x, y, w, h, 8, Paint::solid({1, 1, 1, 1}), Paint::none(), 0);

        // Header
        Color header_bg = type_color();
        renderer.draw_rect(x, y, w, 48, 0, Paint::solid(header_bg), Paint::none(), 0);
        
        // Round top corners
        renderer.draw_rect(x, y, 8, 8, 0, Paint::solid(header_bg), Paint::none(), 0);
        renderer.draw_rect(x + w - 8, y, 8, 8, 0, Paint::solid(header_bg), Paint::none(), 0);

        // Title
        renderer.draw_text(title_, x + 16, y + 30, style->font_family, 16, true, {1, 1, 1, 1});

        // Close button
        renderer.draw_text("✕", x + w - 28, y + 30, style->font_family, 16, false, {1, 1, 1, 0.8f});
        close_bounds_ = {x + w - 36, y + 8, 28, 32};

        // Content
        float content_y = y + 60;
        renderer.draw_text(message_, x + 16, content_y + 16, style->font_family, 13, false, {0.2f, 0.2f, 0.2f, 1});

        // Buttons
        float btn_y = y + h - 52;
        float btn_x = x + w - 16;
        button_bounds_.clear();

        for (int i = static_cast<int>(buttons_.size()) - 1; i >= 0; --i) {
            const auto& btn = buttons_[i];
            float btn_w = btn.label.size() * 9 + 24;
            btn_x -= btn_w + 8;

            Color bg = btn.primary ? header_bg : Color{0.9f, 0.9f, 0.9f, 1};
            Color text_c = btn.primary ? Color{1, 1, 1, 1} : Color{0.2f, 0.2f, 0.2f, 1};

            if (hover_button_ == i) {
                bg.r *= 0.9f; bg.g *= 0.9f; bg.b *= 0.9f;
            }

            renderer.draw_rect(btn_x, btn_y, btn_w, 36, 4, Paint::solid(bg), Paint::none(), 0);
            renderer.draw_text(btn.label, btn_x + 12, btn_y + 24, style->font_family, 13, true, text_c);

            button_bounds_.push_back({i, btn_x, btn_y, btn_w, 36});
        }

        dialog_bounds_ = {x, y, w, h};
    }

    bool has_overlay() const override { return visible_; }

    bool handle_event(const Event& event, Element& elem) override {
        if (!visible_) return false;

        if (event.type == EventType::MouseMove) {
            hover_button_ = -1;
            for (const auto& b : button_bounds_) {
                if (hit(event.x, event.y, {b.x, b.y, b.w, b.h})) {
                    hover_button_ = b.idx;
                    break;
                }
            }
            elem.mark_paint_dirty();
            return true;
        }

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Close button
            if (hit(event.x, event.y, close_bounds_)) {
                visible_ = false;
                if (on_close_) on_close_();
                elem.mark_paint_dirty();
                return true;
            }

            // Action buttons
            for (const auto& b : button_bounds_) {
                if (hit(event.x, event.y, {b.x, b.y, b.w, b.h})) {
                    visible_ = false;
                    if (on_action_) on_action_(buttons_[b.idx].id);
                    elem.mark_paint_dirty();
                    return true;
                }
            }

            // Click outside dialog closes it
            if (!hit(event.x, event.y, dialog_bounds_)) {
                visible_ = false;
                if (on_close_) on_close_();
                elem.mark_paint_dirty();
            }

            return true;
        }

        if (event.type == EventType::KeyDown && event.key == KeyCode::Escape) {
            visible_ = false;
            if (on_close_) on_close_();
            elem.mark_paint_dirty();
            return true;
        }

        return visible_;
    }

    bool wants_mouse_capture() const override { return visible_; }

    // API
    void set_title(const std::string& t) { title_ = t; }
    void set_message(const std::string& m) { message_ = m; }
    void set_size(float w, float h) { width_ = w; height_ = h; }
    void set_type(Type t) { type_ = t; setup_default_buttons(); }

    void set_buttons(std::vector<Button> btns) { buttons_ = std::move(btns); }
    void add_button(const std::string& id, const std::string& label, bool primary = false) {
        buttons_.push_back({id, label, primary});
    }

    using ActionCallback = std::function<void(const std::string& button_id)>;
    using CloseCallback = std::function<void()>;
    void on_action(ActionCallback cb) { on_action_ = std::move(cb); }
    void on_close(CloseCallback cb) { on_close_ = std::move(cb); }

    const char* type_name() const override { return "DialogWidget"; }

private:
    struct Rect { float x, y, w, h; };
    struct BtnBounds { int idx; float x, y, w, h; };

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    Color type_color() const {
        switch (type_) {
            case Type::Confirm: return {0.3f, 0.5f, 0.9f, 1};
            case Type::Warning: return {0.9f, 0.7f, 0.2f, 1};
            case Type::Error:   return {0.85f, 0.25f, 0.25f, 1};
            default:            return {0.4f, 0.5f, 0.6f, 1};
        }
    }

    void setup_default_buttons() {
        buttons_.clear();
        switch (type_) {
            case Type::Confirm:
                buttons_ = {{"cancel", "Cancel", false}, {"ok", "OK", true}};
                break;
            case Type::Warning:
            case Type::Error:
                buttons_ = {{"ok", "OK", true}};
                break;
            default:
                buttons_ = {{"ok", "OK", true}};
                break;
        }
    }

    std::string title_;
    std::string message_;
    Type type_;
    std::vector<Button> buttons_;
    float width_ = 0;
    float height_ = 0;
    bool visible_ = false;

    Rect dialog_bounds_{};
    Rect close_bounds_{};
    std::vector<BtnBounds> button_bounds_;
    int hover_button_ = -1;

    ActionCallback on_action_;
    CloseCallback on_close_;
};

} // namespace flexUI

#endif
