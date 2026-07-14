/*
 * Meta Editor - Text Tool Implementation
 */

#include "meta_editor/tools/text_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <flex/runtime/text.h>
#include <flex/runtime/group.h>

namespace meta_editor {

TextTool::TextTool() {}

void TextTool::activate() {
    mode_ = Mode::Place;
    editing_text_ = nullptr;
    text_buffer_.clear();
    cursor_pos_ = 0;
    show_preview_ = false;
}

void TextTool::deactivate() {
    if (mode_ == Mode::Edit) {
        finish_editing();
    }
}

bool TextTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::Edit) {
        // Check if clicking on the same text - reposition cursor
        // Otherwise finish editing
        finish_editing();
    }

    // Check if clicking on existing text
    auto* hit = canvas_->hit_test(screen_pos);
    if (hit && hit->type() == flex::NodeType::Text) {
        start_editing(static_cast<flex::Text*>(hit));
        return true;
    }

    // Create new text at click position
    auto layers = canvas_->get_all_layers();
    if (layers.empty()) return false;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* text = flex::Text::create(*allocator);
    text->set_position(world_pos.x, world_pos.y);
    text->set_content("Text");
    text->set_font_size(24.0f);
    text->set_color(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});

    layers[0]->add_child(text);
    selection_->select(text);
    start_editing(text);

    return true;
}

bool TextTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::Place) {
        preview_pos_ = world_pos;
        show_preview_ = true;
    }
    return false;
}

bool TextTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    return false;
}

bool TextTool::on_key_down(int key, int mods) {
    if (mode_ != Mode::Edit) return false;

    // Escape - finish editing
    if (key == 27) {
        finish_editing();
        return true;
    }

    // Enter - finish editing (or add newline with Shift)
    if (key == 13) {
        if (mods & 1) {  // Shift
            insert_char("\n");
        } else {
            finish_editing();
        }
        return true;
    }

    // Backspace
    if (key == 8) {
        delete_char();
        return true;
    }

    // Delete
    if (key == 127) {
        if (cursor_pos_ < static_cast<int>(text_buffer_.size())) {
            text_buffer_.erase(cursor_pos_, 1);
            if (editing_text_) {
                editing_text_->set_content(text_buffer_);
            }
        }
        return true;
    }

    // Arrow keys
    if (key == 1073741904) {  // Left
        move_cursor(-1);
        return true;
    }
    if (key == 1073741903) {  // Right
        move_cursor(1);
        return true;
    }

    // Home
    if (key == 1073741898) {
        cursor_pos_ = 0;
        return true;
    }

    // End
    if (key == 1073741901) {
        cursor_pos_ = static_cast<int>(text_buffer_.size());
        return true;
    }

    return false;
}

bool TextTool::on_key_up(int key, int mods) {
    return false;
}

bool TextTool::on_text_input(const char* text) {
    if (mode_ != Mode::Edit) return false;

    insert_char(text);
    return true;
}

void TextTool::render_overlay(flex::Renderer& renderer) {
    // Show preview cursor in place mode
    if (mode_ == Mode::Place && show_preview_) {
        flex::Paint cursor_paint = flex::Paint::solid(flex::Color(0.3f, 0.3f, 0.3f, 0.5f));
        renderer.draw_text("T", preview_pos_.x, preview_pos_.y, "Arial", 24, false,
                          flex::Color{0.3f, 0.3f, 0.3f, 0.5f});
    }

    // Show cursor in edit mode
    if (mode_ == Mode::Edit && editing_text_) {
        // Calculate cursor position based on text content
        float x = editing_text_->x();
        float y = editing_text_->y();
        float font_size = editing_text_->font_size();

        // Approximate cursor x position (rough estimate)
        float char_width = font_size * 0.6f;
        float cursor_x = x + cursor_pos_ * char_width;

        // Blinking cursor
        cursor_blink_ += 0.05f;
        if (static_cast<int>(cursor_blink_ * 2) % 2 == 0) {
            flex::Paint cursor_paint = flex::Paint::solid(flex::Color(0.0f, 0.0f, 0.0f, 1.0f));
            std::string path = "M " + std::to_string(cursor_x) + " " + std::to_string(y) +
                              " L " + std::to_string(cursor_x) + " " + std::to_string(y + font_size);
            renderer.stroke_path(path, cursor_paint, 2.0f);
        }
    }
}

void TextTool::start_editing(flex::Text* text) {
    editing_text_ = text;
    text_buffer_ = text->content();
    cursor_pos_ = static_cast<int>(text_buffer_.size());
    cursor_blink_ = 0;
    mode_ = Mode::Edit;
    selection_->select(text);
}

void TextTool::finish_editing() {
    if (editing_text_ && text_buffer_.empty()) {
        // Remove empty text
        if (editing_text_->parent()) {
            static_cast<flex::Group*>(editing_text_->parent())->remove_child(editing_text_);
        }
        selection_->clear_selection();
    }
    editing_text_ = nullptr;
    text_buffer_.clear();
    cursor_pos_ = 0;
    mode_ = Mode::Place;
}

void TextTool::insert_char(const char* text) {
    if (!editing_text_) return;

    text_buffer_.insert(cursor_pos_, text);
    cursor_pos_ += static_cast<int>(strlen(text));
    editing_text_->set_content(text_buffer_);
    cursor_blink_ = 0;
}

void TextTool::delete_char() {
    if (!editing_text_ || cursor_pos_ <= 0) return;

    text_buffer_.erase(cursor_pos_ - 1, 1);
    cursor_pos_--;
    editing_text_->set_content(text_buffer_);
    cursor_blink_ = 0;
}

void TextTool::move_cursor(int delta) {
    cursor_pos_ += delta;
    if (cursor_pos_ < 0) cursor_pos_ = 0;
    if (cursor_pos_ > static_cast<int>(text_buffer_.size())) {
        cursor_pos_ = static_cast<int>(text_buffer_.size());
    }
    cursor_blink_ = 0;
}

} // namespace meta_editor
