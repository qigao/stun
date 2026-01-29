/*
 * flexUI Designer - Widget Tree Panel
 *
 * Shows hierarchical list of all widgets for easy selection.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <functional>

namespace flexui_designer {

class WidgetTree : public meta_editor::Panel {
public:
    WidgetTree();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;

    void set_widgets(const std::vector<DesignWidget>* widgets) { widgets_ = widgets; }
    void set_selected_id(const std::string* id) { selected_id_ = id; }

    using SelectCallback = std::function<void(const std::string& id)>;
    void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }

private:
    const std::vector<DesignWidget>* widgets_ = nullptr;
    const std::string* selected_id_ = nullptr;
    SelectCallback on_select_;
    float scroll_y_ = 0;
    int hovered_index_ = -1;

    static constexpr float ITEM_HEIGHT = 24.0f;
    static constexpr float PADDING = 8.0f;
    
    const char* widget_type_name(WidgetType type) const;
    const char* widget_type_icon(WidgetType type) const;
};

} // namespace flexui_designer
