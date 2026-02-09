/*
 * Meta Editor - Top Bar (Menu & Toolbar)
 */

#pragma once

#include "meta_editor/view/panel.h"
#include "meta_editor/core/editor.h"
#include <string>
#include <vector>
#include <functional>

namespace meta_editor {

class TopBar : public Panel {
public:
    TopBar(Editor* editor);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

    // Set available width to stretch across screen
    void set_layout(float x, float y, float w);

    // Callbacks for file operations
    std::function<void()> on_new;
    std::function<void()> on_open;
    std::function<void(const std::string&)> on_save; // Takes current path

protected:
    float content_height() const override { return 40.0f; }

private:
    struct Button {
        std::string label;
        std::string icon;
        float x, width;
    };
    
    void draw_icon(flex::Renderer& r, const std::string& name, float cx, float cy, float size);
    
    Editor* editor_;
    std::vector<Button> buttons_;
    float gap_ = 8.0f;
    float padding_ = 6.0f;
    float btn_height_ = 28.0f;
};

} // namespace meta_editor
