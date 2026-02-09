/*
 * Meta Editor - Vertical Tool Panel
 * 
 * Simple approach: get_button_bounds() is the single source of truth
 * for both rendering and hit testing.
 */

#pragma once

#include "panel.h"
#include <flex.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace meta_editor {

class ToolManager;

class ToolPanel : public Panel {
public:
    explicit ToolPanel(ToolManager* tools);
    ~ToolPanel() override = default;

    void render(flex::Renderer& renderer) override;
    
    // Override to handle layout updates
    void update_layout();

protected:
    float content_height() const override;
    bool handle_click(float screen_x, float screen_y) override;

private:
    struct ToolDef {
        std::string name;
        std::string shortcut;
        std::string icon_path; // SVG path data
    };

    void rebuild_layout();
    void update_button_states();
    
    // Flex Layout Hierarchy
    // We use a small standalone flex::Instance for the panel UI
    std::shared_ptr<flex::Instance> ui_instance_;
    flex::Group* root_group_ = nullptr;
    
    // Map tool name to its UI group (for state updates)
    std::unordered_map<std::string, flex::Group*> tool_buttons_;

    ToolManager* tools_;
    std::vector<ToolDef> tool_defs_;

    float padding_ = 6.0f;
    float button_size_ = 36.0f;
    float gap_ = 4.0f;
};

} // namespace meta_editor
