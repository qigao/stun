/*
 * flexUI - PaginationWidget
 *
 * Pagination navigation using Group/Shape composition system.
 */

#ifndef FLEXUI_PAGINATION_WIDGET_H
#define FLEXUI_PAGINATION_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>
#include <vector>

namespace flexUI {

/**
 * PaginationWidget - Page navigation control
 *
 * Structure:
 *   Group (root)
 *   ├── RectShape + TextShape (prev button)
 *   ├── RectShape + TextShape (page 1)
 *   ├── TextShape (ellipsis)
 *   ├── RectShape + TextShape (page N)
 *   └── RectShape + TextShape (next button)
 *
 * CSS variables:
 *   --pagination-bg: "r,g,b,a"
 *   --pagination-bg-active: "r,g,b,a"
 *   --pagination-bg-hover: "r,g,b,a"
 *   --pagination-text: "r,g,b,a"
 *   --pagination-text-active: "r,g,b,a"
 *   --pagination-border: "r,g,b,a"
 *   --pagination-size: "32"
 *   --pagination-gap: "4"
 *   --pagination-radius: "4"
 */
class PaginationWidget : public Widget {
public:
    PaginationWidget(int total_pages = 1, int current_page = 1);

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "PaginationWidget"; }

    // Properties
    int total_pages() const { return total_pages_; }
    void set_total_pages(int pages);

    int current_page() const { return current_page_; }
    void set_current_page(int page);

    int visible_pages() const { return visible_pages_; }
    void set_visible_pages(int count) { visible_pages_ = count; dirty_ = true; }

    bool show_prev_next() const { return show_prev_next_; }
    void set_show_prev_next(bool show) { show_prev_next_ = show; dirty_ = true; }

    // Callback
    using PageChangeCallback = std::function<void(int page)>;
    void set_page_change_callback(PageChangeCallback callback) { page_change_callback_ = std::move(callback); }

private:
    void sync_host_semantics() override;
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    int hit_test(float x, float y, const Element& elem);
    std::vector<int> get_visible_page_numbers() const;

    // Visual composition
    Group root_;
    RectShape* prev_bg_ = nullptr;
    TextShape* prev_text_ = nullptr;
    RectShape* next_bg_ = nullptr;
    TextShape* next_text_ = nullptr;
    std::vector<RectShape*> page_backgrounds_;
    std::vector<TextShape*> page_texts_;
    std::vector<int> page_numbers_; // -1 for ellipsis

    // State
    int total_pages_ = 1;
    int current_page_ = 1;
    int visible_pages_ = 5;
    bool show_prev_next_ = true;
    int hovered_index_ = -1; // -2 = prev, -1 = none, 0+ = page index, INT_MAX = next

    // Cached
    float cached_width_ = 0;
    float cached_height_ = 0;

    PageChangeCallback page_change_callback_;
};

} // namespace flexUI

#endif // FLEXUI_PAGINATION_WIDGET_H
