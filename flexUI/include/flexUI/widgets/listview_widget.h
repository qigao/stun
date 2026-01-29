/*
 * flexUI - ListViewWidget
 *
 * Virtual scrolling list - only renders visible items.
 * Handles thousands of items efficiently.
 * 
 * CSS Variables:
 *   --listview-item-height: 40px
 *   --listview-bg: #ffffff
 *   --listview-item-hover: #f5f5f5
 *   --listview-item-selected: #e3f2fd
 *   --listview-scrollbar-width: 8px
 */

#ifndef FLEXUI_LISTVIEW_WIDGET_H
#define FLEXUI_LISTVIEW_WIDGET_H

#include "../widget.h"
#include "../element.h"
#include "../event.h"
#include "../renderer.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

namespace flexUI {

class ListViewWidget : public Widget {
public:
    struct Item {
        std::string id;
        std::string text;
        std::string secondary;
        bool selected = false;
    };

    using RenderItemFn = std::function<void(const Item&, float x, float y, float w, float h, 
                                            bool hover, bool selected, Renderer&)>;

    ListViewWidget() = default;

    void set_items(std::vector<Item> items) { items_ = std::move(items); }
    std::vector<Item>& items() { return items_; }
    const std::vector<Item>& items() const { return items_; }

    void add_item(const std::string& id, const std::string& text, const std::string& secondary = "") {
        items_.push_back({id, text, secondary, false});
    }

    void clear() { items_.clear(); scroll_y_ = 0; }

    void render(const Element& elem, Renderer& renderer) override {
        auto* style = elem.computed_style;
        if (!style) return;

        float w = elem.width();
        float h = elem.height();
        item_h_ = style->get_variable_float("--listview-item-height", 40.0f);
        bar_w_ = style->get_variable_float("--listview-scrollbar-width", 8.0f);

        Color bg = style->get_variable_color("--listview-bg", {0.15f, 0.17f, 0.2f, 1});
        Color hover_bg = style->get_variable_color("--listview-item-hover", {0.2f, 0.22f, 0.26f, 1});
        Color sel_bg = style->get_variable_color("--listview-item-selected", {0.25f, 0.35f, 0.5f, 1});
        Color bar_bg = style->get_variable_color("--scrollbar-bg", {1, 1, 1, 0.1f});
        Color thumb_c = style->get_variable_color("--scrollbar-thumb", {1, 1, 1, 0.3f});

        // Background
        renderer.draw_rect(0, 0, w, h, 0, Paint::solid(bg), Paint::none(), 0);

        // Calculate scroll
        float content_h = items_.size() * item_h_;
        float view_w = content_h > h ? w - bar_w_ : w;
        max_scroll_ = std::max(0.0f, content_h - h);
        scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll_);

        // Visible range
        int first = static_cast<int>(scroll_y_ / item_h_);
        int last = static_cast<int>((scroll_y_ + h) / item_h_) + 1;
        first = std::max(0, first);
        last = std::min(static_cast<int>(items_.size()), last);

        // Clip content area
        renderer.flex().save();
        renderer.flex().clip_rect(elem.absolute_x(), elem.absolute_y(), view_w, h);

        // Render visible items
        for (int i = first; i < last; ++i) {
            float y = i * item_h_ - scroll_y_;
            const auto& item = items_[i];
            
            // Background
            if (item.selected) {
                renderer.draw_rect(0, y, view_w, item_h_, 0, Paint::solid(sel_bg), Paint::none(), 0);
            } else if (i == hover_index_) {
                renderer.draw_rect(0, y, view_w, item_h_, 0, Paint::solid(hover_bg), Paint::none(), 0);
            }

            // Custom render or default
            if (render_item_) {
                render_item_(item, 0, y, view_w, item_h_, i == hover_index_, item.selected, renderer);
            } else {
                render_default_item(item, y, view_w, style, renderer);
            }
        }

        renderer.flex().restore();

        // Scrollbar
        if (content_h > h) {
            float track_x = w - bar_w_;
            renderer.draw_rect(track_x, 0, bar_w_, h, bar_w_ / 2, Paint::solid(bar_bg), Paint::none(), 0);

            float thumb_h = std::max(20.0f, h * h / content_h);
            float thumb_y = max_scroll_ > 0 ? (h - thumb_h) * scroll_y_ / max_scroll_ : 0;
            renderer.draw_rect(track_x, thumb_y, bar_w_, thumb_h, bar_w_ / 2, Paint::solid(thumb_c), Paint::none(), 0);

            thumb_rect_ = {track_x, thumb_y, bar_w_, thumb_h};
            track_rect_ = {track_x, 0, bar_w_, h};
        }

