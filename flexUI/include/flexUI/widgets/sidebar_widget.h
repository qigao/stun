/*
 * flexUI - SidebarWidget
 *
 * Collapsible navigation sidebar with sections and items.
 */

#ifndef FLEXUI_SIDEBAR_WIDGET_H
#define FLEXUI_SIDEBAR_WIDGET_H

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

class SidebarWidget : public Widget {
public:
    struct Item {
        std::string id;
        std::string icon;
        std::string label;
        std::string badge;
        bool enabled = true;
    };

    struct Section {
        std::string title;
        std::vector<Item> items;
        bool collapsed = false;
    };

    SidebarWidget(bool collapsible = true) : collapsible_(collapsible) {}

    void add_section(const std::string& title) {
        sections_.push_back({title, {}, false});
        sync_host_semantics();
    }

    void add_item(const std::string& id, const std::string& icon, const std::string& label,
                  const std::string& badge = "") {
        if (sections_.empty()) add_section("");
        sections_.back().items.push_back({id, icon, label, badge, true});
        sync_host_semantics();
    }

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = collapsed_ ? 56 : elem.width();
        float h = elem.height();

        // Background
        commands.draw_rect(0, 0, w, h, 0, Paint::solid({0.12f, 0.14f, 0.18f, 1}), Paint::none(), 0);

        float y = 8;
        float item_h = 40;
        item_bounds_.clear();

        for (size_t si = 0; si < sections_.size(); ++si) {
            auto& section = sections_[si];

            // Section header
            if (!section.title.empty() && !collapsed_) {
                draw_inline_text(commands, style, section.title, 16, y + 14, 11.0f, true,
                                 {0.5f, 0.5f, 0.55f, 1});
                y += 24;
            }

            // Items
            for (size_t ii = 0; ii < section.items.size(); ++ii) {
                const auto& item = section.items[ii];
                bool is_selected = (item.id == selected_id_);
                bool is_hover = (hover_section_ == static_cast<int>(si) && hover_item_ == static_cast<int>(ii));

                // Background
                if (is_selected) {
                    commands.draw_rect(4, y, w - 8, item_h, 6, Paint::solid({0.25f, 0.45f, 0.85f, 1}), Paint::none(), 0);
                } else if (is_hover && item.enabled) {
                    commands.draw_rect(4, y, w - 8, item_h, 6, Paint::solid({0.2f, 0.22f, 0.28f, 1}), Paint::none(), 0);
                }

                // Icon
                Color icon_c = is_selected ? Color{1, 1, 1, 1} : 
                              item.enabled ? Color{0.7f, 0.7f, 0.75f, 1} : Color{0.4f, 0.4f, 0.45f, 1};
                float icon_x = collapsed_ ? w/2 - 8 : 16;
                draw_inline_text(commands, style, item.icon, icon_x, y + item_h * 0.6f, 18.0f,
                                 false, icon_c);

                // Label (if not collapsed)
                if (!collapsed_) {
                    Color text_c = is_selected ? Color{1, 1, 1, 1} : 
                                  item.enabled ? Color{0.85f, 0.85f, 0.88f, 1} : Color{0.5f, 0.5f, 0.55f, 1};
                    draw_inline_text(commands, style, item.label, 44, y + item_h * 0.6f, 13.0f,
                                     false, text_c);

                    // Badge
                    if (!item.badge.empty()) {
                        float badge_x = w - 32;
                        commands.draw_rect(badge_x, y + 10, 20, 20, 10, Paint::solid({0.9f, 0.3f, 0.3f, 1}), Paint::none(), 0);
                        draw_inline_text(commands, style, item.badge, badge_x + 6, y + 24, 10.0f,
                                         true, {1, 1, 1, 1});
                    }
                }

                item_bounds_.push_back({static_cast<int>(si), static_cast<int>(ii), 4, y, w - 8, item_h});
                y += item_h + 2;
            }

            y += 8;
        }

