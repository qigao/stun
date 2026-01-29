/*
 * Meta Editor - Sticky Note Tool Implementation
 */

#include "meta_editor/tools/sticky_note_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

// Preset colors: Yellow, Pink, Blue, Green, Orange
const flex::Color StickyNoteTool::COLORS[NUM_COLORS] = {
    {1.0f, 0.95f, 0.6f, 1.0f},   // Yellow
    {1.0f, 0.75f, 0.8f, 1.0f},   // Pink
    {0.7f, 0.85f, 1.0f, 1.0f},   // Blue
    {0.75f, 1.0f, 0.75f, 1.0f},  // Green
    {1.0f, 0.85f, 0.6f, 1.0f}    // Orange
};

bool StickyNoteTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::Edit) {
        // Click outside note finishes editing
        finish_editing();
    }
    
    note_pos_ = world_pos;
    create_note();
    return true;
}

bool StickyNoteTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::Place) {
        preview_pos_ = world_pos;
        show_preview_ = true;
        return true;
    }
    return false;
}

bool StickyNoteTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return false;
}

bool StickyNoteTool::on_key_down(int key, int mods) {
    // Color selection with 1-5 keys
    if (key >= '1' && key <= '5') {
        set_color_index(key - '1');
        return true;
    }
    
    if (mode_ != Mode::Edit) return false;
    
    // Enter finishes editing
    if (key == 13) {  // Enter
        finish_editing();
        return true;
    }
    
    // Escape cancels
    if (key == 27) {  // Escape
        text_buffer_.clear();
        finish_editing();
        return true;
    }
    
    // Backspace
    if (key == 8 && !text_buffer_.empty()) {
        text_buffer_.pop_back();
        if (editing_text_) {
            editing_text_->set_content(text_buffer_.empty() ? "Type here..." : text_buffer_);
        }
        return true;
    }
    
    return false;
}

bool StickyNoteTool::on_text_input(const char* text) {
    if (mode_ != Mode::Edit || !text) return false;
    
    text_buffer_ += text;
    if (editing_text_) {
        editing_text_->set_content(text_buffer_);
    }
    return true;
}

void StickyNoteTool::render_overlay(flex::Renderer& renderer) {
    if (!show_preview_ || mode_ == Mode::Edit) return;
    
    // Draw preview note at cursor
    flex::Color color = COLORS[color_index_];
    color.a = 0.5f;  // Semi-transparent preview
    
    flex::Paint fill = flex::Paint::solid(color);
    flex::Paint stroke = flex::Paint::solid(flex::Color{0, 0, 0, 0.2f});
    renderer.draw_rect(preview_pos_.x(), preview_pos_.y(), 
                       note_width_, note_min_height_, 4.0f,
                       fill, stroke, 1.0f);
}

void StickyNoteTool::set_color_index(int idx) {
    color_index_ = idx % NUM_COLORS;
    
    // Update current note if editing
    if (mode_ == Mode::Edit && editing_note_) {
        auto& children = editing_note_->children();
        if (!children.empty() && children[0]->type() == flex::NodeType::Shape) {
            auto* bg = static_cast<flex::Shape*>(children[0]);
            bg->set_fill(COLORS[color_index_]);
        }
    }
}

void StickyNoteTool::create_note() {
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();
    
    // Create group for note
    auto* group = flex::Group::create(*allocator);
    group->set_position(note_pos_.x(), note_pos_.y());
    
    // Background rectangle
    auto* bg = flex::Shape::create(*allocator);
    bg->set_rect(note_width_, note_min_height_, 4.0f);
    bg->set_position(0, 0);
    bg->set_fill(COLORS[color_index_]);
    bg->set_stroke(flex::Color{0, 0, 0, 0.15f}, 1.0f);
    group->add_child(bg);
    
    // Text
    auto* text = flex::Text::create(*allocator);
    text->set_content("Type here...");
    text->set_position(padding_, padding_);
    text->set_font_size(font_size_);
    text->set_color(flex::Color{0.2f, 0.2f, 0.2f, 1.0f});
    group->add_child(text);
    
    layer->add_child(group);
    
    // Enter edit mode
    mode_ = Mode::Edit;
    editing_note_ = group;
    editing_text_ = text;
    text_buffer_.clear();
    show_preview_ = false;
    
    selection_->clear_selection();
    selection_->select(group);
}

void StickyNoteTool::finish_editing() {
    if (editing_text_ && text_buffer_.empty()) {
        editing_text_->set_content("Note");
    }
    
    mode_ = Mode::Place;
    editing_note_ = nullptr;
    editing_text_ = nullptr;
    text_buffer_.clear();
}

} // namespace meta_editor
