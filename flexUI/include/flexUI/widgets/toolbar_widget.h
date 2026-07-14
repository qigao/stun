/*
 * flexUI - ToolbarWidget
 *
 * Horizontal toolbar with buttons, separators, and dropdowns.
 */

#ifndef FLEXUI_TOOLBAR_WIDGET_H
#define FLEXUI_TOOLBAR_WIDGET_H

#include "../widget.h"
#include "../detail/css_render_transform.h"
#include "../element.h"
#include "../event.h"
#include "../render_command.h"
#include "../text_layout.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

class ToolbarWidget : public Widget {
public:
    struct Item {
        enum Type { Button, Toggle, Separator, Dropdown } type = Button;
        std::string id;
        std::string icon;
        std::string tooltip;
        bool enabled = true;
        bool toggled = false;
        std::vector<std::pair<std::string, std::string>> dropdown_items;  // id, label
        std::function<void()> action;
    };

    ToolbarWidget() = default;

    void sync_host_semantics_for_layout(Element& elem) override {
        (void)elem;
        sync_host_semantics();
    }

    void add_button(const std::string& id, const std::string& icon, 
                    std::function<void()> action, const std::string& tooltip = "") {
        items_.push_back({Item::Button, id, icon, tooltip, true, false, {}, action});
        sync_host_semantics();
    }

    void add_toggle(const std::string& id, const std::string& icon, bool initial = false,
                    const std::string& tooltip = "") {
        items_.push_back({Item::Toggle, id, icon, tooltip, true, initial, {}, nullptr});
        sync_host_semantics();
    }

    void add_separator() {
        items_.push_back({Item::Separator, "", "", "", true, false, {}, nullptr});
        sync_host_semantics();
    }

    void add_dropdown(const std::string& id, const std::string& icon,
                      std::vector<std::pair<std::string, std::string>> items,
                      const std::string& tooltip = "") {
        items_.push_back({Item::Dropdown, id, icon, tooltip, true, false, std::move(items), nullptr});
        sync_host_semantics();
    }

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float h = elem.height();
        float btn_size = h - 8;
        float x = 4;
        commands.draw_rect(0, 0, elem.width(), h, 0,
                           Paint::solid({0.18f, 0.2f, 0.24f, 1}),
                           Paint::solid({0.25f, 0.27f, 0.3f, 1}), 1);

        item_bounds_.clear();

