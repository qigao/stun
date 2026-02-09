/*
 * Meta Editor - Bottom Bar (Status & Controls)
 */

#pragma once

#include "meta_editor/view/panel.h"
#include "meta_editor/core/editor.h"

namespace meta_editor {

class BottomBar : public Panel {
public:
    BottomBar(Editor* editor);
    
    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;
    
    // Set position and fill width
    void set_layout(float x, float y, float w);
    
    // Updates internal state from editor if needed (most is pulled directly)
    void update();

protected:
    float content_height() const override { return 30.0f; }

private:
    Editor* editor_;
    float snap_btn_x_ = 0;
    float snap_btn_w_ = 80;
    float grid_btn_x_ = 0;
    float grid_btn_w_ = 80;
};

} // namespace meta_editor
