/*
 * Meta Editor - Eraser Tool Implementation
 */

#include "meta_editor/tools/eraser_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

bool EraserTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_erasing_ = true;
    cursor_pos_ = world_pos;
    erase_at(world_pos);
    return true;
}

bool EraserTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    cursor_pos_ = world_pos;
    if (is_erasing_) {
        erase_at(world_pos);
    }
    return true;
}

bool EraserTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_erasing_ = false;
    return true;
}

void EraserTool::erase_at(const flex::Vec2& world_pos) {
    auto* node = canvas_->hit_test(canvas_->world_to_screen(world_pos));
    if (!node) return;
    if (node == canvas_->content_root()) return;
    
    // Remove from selection if selected
    selection_->remove_from_selection(node);
    
    // Remove from parent
    if (node->parent()) {
        static_cast<flex::Group*>(node->parent())->remove_child(node);
    }
}

void EraserTool::render_overlay(flex::Renderer& renderer) {
    // Draw eraser cursor circle
    flex::Paint stroke = flex::Paint::solid(flex::Color{1.0f, 0.3f, 0.3f, 0.8f});
    flex::Paint fill = flex::Paint::solid(flex::Color{1.0f, 0.3f, 0.3f, 0.15f});
    
    renderer.draw_circle(cursor_pos_.x(), cursor_pos_.y(), size_ / 2,
                         fill, stroke, 2.0f);
}

} // namespace meta_editor
