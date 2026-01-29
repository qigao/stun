/*
 * flexUI - SearchBoxWidget
 *
 * Input field with autocomplete dropdown.
 * 
 * CSS Variables:
 *   --searchbox-height: 36px
 *   --searchbox-dropdown-max-height: 200px
 */

#ifndef FLEXUI_SEARCHBOX_WIDGET_H
#define FLEXUI_SEARCHBOX_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

namespace flexUI {

class SearchBoxWidget : public Widget {
public:
    struct Suggestion {
        std::string id;
        std::string text;
        std::string description;
    };

    SearchBoxWidget(const std::string& placeholder = "Search...")
        : placeholder_(placeholder) {}

    void render(const Element& elem, Renderer& renderer) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();

        // Input background
        Color bg = focused_ ? Color{1, 1, 1, 1} : Color{0.98f, 0.98f, 0.98f, 1};
        Color border = focused_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.85f, 0.85f, 0.85f, 1};
        renderer.draw_rect(0, 0, w, h, 4, Paint::solid(bg), Paint::solid(border), 1);

        // Search icon
        renderer.draw_text("🔍", 10, h * 0.65f, style->font_family, 14, false, {0.5f, 0.5f, 0.5f, 1});

        // Text or placeholder
        float text_x = 32;
        if (text_.empty() && !focused_) {
            renderer.draw_text(placeholder_, text_x, h * 0.65f, style->font_family, 13, false, {0.6f, 0.6f, 0.6f, 1});
        } else {
            renderer.draw_text(text_, text_x, h * 0.65f, style->font_family, 13, false, {0.1f, 0.1f, 0.1f, 1});
            
            // Cursor
            if (focused_ && cursor_visible_) {
                float cursor_x = text_x + cursor_pos_ * 8;
                renderer.draw_rect(cursor_x, 8, 1, h - 16, 0, Paint::solid({0.2f, 0.2f, 0.2f, 1}), Paint::none(), 0);
            }
        }

        // Clear button
        if (!text_.empty()) {
            float clear_x = w - 28;
            renderer.draw_text("✕", clear_x, h * 0.65f, style->font_family, 12, false, {0.5f, 0.5f, 0.5f, 1});
        }
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!show_dropdown_ || filtered_.empty()) return;

        auto* style = elem.computed_style;
        float x = elem.absolute_x();
        float y = elem.absolute_y() + elem.height() + 2;
        float w = elem.width();
        float max_h = style->get_variable_float("--searchbox-dropdown-max-height", 200.0f);
        float item_h = 36.0f;
        float h = std::min(max_h, filtered_.size() * item_h + 8);

        // Shadow
        renderer.draw_rect(x + 2, y + 2, w, h, 4, Paint::solid({0, 0, 0, 0.12f}), Paint::none(), 0);

        // Background
        renderer.draw_rect(x, y, w, h, 4, Paint::solid({1, 1, 1, 1}), Paint::solid({0.9f, 0.9f, 0.9f, 1}), 1);

        // Items
        float iy = y + 4;
        for (size_t i = 0; i < filtered_.size() && iy < y + h - 4; ++i) {
            const auto& item = filtered_[i];
            
            if (static_cast<int>(i) == hover_index_) {
                renderer.draw_rect(x + 4, iy, w - 8, item_h, 4, Paint::solid({0.95f, 0.95f, 0.95f, 1}), Paint::none(), 0);
            }

            Color text_c{0.1f, 0.1f, 0.1f, 1};
            Color desc_c{0.5f, 0.5f, 0.5f, 1};

            if (item.description.empty()) {
                renderer.draw_text(item.text, x + 12, iy + item_h * 0.6f, style->font_family, 13, false, text_c);
            } else {
                renderer.draw_text(item.text, x + 12, iy + item_h * 0.4f, style->font_family, 13, false, text_c);
                renderer.draw_text(item.description, x + 12, iy + item_h * 0.75f, style->font_family, 11, false, desc_c);
            }

            iy += item_h;
        }

        dropdown_bounds_ = {x, y, w, h};
        item_height_ = item_h;
    }

    bool has_overlay() const override { return show_dropdown_ && !filtered_.empty(); }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Click on input
            if (lx >= 0 && lx < elem.width() && ly >= 0 && ly < elem.height()) {
                // Clear button
                if (!text_.empty() && lx > elem.width() - 32) {
                    text_.clear();
                    cursor_pos_ = 0;
                    filter_suggestions();
                    if (on_change_) on_change_(text_);
                    elem.mark_paint_dirty();
                    return true;
                }

                focused_ = true;
                show_dropdown_ = !text_.empty() || !suggestions_.empty();
                elem.mark_paint_dirty();
                return true;
            }

            // Click on dropdown
            if (show_dropdown_ && hit_dropdown(event.x, event.y)) {
                int idx = dropdown_item_at(event.y);
                if (idx >= 0 && idx < static_cast<int>(filtered_.size())) {
                    select_suggestion(idx, elem);
                }
                return true;
            }

            // Click outside
            focused_ = false;
            show_dropdown_ = false;
            elem.mark_paint_dirty();
            return false;
        }

        if (event.type == EventType::MouseMove && show_dropdown_ && hit_dropdown(event.x, event.y)) {
            int new_hover = dropdown_item_at(event.y);
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                elem.mark_paint_dirty();
            }
            return true;
        }

        if (event.type == EventType::KeyDown && focused_) {
            if (event.key == KeyCode::Escape) {
                show_dropdown_ = false;
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Enter && hover_index_ >= 0) {
                select_suggestion(hover_index_, elem);
                return true;
            }
            if (event.key == KeyCode::Down && show_dropdown_) {
                hover_index_ = std::min(hover_index_ + 1, static_cast<int>(filtered_.size()) - 1);
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Up && show_dropdown_) {
                hover_index_ = std::max(hover_index_ - 1, 0);
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Backspace && cursor_pos_ > 0) {
                text_.erase(cursor_pos_ - 1, 1);
                cursor_pos_--;
                filter_suggestions();
                if (on_change_) on_change_(text_);
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::TextInput && focused_) {
            text_.insert(cursor_pos_, event.text);
            cursor_pos_ += event.text.size();
            filter_suggestions();
            show_dropdown_ = true;
            if (on_change_) on_change_(text_);
            elem.mark_paint_dirty();
            return true;
        }

        return false;
    }

    void update(float delta_ms, Element& elem) override {
        cursor_timer_ += delta_ms;
        if (cursor_timer_ > 500) {
            cursor_timer_ = 0;
            cursor_visible_ = !cursor_visible_;
            if (focused_) elem.mark_paint_dirty();
        }
    }

    bool wants_mouse_capture() const override { return show_dropdown_; }

    // API
    const std::string& text() const { return text_; }
    void set_text(const std::string& t) { text_ = t; cursor_pos_ = t.size(); filter_suggestions(); }

    void set_suggestions(std::vector<Suggestion> s) { suggestions_ = std::move(s); filter_suggestions(); }
    void add_suggestion(const std::string& id, const std::string& text, const std::string& desc = "") {
        suggestions_.push_back({id, text, desc});
    }

    using ChangeCallback = std::function<void(const std::string&)>;
    using SelectCallback = std::function<void(const Suggestion&)>;
    void on_change(ChangeCallback cb) { on_change_ = std::move(cb); }
    void on_select(SelectCallback cb) { on_select_ = std::move(cb); }

    const char* type_name() const override { return "SearchBoxWidget"; }

