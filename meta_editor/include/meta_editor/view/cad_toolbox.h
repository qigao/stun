#pragma once

#include "meta_editor/view/panel.h"
#include <string>
#include <vector>
#include <map>

namespace meta_editor {

class IconSystem;

class CADToolbox : public Panel {
public:
    CADToolbox();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    void set_icons(IconSystem* icons) { icons_ = icons; }

    struct Tool {
        std::string name;
        std::string icon;
        std::string shortcut;
    };

    void add_tool(const std::string& tab, const Tool& tool);
    void set_active_tab(const std::string& tab);

protected:
    float content_height() const override { return height_; }

private:
    void render_tab_bar(flex::Renderer& r);
    void render_grid(flex::Renderer& r);
    void render_selection_mode(flex::Renderer& r);

    std::string active_tab_ = "Place";
    std::vector<std::string> tabs_ = {"Place", "Modify", "Dimensions"};
    std::map<std::string, std::vector<Tool>> tool_map_;

    float tab_height_ = 32;
    float row_height_ = 45;
    float icon_size_ = 24.0f;
    int columns_ = 4;
    float padding_ = 8;

    IconSystem* icons_ = nullptr;
};

} // namespace meta_editor
