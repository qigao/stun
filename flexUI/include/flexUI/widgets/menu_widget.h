/*
 * flexUI - MenuWidget
 *
 * Context menu and dropdown menu with nested submenus.
 * 
 * CSS Variables:
 *   --menu-bg: #ffffff
 *   --menu-border: #e0e0e0
 *   --menu-item-hover: #f0f0f0
 *   --menu-item-height: 32px
 *   --menu-padding: 4px
 */

#ifndef FLEXUI_MENU_WIDGET_H
#define FLEXUI_MENU_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace flexUI {

class MenuWidget : public Widget {
public:
    struct MenuItem {
        std::string id;
        std::string label;
        std::string shortcut;
        bool enabled = true;
        bool separator = false;
        bool checked = false;
        std::vector<MenuItem> children;
        std::function<void()> action;
        
        static MenuItem Separator() { MenuItem m; m.separator = true; return m; }
    };

    MenuWidget() = default;

    void add_item(const std::string& id, const std::string& label, 
                  std::function<void()> action = nullptr, const std::string& shortcut = "") {
        items_.push_back({id, label, shortcut, true, false, false, {}, action});
    }

    void add_separator() { items_.push_back(MenuItem::Separator()); }

    void add_submenu(const std::string& id, const std::string& label, std::vector<MenuItem> children) {
        items_.push_back({id, label, "", true, false, false, std::move(children), nullptr});
    }

    void show(float x, float y) {
        visible_ = true;
        menu_x_ = x;
        menu_y_ = y;
        hover_index_ = -1;
        submenu_index_ = -1;
        sync_host_semantics();
    }

    void hide() {
        visible_ = false;
        hover_index_ = -1;
        submenu_index_ = -1;
        sync_host_semantics();
    }

    bool is_visible() const { return visible_; }

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        // Menu renders as overlay, not in normal flow
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        if (!visible_) return;

        auto* style = elem.computed_style;
        Color bg = style->get_variable_color("--menu-bg", {1, 1, 1, 1});
        Color border = style->get_variable_color("--menu-border", {0.88f, 0.88f, 0.88f, 1});
        Color hover_bg = style->get_variable_color("--menu-item-hover", {0.94f, 0.94f, 0.94f, 1});
        float item_h = style->get_variable_float("--menu-item-height", 32.0f);
        float padding = style->get_variable_float("--menu-padding", 4.0f);

        float menu_w = calculate_width(style);
        float menu_h = calculate_height(item_h, padding);
        commands.draw_rect(menu_x_ + 2, menu_y_ + 2, menu_w, menu_h, 4,
                           Paint::solid({0, 0, 0, 0.15f}),
                           Paint::none(), 0);
        commands.draw_rect(menu_x_, menu_y_, menu_w, menu_h, 4,
                           Paint::solid(bg), Paint::solid(border), 1);

        // Items
        float y = menu_y_ + padding;
        for (size_t i = 0; i < items_.size(); ++i) {
            const auto& item = items_[i];
            
            if (item.separator) {
                commands.draw_rect(menu_x_ + 8, y + 4, menu_w - 16, 1, 0,
                                   Paint::solid(border), Paint::none(), 0);
                y += 9;
                continue;
            }

            // Hover background
            if (static_cast<int>(i) == hover_index_ && item.enabled) {
                commands.draw_rect(menu_x_ + padding, y, menu_w - padding * 2, item_h, 4,
                                   Paint::solid(hover_bg), Paint::none(), 0);
            }

            // Label
            Color text_color = item.enabled ? Color{0.1f, 0.1f, 0.1f, 1} : Color{0.6f, 0.6f, 0.6f, 1};
            draw_inline_text(commands, style, item.label, menu_x_ + 12, y + item_h * 0.65f,
                             13.0f, false, text_color);

            // Shortcut
            if (!item.shortcut.empty()) {
                float shortcut_x =
                    menu_x_ + menu_w - 12 -
                    text_width(style, item.shortcut, 12.0f, false);
                draw_inline_text(commands, style, item.shortcut, shortcut_x,
                                 y + item_h * 0.65f, 12.0f, false,
                                 {0.5f, 0.5f, 0.5f, 1.0f});
            }

            // Submenu arrow
            if (!item.children.empty()) {
                draw_inline_text(commands, style, ">", menu_x_ + menu_w - 16,
                                 y + item_h * 0.65f, 13.0f, false, text_color);
            }

            // Checkmark
            if (item.checked) {
                draw_inline_text(commands, style, "✓", menu_x_ + 4,
                                 y + item_h * 0.65f, 12.0f, false, text_color);
            }

            y += item_h;
        }
        // Render submenu
        if (submenu_index_ >= 0 && submenu_index_ < static_cast<int>(items_.size())) {
            render_submenu(items_[submenu_index_].children, menu_x_ + menu_w - 4,
                           menu_y_ + padding + submenu_index_ * item_h,
                           style, commands, item_h, padding);
        }

