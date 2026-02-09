#pragma once

#include "meta_editor/view/panel.h"
#include <string>
#include <vector>

namespace meta_editor {
class IconSystem;

class CADBottomBar : public Panel {
public:
    CADBottomBar();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    void set_layout(float x, float y, float w);
    void set_icons(IconSystem* icons) { icons_ = icons; }

protected:
    float content_height() const override { return 36.0f; }

private:
    void render_coords(flex::Renderer& r);
    void render_snap_tools(flex::Renderer& r);
    void render_pagination(flex::Renderer& r);

    struct SnapTool {
        std::string icon;
        bool active;
        float x;
    };

    std::vector<SnapTool> snap_tools_;
    IconSystem* icons_ = nullptr;
};

} // namespace meta_editor
