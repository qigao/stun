/*
 * Meta Editor - Image Tool Implementation
 */

#include "meta_editor/tools/image_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

bool ImageTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();
    
    if (!pending_image_path_.empty()) {
        // Create actual image
        auto* img = flex::Image::create(*allocator);
        img->set_src(pending_image_path_);
        img->set_position(world_pos.x, world_pos.y);
        img->set_width(default_width_);
        img->set_height(default_height_);
        layer->add_child(img);
        
        selection_->clear_selection();
        selection_->select(img);
        pending_image_path_.clear();
    } else {
        // Create placeholder (rect with X)
        auto* group = flex::Group::create(*allocator);
        group->set_position(world_pos.x, world_pos.y);
        
        // Background
        auto* bg = flex::Shape::create(*allocator);
        bg->set_rect(default_width_, default_height_, 4.0f);
        bg->set_position(0, 0);
        bg->set_fill(flex::Color{0.95f, 0.95f, 0.95f, 1.0f});
        bg->set_stroke(flex::Color{0.8f, 0.8f, 0.8f, 1.0f}, 2.0f);
        group->add_child(bg);
        
        // X lines (placeholder indicator) - use set_path with explicit bounds
        auto* line1 = flex::Shape::create(*allocator);
        line1->set_path("M 20 20 L 180 130", 160, 110, 20, 20);
        line1->set_stroke(flex::Color{0.7f, 0.7f, 0.7f, 1.0f}, 2.0f);
        group->add_child(line1);
        
        auto* line2 = flex::Shape::create(*allocator);
        line2->set_path("M 180 20 L 20 130", 160, 110, 20, 20);
        line2->set_stroke(flex::Color{0.7f, 0.7f, 0.7f, 1.0f}, 2.0f);
        group->add_child(line2);
        
        // Icon hint
        auto* label = flex::Text::create(*allocator);
        label->set_content("Image");
        label->set_position(default_width_ / 2 - 20, default_height_ / 2 - 8);
        label->set_font_size(14.0f);
        label->set_color(flex::Color{0.6f, 0.6f, 0.6f, 1.0f});
        group->add_child(label);
        
        layer->add_child(group);
        
        selection_->clear_selection();
        selection_->select(group);
    }
    
    return true;
}

bool ImageTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    preview_pos_ = world_pos;
    show_preview_ = true;
    return true;
}

bool ImageTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return false;
}

void ImageTool::render_overlay(flex::Renderer& renderer) {
    if (!show_preview_) return;
    
    flex::Paint fill = flex::Paint::solid(flex::Color{0.9f, 0.9f, 0.9f, 0.5f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.5f, 0.5f, 0.5f, 0.8f});
    
    renderer.draw_rect(preview_pos_.x, preview_pos_.y,
                       default_width_, default_height_, 4.0f,
                       fill, stroke, 2.0f);
}

} // namespace meta_editor