        for (size_t i = 0; i < items_.size(); ++i) {
            const auto& item = items_[i];

            if (item.type == Item::Separator) {
                commands.draw_rect(x + 4, 6, 1, h - 12, 0,
                                   Paint::solid({0.8f, 0.8f, 0.8f, 1}),
                                   Paint::none(), 0);
                item_bounds_.push_back({x, 4, 9, btn_size});
                x += 9;
                continue;
            }

            // Button background
            Color bg{0, 0, 0, 0};
            if (!item.enabled) {
                bg = {0.3f, 0.3f, 0.3f, 0.5f};
            } else if (static_cast<int>(i) == active_index_) {
                bg = {0.3f, 0.4f, 0.6f, 1};
            } else if (static_cast<int>(i) == hover_index_) {
                bg = {0.25f, 0.27f, 0.32f, 1};
            } else if (item.toggled) {
                bg = {0.25f, 0.35f, 0.5f, 1};
            }

            if (bg.a > 0) {
                commands.draw_rect(x, 4, btn_size, btn_size, 4, Paint::solid(bg),
                                   Paint::none(), 0);
            }

            // Icon - light color for dark theme
            Color icon_c = item.enabled ? Color{0.9f, 0.9f, 0.9f, 1} : Color{0.5f, 0.5f, 0.5f, 1};
            draw_inline_text(commands, style, item.icon, x + btn_size/2 - 7,
                             4 + btn_size * 0.65f, 16.0f, false, icon_c);

            // Dropdown arrow
            if (item.type == Item::Dropdown) {
                draw_inline_text(commands, style, "▾", x + btn_size - 10,
                                 4 + btn_size * 0.7f, 10.0f, false, icon_c);
            }

            item_bounds_.push_back({x, 4, btn_size, btn_size});
            x += btn_size + 2;
        }
    }

    void emit_overlay_commands(const Element& elem, RenderCommandList& commands) override {
        if (dropdown_open_ < 0 || dropdown_open_ >= static_cast<int>(items_.size())) return;

        const auto& item = items_[dropdown_open_];
        if (item.dropdown_items.empty()) return;

        auto* style = elem.computed_style;
        const auto& bounds = item_bounds_[dropdown_open_];
        const auto anchor_bounds =
            detail::css_render_rect_bounds(&elem, bounds.x, bounds.y,
                                           bounds.w, bounds.h);
        float x = anchor_bounds.x;
        float y = anchor_bounds.y + anchor_bounds.height + 2;
        float w = dropdown_width(item, style);
        float item_h = 28;
        float h = item.dropdown_items.size() * item_h + 8;
        commands.draw_rect(x + 2, y + 2, w, h, 4,
                           Paint::solid({0, 0, 0, 0.1f}), Paint::none(), 0);
        commands.draw_rect(x, y, w, h, 4, Paint::solid({1, 1, 1, 1}),
                           Paint::solid({0.85f, 0.85f, 0.85f, 1}), 1);

        float iy = y + 4;
        for (size_t i = 0; i < item.dropdown_items.size(); ++i) {
            if (static_cast<int>(i) == dropdown_hover_) {
                commands.draw_rect(x + 4, iy, w - 8, item_h, 4,
                                   Paint::solid({0.94f, 0.94f, 0.94f, 1}),
                                   Paint::none(), 0);
            }
            draw_inline_text(commands, style, item.dropdown_items[i].second, x + 12,
                             iy + item_h * 0.65f, 12.0f, false, {0.1f, 0.1f, 0.1f, 1});
            iy += item_h;
        }

        dropdown_bounds_ = {x, y, w, h};
        dropdown_item_h_ = item_h;
    }

    bool has_overlay() const override { return dropdown_open_ >= 0; }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

        if (event.type == EventType::MouseMove) {
            int new_hover = item_at(lx, ly);
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                elem.mark_paint_dirty();
            }

            // Dropdown hover
            if (dropdown_open_ >= 0 && hit(event.x, event.y, dropdown_bounds_)) {
                dropdown_hover_ = static_cast<int>((event.y - dropdown_bounds_.y - 4) / dropdown_item_h_);
                elem.mark_paint_dirty();
            }
            return dropdown_open_ >= 0;
        }

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            int idx = item_at(lx, ly);
            if (idx >= 0 && items_[idx].enabled) {
                active_index_ = idx;
                
                if (items_[idx].type == Item::Dropdown) {
                    dropdown_open_ = (dropdown_open_ == idx) ? -1 : idx;
                    dropdown_hover_ = -1;
                }
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }

            // Dropdown item click
            if (dropdown_open_ >= 0 && hit(event.x, event.y, dropdown_bounds_)) {
                int di = static_cast<int>((event.y - dropdown_bounds_.y - 4) / dropdown_item_h_);
                if (di >= 0 && di < static_cast<int>(items_[dropdown_open_].dropdown_items.size())) {
                    if (on_dropdown_) {
                        on_dropdown_(items_[dropdown_open_].id, items_[dropdown_open_].dropdown_items[di].first);
                    }
                }
                dropdown_open_ = -1;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }

            // Click outside dropdown
            if (dropdown_open_ >= 0) {
                dropdown_open_ = -1;
                sync_host_semantics();
                elem.mark_paint_dirty();
            }
        }

        if (event.type == EventType::MouseUp && event.button == MouseButton::Left) {
            if (active_index_ >= 0) {
                auto& item = items_[active_index_];
                if (item.type == Item::Button && item.action) {
                    item.action();
                } else if (item.type == Item::Toggle) {
                    item.toggled = !item.toggled;
                    if (on_toggle_) on_toggle_(item.id, item.toggled);
                }
                active_index_ = -1;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }
        }

        return false;
    }

    bool wants_mouse_capture() const override { return dropdown_open_ >= 0; }

    // API
    bool is_toggled(const std::string& id) const {
        for (const auto& item : items_) {
            if (item.id == id) return item.toggled;
        }
        return false;
    }

    void set_toggled(const std::string& id, bool v) {
        for (auto& item : items_) {
            if (item.id == id) {
                item.toggled = v;
                sync_host_semantics();
                break;
            }
        }
    }

    void set_enabled(const std::string& id, bool v) {
        for (auto& item : items_) {
            if (item.id == id) {
                item.enabled = v;
                sync_host_semantics();
                break;
            }
        }
    }

    void set_open_dropdown(const std::string& id) {
        dropdown_open_ = -1;
        dropdown_hover_ = -1;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].type == Item::Dropdown && items_[i].id == id) {
                dropdown_open_ = static_cast<int>(i);
                break;
            }
        }
        sync_host_semantics();
    }

    using ToggleCallback = std::function<void(const std::string& id, bool toggled)>;
    using DropdownCallback = std::function<void(const std::string& toolbar_id, const std::string& item_id)>;
    void on_toggle(ToggleCallback cb) { on_toggle_ = std::move(cb); }
    void on_dropdown_select(DropdownCallback cb) { on_dropdown_ = std::move(cb); }

    const char* type_name() const override { return "ToolbarWidget"; }

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

    static float dropdown_width(const Item& item, const ComputedStyle* style) {
        float width = 120.0f;
        for (const auto& entry : item.dropdown_items) {
            width = std::max(width, text_width(style, entry.second, 12.0f, false) + 24.0f);
        }
        return width;
    }

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    void sync_host_semantics() override {
        set_host_attribute("role", "toolbar");
        set_host_attribute("aria-orientation", "horizontal");
        set_host_attribute("data-state", dropdown_open_ >= 0 ? "open" : "closed");
        if (dropdown_open_ >= 0 && dropdown_open_ < static_cast<int>(items_.size()) &&
            !items_[static_cast<size_t>(dropdown_open_)].id.empty()) {
            set_host_attribute("data-open-id", items_[static_cast<size_t>(dropdown_open_)].id);
        } else {
            clear_host_attribute("data-open-id");
        }
    }

    int item_at(float lx, float ly) const {
        for (size_t i = 0; i < item_bounds_.size(); ++i) {
            if (hit(lx, ly, item_bounds_[i]) && items_[i].type != Item::Separator) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::vector<Item> items_;
    std::vector<Rect> item_bounds_;
    int hover_index_ = -1;
    int active_index_ = -1;
    int dropdown_open_ = -1;
    int dropdown_hover_ = -1;
    Rect dropdown_bounds_{};
    float dropdown_item_h_ = 28;

    ToggleCallback on_toggle_;
    DropdownCallback on_dropdown_;
};

} // namespace flexUI

#endif