        // Collapse toggle
        if (collapsible_) {
            float toggle_y = h - 44;
            Color toggle_c = hover_toggle_ ? Color{0.3f, 0.32f, 0.38f, 1} : Color{0.18f, 0.2f, 0.24f, 1};
            commands.draw_rect(4, toggle_y, w - 8, 36, 6, Paint::solid(toggle_c), Paint::none(), 0);
            
            const char* icon = collapsed_ ? "»" : "«";
            float ix = collapsed_ ? w/2 - 6 : w - 24;
            draw_inline_text(commands, style, icon, ix, toggle_y + 24, 16.0f, false,
                             {0.6f, 0.6f, 0.65f, 1});
            
            toggle_bounds_ = {4, toggle_y, w - 8, 36};
        }
        actual_width_ = w;
    }

    bool handle_event(const Event& event, Element& elem) override {
        const flex::Vec2 local_pos =
            detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
        float lx = local_pos.x;
        float ly = local_pos.y;

        if (event.type == EventType::MouseMove) {
            hover_section_ = hover_item_ = -1;
            hover_toggle_ = false;

            for (const auto& b : item_bounds_) {
                if (lx >= b.x && lx < b.x + b.w && ly >= b.y && ly < b.y + b.h) {
                    hover_section_ = b.section;
                    hover_item_ = b.item;
                    break;
                }
            }

            if (collapsible_ && lx >= toggle_bounds_.x && lx < toggle_bounds_.x + toggle_bounds_.w &&
                ly >= toggle_bounds_.y && ly < toggle_bounds_.y + toggle_bounds_.h) {
                hover_toggle_ = true;
            }

            elem.mark_paint_dirty();
            return false;
        }

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Toggle collapse
            if (hover_toggle_) {
                collapsed_ = !collapsed_;
                sync_host_semantics();
                elem.mark_paint_dirty();
                return true;
            }

            // Item click
            if (hover_section_ >= 0 && hover_item_ >= 0) {
                const auto& item = sections_[hover_section_].items[hover_item_];
                if (item.enabled) {
                    selected_id_ = item.id;
                    sync_host_semantics();
                    if (on_select_) on_select_(item.id);
                    elem.mark_paint_dirty();
                    return true;
                }
            }
        }

        return false;
    }

    // API
    void select(const std::string& id) {
        selected_id_ = id;
        sync_host_semantics();
    }
    const std::string& selected() const { return selected_id_; }
    
    void set_collapsed(bool v) {
        collapsed_ = v;
        sync_host_semantics();
    }
    bool is_collapsed() const { return collapsed_; }
    
    float actual_width() const { return actual_width_; }

    void sync_host_semantics_for_layout(Element& elem) override {
        (void)elem;
        sync_host_semantics();
    }

    using SelectCallback = std::function<void(const std::string& id)>;
    void on_select(SelectCallback cb) { on_select_ = std::move(cb); }

    bool measure_intrinsic_size(const Element& elem, float available_width,
                                float available_height, float& out_width,
                                float& out_height) const override {
        (void)elem;
        (void)available_width;
        (void)available_height;
        out_width = collapsed_ ? 56.0f : 240.0f;

        float total_h = 8.0f;
        for (const auto& section : sections_) {
            if (!section.title.empty() && !collapsed_) {
                total_h += 24.0f;
            }
            total_h += static_cast<float>(section.items.size()) * 42.0f;
            total_h += 8.0f;
        }
        if (collapsible_) {
            total_h += 44.0f;
        }
        out_height = std::max(total_h, 180.0f);
        return true;
    }

    const char* type_name() const override { return "SidebarWidget"; }

private:
    struct ItemBounds { int section, item; float x, y, w, h; };
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

    static float draw_inline_text(RenderCommandList& commands, const ComputedStyle* base_style,
                                  const std::string& text, float x, float baseline_y,
                                  float font_size, bool bold, const Color& color) {
        const auto text_style = make_text_style(base_style, font_size, bold);
        return emit_segmented_text_line(commands, &text_style, text, x,
                                                       baseline_y, color, bold);
    }

    void sync_host_semantics() override {
        set_host_attribute("role", "navigation");
        set_host_attribute("aria-orientation", "vertical");
        set_host_attribute("data-state", collapsed_ ? "collapsed" : "expanded");
        set_host_boolean_attribute("data-collapsible", collapsible_);

        size_t item_count = 0;
        for (const auto& section : sections_) {
            item_count += section.items.size();
        }
        set_host_attribute("data-item-count", std::to_string(item_count));
        if (!selected_id_.empty()) {
            set_host_attribute("data-selected-id", selected_id_);
        } else {
            clear_host_attribute("data-selected-id");
        }
    }

    std::vector<Section> sections_;
    std::vector<ItemBounds> item_bounds_;
    Rect toggle_bounds_{};
    
    std::string selected_id_;
    int hover_section_ = -1;
    int hover_item_ = -1;
    bool hover_toggle_ = false;
    
    bool collapsible_;
    bool collapsed_ = false;
    float actual_width_ = 0;

    SelectCallback on_select_;
};

} // namespace flexUI

#endif
