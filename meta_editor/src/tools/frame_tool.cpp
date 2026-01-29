/*
 * Meta Editor - Frame Tool Implementation
 */

#include "meta_editor/tools/frame_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include <algorithm>

namespace meta_editor {

bool FrameTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_drawing_ = true;
    start_pos_ = world_pos;
    current_pos_ = world_pos;
    return true;
}

bool FrameTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) return false;
    current_pos_ = world_pos;
    return true;
}

bool FrameTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) return false;
    is_drawing_ = false;
    
    float x = std::min(start_pos_.x(), world_pos.x());
    float y = std::min(start_pos_.y(), world_pos.y());
    float w = std::abs(world_pos.x() - start_pos_.x());
    float h = std::abs(world_pos.y() - start_pos_.y());
    
    // Minimum size
    if (w < 20 || h < 20) {
        w = std::max(w, 200.0f);
        h = std::max(h, 150.0f);
    }
    
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();
    
    // Create frame group with background
    auto* frame = flex::Group::create(*allocator);
    frame->set_position(x, y);
    
    // Background rect (white with border)
    auto* bg = flex::Shape::create(*allocator);
    bg->set_rect(w, h, 0);
    bg->set_position(0, 0);
    bg->set_fill(flex::Color{1.0f, 1.0f, 1.0f, 1.0f});
    bg->set_stroke(flex::Color{0.8f, 0.8f, 0.8f, 1.0f}, 1.0f);
    frame->add_child(bg);
    
    // Frame label (above the frame, not inside)
    auto* label = flex::Text::create(*allocator);
    label->set_content("Frame");
    label->set_position(8, 8);
    label->set_font_size(12.0f);
    label->set_color(flex::Color{0.5f, 0.5f, 0.5f, 1.0f});
    frame->add_child(label);
    
    layer->add_child(frame);
    
    selection_->clear_selection();
    selection_->select(frame);
    
    return true;
}

void FrameTool::render_overlay(flex::Renderer& renderer) {
    if (!is_drawing_) return;
    
    float x = std::min(start_pos_.x(), current_pos_.x());
    float y = std::min(start_pos_.y(), current_pos_.y());
    float w = std::abs(current_pos_.x() - start_pos_.x());
    float h = std::abs(current_pos_.y() - start_pos_.y());
    
    flex::Paint fill = flex::Paint::solid(flex::Color{0.9f, 0.95f, 1.0f, 0.3f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.2f, 0.5f, 1.0f, 0.8f});
    
    renderer.draw_rect(x, y, w, h, 0, fill, stroke, 2.0f);
}

} // namespace meta_editor
