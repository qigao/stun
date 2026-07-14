/*
 * Meta Editor - Hand Tool Implementation
 */

#include "meta_editor/tools/hand_tool.h"
#include "meta_editor/canvas.h"

namespace meta_editor {

bool HandTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_panning_ = true;
    last_screen_pos_ = screen_pos;
    return true;
}

bool HandTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_panning_) return false;
    
    float dx = screen_pos.x - last_screen_pos_.x;
    float dy = screen_pos.y - last_screen_pos_.y;
    
    canvas_->pan(dx, dy);
    last_screen_pos_ = screen_pos;
    return true;
}

bool HandTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_panning_ = false;
    return true;
}

} // namespace meta_editor
