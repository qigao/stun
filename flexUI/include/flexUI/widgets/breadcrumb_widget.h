/*
 * flexUI - BreadcrumbWidget
 *
 * Breadcrumb navigation using Group/Shape composition system.
 */

#ifndef FLEXUI_BREADCRUMB_WIDGET_H
#define FLEXUI_BREADCRUMB_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>
#include <vector>
#include <string>

namespace flexUI {

class RenderCommandList;

/**
 * BreadcrumbWidget - Navigation breadcrumb trail
 *
 * Structure:
 *   Group (root)
 *   ├── TextShape (item 1)
 *   ├── TextShape (separator)
 *   ├── TextShape (item 2)
 *   ├── TextShape (separator)
 *   └── TextShape (item N, current)
 *
 * CSS variables:
 *   --breadcrumb-color: "r,g,b,a"
 *   --breadcrumb-color-active: "r,g,b,a"
 *   --breadcrumb-color-hover: "r,g,b,a"
 *   --breadcrumb-separator: "/" or ">" etc
 *   --breadcrumb-gap: "8"
 */
class BreadcrumbWidget : public Widget {
public:
    struct Item {
        std::string id;
        std::string label;
    };

    BreadcrumbWidget();

    // Widget interface
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "BreadcrumbWidget"; }

    // Items
    void set_items(const std::vector<Item>& items);
    const std::vector<Item>& items() const { return items_; }

    // Separator
    const std::string& separator() const { return separator_; }
    void set_separator(const std::string& sep) { separator_ = sep; dirty_ = true; }

    // Callback
    using ClickCallback = std::function<void(int index, const std::string& id)>;
    void set_click_callback(ClickCallback callback) { click_callback_ = std::move(callback); }

private:
    void sync_host_semantics() override;
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    int hit_test(float x, float y, const Element& elem);

    // Visual composition
    Group root_;
    std::vector<TextShape*> item_texts_;
    std::vector<TextShape*> separator_texts_;
    std::vector<float> item_x_positions_;
    std::vector<float> item_widths_;

    // State
    std::vector<Item> items_;
    std::string separator_ = "/";
    int hovered_index_ = -1;

    // Cached
    float cached_width_ = 0;

    ClickCallback click_callback_;
};

} // namespace flexUI

#endif // FLEXUI_BREADCRUMB_WIDGET_H
