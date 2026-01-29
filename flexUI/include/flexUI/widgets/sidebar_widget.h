/*
 * flexUI - SidebarWidget
 *
 * Collapsible navigation sidebar with sections and items.
 */

#ifndef FLEXUI_SIDEBAR_WIDGET_H
#define FLEXUI_SIDEBAR_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
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
    }

    void add_item(const std::string& id, const std::string& icon, const std::string& label,
                  const std::string& badge = "") {
        if (sections_.empty()) add_section("");
        sections_.back().items.push_back({id, icon, label, badge, true});
    }

    void render(const Element& elem, Renderer& renderer) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = collapsed_ ? 56 : elem.width();
        float h = elem.height();

        // Background
        renderer.draw_rect(0, 0, w, h, 0, Paint::solid({0.12f, 0.14f, 0.18f, 1}), Paint::none(), 0);

        float y = 8;
        float item_h = 40;
        item_bounds_.clear();

        for (size_t si = 0; si < sections_.size(); ++si) {
            auto& section = sections_[si];

            // Section header
            if (!section.title.empty() && !collapsed_) {
                renderer.draw_text(section.title, 16, y + 14, style->font_family, 11, true, {0.5f, 0.5f, 0.55f, 1});
                y += 24;
            }

            // Items
            for (size_t ii = 0; ii < section.items.size(); ++ii) {
                const auto& item = section.items[ii];
                bool is_selected = (item.id == selected_id_);
                bool is_hover = (hover_section_ == static_cast<int>(si) && hover_item_ == static_cast<int>(ii));

                // Background
                if (is_selected) {
                    renderer.draw_rect(4, y, w - 8, item_h, 6, Paint::solid({0.25f, 0.45f, 0.85f, 1}), Paint::none(), 0);
                } else if (is_hover && item.enabled) {
                    renderer.draw_rect(4, y, w - 8, item_h, 6, Paint::solid({0.2f, 0.22f, 0.28f, 1}), Paint::none(), 0);
                }

                // Icon
                Color icon_c = is_selected ? Color{1, 1, 1, 1} : 
                              item.enabled ? Color{0.7f, 0.7f, 0.75f, 1} : Color{0.4f, 0.4f, 0.45f, 1};
                float icon_x = collapsed_ ? w/2 - 8 : 16;
                renderer.draw_text(item.icon, icon_x, y + item_h * 0.6f, style->font_family, 18, false, icon_c);

                // Label (if not collapsed)
                if (!collapsed_) {
                    Color text_c = is_selected ? Color{1, 1, 1, 1} : 
                                  item.enabled ? Color{0.85f, 0.85f, 0.88f, 1} : Color{0.5f, 0.5f, 0.55f, 1};
                    renderer.draw_text(item.label, 44, y + item_h * 0.6f, style->font_family, 13, false, text_c);

                    // Badge
                    if (!item.badge.empty()) {
                        float badge_x = w - 32;
                        renderer.draw_rect(badge_x, y + 10, 20, 20, 10, Paint::solid({0.9f, 0.3f, 0.3f, 1}), Paint::none(), 0);
                        renderer.draw_text(item.badge, badge_x + 6, y + 24, style->font_family, 10, true, {1, 1, 1, 1});
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
            renderer.draw_rect(4, toggle_y, w - 8, 36, 6, Paint::solid(toggle_c), Paint::none(), 0);
            
            const char* icon = collapsed_ ? "»" : "«";
            float ix = collapsed_ ? w/2 - 6 : w - 24;
            renderer.draw_text(icon, ix, toggle_y + 24, style->font_family, 16, false, {0.6f, 0.6f, 0.65f, 1});
            
            toggle_bounds_ = {4, toggle_y, w - 8, 36};
        }

        actual_width_ = w;
    }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();

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
                elem.mark_paint_dirty();
                return true;
            }

            // Item click
            if (hover_section_ >= 0 && hover_item_ >= 0) {
                const auto& item = sections_[hover_section_].items[hover_item_];
                if (item.enabled) {
                    selected_id_ = item.id;
                    if (on_select_) on_select_(item.id);
                    elem.mark_paint_dirty();
                    return true;
                }
            }
        }

        return false;
    }

    // API
    void select(const std::string& id) { selected_id_ = id; }
    const std::string& selected() const { return selected_id_; }
    
    void set_collapsed(bool v) { collapsed_ = v; }
    bool is_collapsed() const { return collapsed_; }
    
    float actual_width() const { return actual_width_; }

    using SelectCallback = std::function<void(const std::string& id)>;
    void on_select(SelectCallback cb) { on_select_ = std::move(cb); }

    const char* type_name() const override { return "SidebarWidget"; }

private:
    struct ItemBounds { int section, item; float x, y, w, h; };
    struct Rect { float x, y, w, h; };

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
