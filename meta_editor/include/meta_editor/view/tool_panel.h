/*
 * Meta Editor - Vertical Tool Panel
 */

#pragma once

#include "panel.h"
#include <string>
#include <vector>

namespace meta_editor {

class ToolManager;

struct ToolDef {
    std::string name;
    std::string icon;      // Unused now - we draw geometric icons
    std::string shortcut;  // Keyboard shortcut hint
};

class ToolPanel : public Panel {
public:
    explicit ToolPanel(ToolManager* tools);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y);

protected:
    float content_height() const override;

private:
    void draw_tool_icon(flex::Renderer& renderer, const std::string& name,
                        float cx, float cy, float size, const flex::Color& color);

    ToolManager* tools_;

    float button_size_ = 36;
    float padding_ = 4;
    float gap_ = 2;

    std::vector<ToolDef> tool_defs_;
    int hover_index_ = -1;
};

} // namespace meta_editor
