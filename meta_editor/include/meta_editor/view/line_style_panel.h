/*
 * Meta Editor - Line Style Panel
 *
 * Adjust stroke color, width, and dash pattern for selected lines.
 */

#pragma once

#include "panel.h"
#include <functional>

namespace meta_editor {

class SelectionManager;
class LineTool;

class LineStylePanel : public Panel {
public:
    LineStylePanel(SelectionManager* selection, LineTool* line_tool);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    using StyleChangeCallback = std::function<void()>;
    void set_style_change_callback(StyleChangeCallback cb) { on_change_ = std::move(cb); }

protected:
    float content_height() const override;

private:
    void apply_stroke_to_selection();

    SelectionManager* selection_;
    LineTool* line_tool_;

    float stroke_width_ = 2.0f;
    int color_index_ = 0;
    StyleChangeCallback on_change_;

    static constexpr int COLOR_COUNT = 6;
    static constexpr float PADDING = 12.0f;
    static constexpr float ROW_HEIGHT = 28.0f;
};

} // namespace meta_editor
