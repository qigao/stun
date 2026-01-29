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
#include "../renderer.h"
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
    }

    void hide() {
        visible_ = false;
        hover_index_ = -1;
        submenu_index_ = -1;
    }

    bool is_visible() const { return visible_; }

    void render(const Element& elem, Renderer& renderer) override {
        // Menu renders as overlay, not in normal flow
    }

    void render_overlay(const Element& elem, Renderer& renderer) override {
        if (!visible_) return;

        auto* style = elem.computed_style;
        Color bg = style->get_variable_color("--menu-bg", {1, 1, 1, 1});
        Color border = style->get_variable_color("--menu-border", {0.88f, 0.88f, 0.88f, 1});
        Color hover_bg = style->get_variable_color("--menu-item-hover", {0.94f, 0.94f, 0.94f, 1});
        float item_h = style->get_variable_float("--menu-item-height", 32.0f);
        float padding = style->get_variable_float("--menu-padding", 4.0f);

        float menu_w = calculate_width(style);
        float menu_h = calculate_height(item_h, padding);

        // Shadow
        renderer.draw_rect(menu_x_ + 2, menu_y_ + 2, menu_w, menu_h, 4,
                          Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);

        // Background
        renderer.draw_rect(menu_x_, menu_y_, menu_w, menu_h, 4,
                          Paint::solid(bg), Paint::solid(border), 1);

        // Items
        float y = menu_y_ + padding;
        for (size_t i = 0; i < items_.size(); ++i) {
            const auto& item = items_[i];
            
            if (item.separator) {
                renderer.draw_rect(menu_x_ + 8, y + 4, menu_w - 16, 1, 0,
                                  Paint::solid(border), Paint::none(), 0);
                y += 9;
                continue;
            }

            // Hover background
            if (static_cast<int>(i) == hover_index_ && item.enabled) {
                renderer.draw_rect(menu_x_ + padding, y, menu_w - padding * 2, item_h, 4,
                                  Paint::solid(hover_bg), Paint::none(), 0);
            }

            // Label
            Color text_color = item.enabled ? Color{0.1f, 0.1f, 0.1f, 1} : Color{0.6f, 0.6f, 0.6f, 1};
            renderer.draw_text(item.label, menu_x_ + 12, y + item_h * 0.65f,
                              style->font_family, 13, false, text_color);

            // Shortcut
            if (!item.shortcut.empty()) {
                float shortcut_x = menu_x_ + menu_w - 12 - item.shortcut.size() * 7;
                renderer.draw_text(item.shortcut, shortcut_x, y + item_h * 0.65f,
                                  style->font_family, 12, false, {0.5f, 0.5f, 0.5f, 1});
            }

            // Submenu arrow
            if (!item.children.empty()) {
                renderer.draw_text(">", menu_x_ + menu_w - 16, y + item_h * 0.65f,
                                  style->font_family, 13, false, text_color);
            }

            // Checkmark
            if (item.checked) {
                renderer.draw_text("✓", menu_x_ + 4, y + item_h * 0.65f,
                                  style->font_family, 12, false, text_color);
            }

            y += item_h;
        }

        // Render submenu
        if (submenu_index_ >= 0 && submenu_index_ < static_cast<int>(items_.size())) {
            render_submenu(items_[submenu_index_].children, menu_x_ + menu_w - 4,
                          menu_y_ + padding + submenu_index_ * item_h, style, renderer, item_h, padding);
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

    const char* type_name() const override { return "MenuWidget"; }

    std::vector<MenuItem>& items() { return items_; }

private:
    float calculate_width(const ComputedStyle* style) const {
        float max_w = 120;
        for (const auto& item : items_) {
            float w = item.label.size() * 8 + 40;
            if (!item.shortcut.empty()) w += item.shortcut.size() * 7 + 20;
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
                        const ComputedStyle* style, Renderer& renderer,
                        float item_h, float padding) {
        if (items.empty()) return;

        Color bg = style->get_variable_color("--menu-bg", {1, 1, 1, 1});
        Color border = style->get_variable_color("--menu-border", {0.88f, 0.88f, 0.88f, 1});

        float w = 120;
        for (const auto& item : items) {
            w = std::max(w, item.label.size() * 8.0f + 40);
        }
        float h = padding * 2 + items.size() * item_h;

        renderer.draw_rect(x + 2, y + 2, w, h, 4, Paint::solid({0, 0, 0, 0.15f}), Paint::none(), 0);
        renderer.draw_rect(x, y, w, h, 4, Paint::solid(bg), Paint::solid(border), 1);

        float iy = y + padding;
        for (const auto& item : items) {
            Color tc = item.enabled ? Color{0.1f, 0.1f, 0.1f, 1} : Color{0.6f, 0.6f, 0.6f, 1};
            renderer.draw_text(item.label, x + 12, iy + item_h * 0.65f, style->font_family, 13, false, tc);
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