        menu_width_ = menu_w;
        menu_height_ = menu_h;
        item_height_ = item_h;
        padding_ = padding;
    }

    bool has_overlay() const override { return visible_; }

    bool handle_event(const Event& event, Element& elem) override {
        if (!visible_) return false;

        float lx = event.x - menu_x_;
        float ly = event.y - menu_y_;

        if (event.type == EventType::MouseMove) {
            int new_hover = hit_test_item(lx, ly);
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                if (new_hover >= 0 && !items_[new_hover].children.empty()) {
                    submenu_index_ = new_hover;
                }
                elem.mark_paint_dirty();
            }
            return true;
        }

        if (event.type == EventType::MouseDown || event.type == EventType::MouseUp) {
            // Click outside closes menu
            if (lx < 0 || lx > menu_width_ || ly < 0 || ly > menu_height_) {
                hide();
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseUp && event.button == MouseButton::Left) {
            int idx = hit_test_item(lx, ly);
            if (idx >= 0 && items_[idx].enabled && !items_[idx].separator && items_[idx].children.empty()) {
                if (items_[idx].action) items_[idx].action();
                hide();
                elem.mark_paint_dirty();
                return true;
            }
        }

        return visible_;
    }

    bool wants_mouse_capture() const override { return visible_; }

    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override {
        (void)available_width;
        (void)available_height;
        const auto* style = elem.computed_style;
        const float item_h = style ? style->get_variable_float("--menu-item-height", 32.0f)
                                   : 32.0f;
        const float padding = style ? style->get_variable_float("--menu-padding", 4.0f)
                                    : 4.0f;
        out_width = calculate_width(style);
        out_height = calculate_height(item_h, padding);
        return true;
    }

    const char* type_name() const override { return "MenuWidget"; }

    std::vector<MenuItem>& items() { return items_; }

private:
    void sync_host_semantics() override {
        set_host_attribute("role", "menu");
        set_host_attribute("data-state", visible_ ? "open" : "closed");
        set_host_attribute("aria-orientation", "vertical");
        set_host_boolean_attribute("aria-hidden", !visible_);
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

    float calculate_width(const ComputedStyle* style) const {
        float max_w = 120;
        for (const auto& item : items_) {
            float w = text_width(style, item.label, 13.0f, false) + 40.0f;
            if (!item.shortcut.empty()) {
                w += text_width(style, item.shortcut, 12.0f, false) + 20.0f;
            }
            if (!item.children.empty()) w += 20;
            max_w = std::max(max_w, w);
        }
        return max_w;
    }

    float calculate_height(float item_h, float padding) const {
        float h = padding * 2;
        for (const auto& item : items_) {
            h += item.separator ? 9 : item_h;
        }
        return h;
    }

    int hit_test_item(float lx, float ly) const {
        if (lx < 0 || lx > menu_width_) return -1;
        
        float y = padding_;
        for (size_t i = 0; i < items_.size(); ++i) {
            float h = items_[i].separator ? 9 : item_height_;
            if (ly >= y && ly < y + h && !items_[i].separator) {
                return static_cast<int>(i);
            }
            y += h;
        }
        return -1;
    }

    void render_submenu(const std::vector<MenuItem>& items, float x, float y,
                        const ComputedStyle* style, RenderCommandList& commands,
                        float item_h, float padding) {
        if (items.empty()) return;

        Color bg = style->get_variable_color("--menu-bg", {1, 1, 1, 1});
        Color border = style->get_variable_color("--menu-border", {0.88f, 0.88f, 0.88f, 1});

        float w = 120;
        for (const auto& item : items) {
            w = std::max(w, text_width(style, item.label, 13.0f, false) + 40.0f);
        }
        float h = padding * 2 + items.size() * item_h;

        commands.draw_rect(x + 2, y + 2, w, h, 4,
                           Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);
        commands.draw_rect(x, y, w, h, 4, Paint::solid(bg),
                           Paint::solid(border), 1);

        float iy = y + padding;
        for (const auto& item : items) {
            Color tc = item.enabled ? Color{0.1f, 0.1f, 0.1f, 1} : Color{0.6f, 0.6f, 0.6f, 1};
            draw_inline_text(commands, style, item.label, x + 12, iy + item_h * 0.65f,
                             13.0f, false, tc);
            iy += item_h;
        }
    }

    std::vector<MenuItem> items_;
    bool visible_ = false;
    float menu_x_ = 0, menu_y_ = 0;
    float menu_width_ = 0, menu_height_ = 0;
    float item_height_ = 32, padding_ = 4;
    int hover_index_ = -1;
    int submenu_index_ = -1;
};

} // namespace flexUI

#endif