        view_width_ = view_w;
        view_height_ = h;
    }

    bool handle_event(const Event& event, Element& elem) override {
        float lx = event.x - elem.absolute_x();
        float ly = event.y - elem.absolute_y();

        if (event.type == EventType::MouseWheel) {
            scroll_y_ = std::clamp(scroll_y_ - event.delta_y * item_h_, 0.0f, max_scroll_);
            elem.mark_paint_dirty();
            return true;
        }

        if (event.type == EventType::MouseDown && event.button == MouseButton::Left) {
            // Scrollbar
            if (lx >= track_rect_.x) {
                if (hit(lx, ly, thumb_rect_)) {
                    dragging_ = true;
                    drag_start_ = ly;
                    drag_scroll_ = scroll_y_;
                } else {
                    scroll_y_ = std::clamp(ly / view_height_ * max_scroll_, 0.0f, max_scroll_);
                }
                elem.mark_paint_dirty();
                return true;
            }

            // Item click
            int idx = item_at(ly);
            if (idx >= 0) {
                if (multi_select_) {
                    items_[idx].selected = !items_[idx].selected;
                } else {
                    for (auto& item : items_) item.selected = false;
                    items_[idx].selected = true;
                }
                if (on_select_) on_select_(idx, items_[idx]);
                elem.mark_paint_dirty();
                return true;
            }
        }

        if (event.type == EventType::MouseMove) {
            if (dragging_) {
                float range = view_height_ - thumb_rect_.h;
                if (range > 0) {
                    scroll_y_ = std::clamp(drag_scroll_ + (ly - drag_start_) / range * max_scroll_, 0.0f, max_scroll_);
                    elem.mark_paint_dirty();
                }
                return true;
            }

            int new_hover = lx < track_rect_.x ? item_at(ly) : -1;
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                elem.mark_paint_dirty();
            }
        }

        if (event.type == EventType::MouseUp && dragging_) {
            dragging_ = false;
            return true;
        }

        return false;
    }

    bool wants_mouse_capture() const override { return dragging_; }

    // API
    void set_multi_select(bool v) { multi_select_ = v; }
    void set_render_item(RenderItemFn fn) { render_item_ = std::move(fn); }

    int selected_index() const {
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].selected) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<int> selected_indices() const {
        std::vector<int> result;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].selected) result.push_back(static_cast<int>(i));
        }
        return result;
    }

    void scroll_to_item(int index) {
        if (index < 0 || index >= static_cast<int>(items_.size())) return;
        float item_y = index * item_h_;
        if (item_y < scroll_y_) scroll_y_ = item_y;
        else if (item_y + item_h_ > scroll_y_ + view_height_) scroll_y_ = item_y + item_h_ - view_height_;
        scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll_);
    }

    using SelectCallback = std::function<void(int index, const Item& item)>;
    void on_select(SelectCallback cb) { on_select_ = std::move(cb); }

    const char* type_name() const override { return "ListViewWidget"; }

private:
    struct Rect { float x, y, w, h; };

    bool hit(float px, float py, const Rect& r) const {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    }

    int item_at(float ly) const {
        float y = ly + scroll_y_;
        int idx = static_cast<int>(y / item_h_);
        return (idx >= 0 && idx < static_cast<int>(items_.size())) ? idx : -1;
    }

    void render_default_item(const Item& item, float y, float w, const ComputedStyle* style, Renderer& renderer) {
        Color text_c{0.9f, 0.9f, 0.9f, 1};
        Color sec_c{0.6f, 0.6f, 0.6f, 1};

        if (item.secondary.empty()) {
            renderer.draw_text(item.text, 12, y + item_h_ * 0.6f, style->font_family, 14, false, text_c);
        } else {
            renderer.draw_text(item.text, 12, y + item_h_ * 0.4f, style->font_family, 14, false, text_c);
            renderer.draw_text(item.secondary, 12, y + item_h_ * 0.75f, style->font_family, 11, false, sec_c);
        }
    }

    std::vector<Item> items_;
    float scroll_y_ = 0;
    float max_scroll_ = 0;
    float item_h_ = 40;
    float bar_w_ = 8;
    float view_width_ = 0;
    float view_height_ = 0;
    int hover_index_ = -1;
    bool multi_select_ = false;
    bool dragging_ = false;
    float drag_start_ = 0;
    float drag_scroll_ = 0;
    Rect thumb_rect_{}, track_rect_{};
    RenderItemFn render_item_;
    SelectCallback on_select_;
};

} // namespace flexUI

#endif
