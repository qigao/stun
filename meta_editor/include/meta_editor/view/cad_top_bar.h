#pragma once

#include "meta_editor/view/panel.h"
#include <string>
#include <vector>

namespace meta_editor {
class IconSystem;

class CADTopBar : public Panel {
public:
    CADTopBar();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    void set_layout(float x, float y, float w);
    void set_icons(IconSystem* icons) { icons_ = icons; }

protected:
    float content_height() const override { return 72.0f; } // Two rows

private:
    struct ButtonGroup {
        std::string name;
        std::vector<std::string> icons;
        float x_start;
        float x_end;
    };

    struct Property {
        std::string label;
        std::string value;
        float x, w;
    };

    void render_row1(flex::Renderer& r);
    void render_row2(flex::Renderer& r);

    std::vector<ButtonGroup> groups_;
    std::vector<Property> properties_;

    float row1_h_ = 36.0f;
    float row2_h_ = 36.0f;
    float icon_size_ = 22.0f;
    IconSystem* icons_ = nullptr;
};

} // namespace meta_editor
