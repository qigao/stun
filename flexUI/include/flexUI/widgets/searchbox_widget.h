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
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
#include "../text_util.h"
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

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();

        // Input background
        Color bg = focused_ ? Color{1, 1, 1, 1} : Color{0.98f, 0.98f, 0.98f, 1};
        Color border = focused_ ? Color{0.4f, 0.5f, 0.9f, 1} : Color{0.85f, 0.85f, 0.85f, 1};
        commands.draw_rect(0, 0, w, h, 4, Paint::solid(bg),
                           Paint::solid(border), 1);

        // Search icon
        draw_inline_text(commands, style, "🔍", 10.0f, h * 0.65f, 14.0f, false,
                         {0.5f, 0.5f, 0.5f, 1.0f});

        // Text or placeholder
        float text_x = 32;
        if (text_.empty() && !focused_) {
            draw_inline_text(commands, style, placeholder_, text_x, h * 0.65f, 13.0f,
                             false, {0.6f, 0.6f, 0.6f, 1.0f});
        } else {
            draw_inline_text(commands, style, text_, text_x, h * 0.65f, 13.0f, false,
                             {0.1f, 0.1f, 0.1f, 1.0f});
            
            // Cursor
            if (focused_ && cursor_visible_) {
                const size_t clamped_cursor = std::min(cursor_pos_, text_.size());
                float cursor_x =
                    text_x + text_width(style, text_.substr(0, clamped_cursor), 13.0f, false);
                commands.draw_rect(cursor_x, 8, 1, h - 16, 0,
                                   Paint::solid({0.2f, 0.2f, 0.2f, 1}),
                                   Paint::none(), 0);
            }
        }

        // Clear button
        if (!text_.empty()) {
            float clear_x = w - 28;
            draw_inline_text(commands, style, "✕", clear_x, h * 0.65f, 12.0f, false,
                             {0.5f, 0.5f, 0.5f, 1.0f});
        }
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        if (!show_dropdown_ || filtered_.empty()) return;

        auto* style = elem.computed_style;
        const auto anchor_bounds = detail::css_render_world_bounds(&elem);
        float x = anchor_bounds.x;
        float y = anchor_bounds.y + anchor_bounds.height + 2;
        float w = anchor_bounds.width;
        float max_h = style->get_variable_float("--searchbox-dropdown-max-height", 200.0f);
        float item_h = 36.0f;
        float h = std::min(max_h, filtered_.size() * item_h + 8);
        commands.draw_rect(x + 2, y + 2, w, h, 4,
                           Paint::solid({0, 0, 0, 0.12f}), Paint::none(), 0);
        commands.draw_rect(x, y, w, h, 4, Paint::solid({1, 1, 1, 1}),
                           Paint::solid({0.9f, 0.9f, 0.9f, 1}), 1);

        // Items
        float iy = y + 4;
        for (size_t i = 0; i < filtered_.size() && iy < y + h - 4; ++i) {
            const auto& item = filtered_[i];
            
            if (static_cast<int>(i) == hover_index_) {
                commands.draw_rect(x + 4, iy, w - 8, item_h, 4,
                                   Paint::solid({0.95f, 0.95f, 0.95f, 1}),
                                   Paint::none(), 0);
            }

            Color text_c{0.1f, 0.1f, 0.1f, 1};
            Color desc_c{0.5f, 0.5f, 0.5f, 1};

            if (item.description.empty()) {
                draw_inline_text(commands, style, item.text, x + 12, iy + item_h * 0.6f,
                                 13.0f, false, text_c);
            } else {
                draw_inline_text(commands, style, item.text, x + 12, iy + item_h * 0.4f,
                                 13.0f, false, text_c);
                draw_inline_text(commands, style, item.description, x + 12,
                                 iy + item_h * 0.75f, 11.0f, false, desc_c);
            }

            iy += item_h;
        }

        dropdown_bounds_ = {x, y, w, h};
        item_height_ = item_h;
    }

    bool has_overlay() const override { return show_dropdown_ && !filtered_.empty(); }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

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
                sync_host_semantics();
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
            sync_host_semantics();
            elem.mark_paint_dirty();
            return false;
        }

        if (event.type == EventType::MouseMove && show_dropdown_ && hit_dropdown(event.x, event.y)) {
            int new_hover = dropdown_item_at(event.y);
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                sync_host_semantics();
                elem.mark_paint_dirty();
            }
            return true;
        }

        if (event.type == EventType::KeyDown && focused_) {
            if (event.key == KeyCode::Escape) {
                show_dropdown_ = false;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Enter && hover_index_ >= 0) {
                select_suggestion(hover_index_, elem);
                return true;
            }
            if (event.key == KeyCode::Down && show_dropdown_) {
                hover_index_ = std::min(hover_index_ + 1, static_cast<int>(filtered_.size()) - 1);
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Up && show_dropdown_) {
                hover_index_ = std::max(hover_index_ - 1, 0);
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }
            if (event.key == KeyCode::Backspace && cursor_pos_ > 0) {
                const size_t current = cursor_pos_;
                const size_t previous = previous_codepoint_start(text_, current);
                text_.erase(previous, current - previous);
                cursor_pos_ = previous;
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
            sync_host_semantics();
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

    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override {
        (void)available_width;
        (void)available_height;
        const auto* style = elem.computed_style;
        const std::string& display = text_.empty() ? placeholder_ : text_;
        const float text_w = text_width(style, display, 13.0f, false);
        out_width = std::max(220.0f, text_w + 72.0f);
        out_height = style ? style->get_variable_float("--searchbox-height", 36.0f)
                           : 36.0f;
        return true;
    }

    bool wants_mouse_capture() const override { return show_dropdown_; }
    void sync_host_semantics_for_layout(Element& elem) override {
        (void)elem;
        sync_host_semantics();
    }

    // API
    const std::string& text() const { return text_; }
    void set_text(const std::string& t) {
        text_ = t;
        cursor_pos_ = t.size();
        filter_suggestions();
        sync_host_semantics();
    }
    void set_focused(bool focused) {
        focused_ = focused;
        sync_host_semantics();
    }
    void set_dropdown_open(bool open) {
        show_dropdown_ = open;
        sync_host_semantics();
    }

    void set_suggestions(std::vector<Suggestion> s) {
        suggestions_ = std::move(s);
        filter_suggestions();
        sync_host_semantics();
    }
    void add_suggestion(const std::string& id, const std::string& text, const std::string& desc = "") {
        suggestions_.push_back({id, text, desc});
        filter_suggestions();
        sync_host_semantics();
    }

    using ChangeCallback = std::function<void(const std::string&)>;
    using SelectCallback = std::function<void(const Suggestion&)>;
    void on_change(ChangeCallback cb) { on_change_ = std::move(cb); }
    void on_select(SelectCallback cb) { on_select_ = std::move(cb); }

    const char* type_name() const override { return "SearchBoxWidget"; }

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

    static size_t previous_codepoint_start(const std::string& text, size_t byte_pos) {
        size_t pos = std::min(byte_pos, text.size());
        if (pos == 0) {
            return 0;
        }
        --pos;
        while (pos > 0 &&
               (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u) {
            --pos;
        }
        return pos;
    }

    bool hit_dropdown(float px, float py) const {
        return px >= dropdown_bounds_.x && px < dropdown_bounds_.x + dropdown_bounds_.w &&
               py >= dropdown_bounds_.y && py < dropdown_bounds_.y + dropdown_bounds_.h;
    }

    int dropdown_item_at(float py) const {
        float rel_y = py - dropdown_bounds_.y - 4;
        return static_cast<int>(rel_y / item_height_);
    }

    void sync_host_semantics() override {
        const bool expanded = show_dropdown_ && !filtered_.empty();
        set_host_attribute("role", "combobox");
        set_host_attribute("aria-autocomplete", "list");
        set_host_attribute("aria-haspopup", "listbox");
        set_host_boolean_attribute("aria-expanded", expanded);
        set_host_boolean_attribute("aria-hidden", false);
        set_host_attribute("data-state", expanded ? "open" : "closed");
        set_host_attribute("data-result-count", std::to_string(filtered_.size()));
        if (expanded && hover_index_ >= 0 &&
            hover_index_ < static_cast<int>(filtered_.size()) &&
            !filtered_[static_cast<size_t>(hover_index_)].id.empty()) {
            set_host_attribute("data-active-id",
                               filtered_[static_cast<size_t>(hover_index_)].id);
            set_host_attribute("aria-activedescendant",
                               filtered_[static_cast<size_t>(hover_index_)].id);
        } else {
            clear_host_attribute("data-active-id");
            clear_host_attribute("aria-activedescendant");
        }
        if (!text_.empty()) {
            set_host_attribute("data-value", text_);
            set_host_attribute("aria-label", text_);
        } else {
            clear_host_attribute("data-value");
            if (!placeholder_.empty()) {
                set_host_attribute("aria-label", placeholder_);
            } else {
                clear_host_attribute("aria-label");
            }
        }
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
        sync_host_semantics();
    }

    void select_suggestion(int idx, Element& elem) {
        if (idx < 0 || idx >= static_cast<int>(filtered_.size())) return;
        
        const auto& s = filtered_[idx];
        text_ = s.text;
        cursor_pos_ = text_.size();
        show_dropdown_ = false;
        sync_host_semantics();
        
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
    size_t cursor_pos_ = 0;
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
