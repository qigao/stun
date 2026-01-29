/*
 * flexUI Designer - Widget Palette
 *
 * Sidebar showing available widgets for drag & drop.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <functional>

namespace flexui_designer {

class WidgetPalette : public meta_editor::Panel {
public:
    WidgetPalette();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    // Drag state
    bool is_dragging() const { return dragging_; }
    WidgetType drag_type() const { return drag_type_; }
    void start_drag(WidgetType type, float x, float y);
    void update_drag(float x, float y);
    void end_drag(float x, float y);
    float drag_x() const { return drag_x_; }
    float drag_y() const { return drag_y_; }

    using DropCallback = std::function<void(WidgetType type, float x, float y)>;
    void set_drop_callback(DropCallback cb) { on_drop_ = std::move(cb); }

    void render_drag_preview(flex::Renderer& renderer);

private:
    struct PaletteItem {
        const char* name;
        WidgetType type;
        float y;
    };

    std::vector<PaletteItem> items_;
    bool dragging_ = false;
    WidgetType drag_type_;
    float drag_x_ = 0, drag_y_ = 0;
    int hover_index_ = -1;
    DropCallback on_drop_;
};

} // namespace flexui_designer