private:
    struct Rect { float x, y, w, h; };

    bool hit_dropdown(float px, float py) const {
        return px >= dropdown_bounds_.x && px < dropdown_bounds_.x + dropdown_bounds_.w &&
               py >= dropdown_bounds_.y && py < dropdown_bounds_.y + dropdown_bounds_.h;
    }

    int dropdown_item_at(float py) const {
        float rel_y = py - dropdown_bounds_.y - 4;
        return static_cast<int>(rel_y / item_height_);
    }

    void filter_suggestions() {
        filtered_.clear();
        if (text_.empty()) {
            filtered_ = suggestions_;
        } else {
            std::string lower_text = text_;
            std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
            
            for (const auto& s : suggestions_) {
                std::string lower_s = s.text;
                std::transform(lower_s.begin(), lower_s.end(), lower_s.begin(), ::tolower);
                if (lower_s.find(lower_text) != std::string::npos) {
                    filtered_.push_back(s);
                }
            }
        }
        hover_index_ = filtered_.empty() ? -1 : 0;
    }

    void select_suggestion(int idx, Element& elem) {
        if (idx < 0 || idx >= static_cast<int>(filtered_.size())) return;
        
        const auto& s = filtered_[idx];
        text_ = s.text;
        cursor_pos_ = text_.size();
        show_dropdown_ = false;
        
        if (on_select_) on_select_(s);
        if (on_change_) on_change_(text_);
        elem.mark_paint_dirty();
    }

    std::string text_;
    std::string placeholder_;
    std::vector<Suggestion> suggestions_;
    std::vector<Suggestion> filtered_;
    
    bool focused_ = false;
    bool show_dropdown_ = false;
    int cursor_pos_ = 0;
    int hover_index_ = -1;
    
    bool cursor_visible_ = true;
    float cursor_timer_ = 0;
    
    Rect dropdown_bounds_{};
    float item_height_ = 36;
    
    ChangeCallback on_change_;
    SelectCallback on_select_;
};

} // namespace flexUI

#endif
