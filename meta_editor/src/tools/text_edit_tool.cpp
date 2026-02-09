/*
 * Meta Editor - Text Edit Tool Implementation
 */

#include "meta_editor/tools/text_edit_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <flex/runtime/text.h>
#include <cstring>

namespace meta_editor {

void TextEditTool::activate() {
    // If no text is being edited, this tool shouldn't be active
}

void TextEditTool::deactivate() {
    if (editing_text_) {
        commit_edit();
    }
}

void TextEditTool::edit_text(flex::Node* text_node) {
    if (!text_node || text_node->type() != flex::NodeType::Text) return;

    editing_text_ = text_node;
    auto* text = static_cast<flex::Text*>(text_node);
    text_buffer_ = text->content();
    original_text_ = text_buffer_;
    cursor_pos_ = text_buffer_.length();
}

bool TextEditTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!editing_text_) return false;

    // Click outside text = commit and exit
    auto* hit = canvas_->hit_test(world_pos);
    if (hit != editing_text_) {
        commit_edit();
        return true;
    }
    return true;
}

bool TextEditTool::on_key_down(int key, int mods) {
    if (!editing_text_) return false;

    if (key == static_cast<int>(Key::Escape)) {
        // Cancel - restore original
        auto* text = static_cast<flex::Text*>(editing_text_);
        text->set_content(original_text_.c_str());
        editing_text_ = nullptr;
        text_buffer_.clear();
        return true;
    }

    if (key == static_cast<int>(Key::Enter)) {
        commit_edit();
        return true;
    }

    if (key == static_cast<int>(Key::Backspace) && cursor_pos_ > 0) {
        text_buffer_.erase(cursor_pos_ - 1, 1);
        cursor_pos_--;
        auto* text = static_cast<flex::Text*>(editing_text_);
        text->set_content(text_buffer_.c_str());
        return true;
    }

    if (key == static_cast<int>(Key::Delete) && cursor_pos_ < text_buffer_.length()) {
        text_buffer_.erase(cursor_pos_, 1);
        auto* text = static_cast<flex::Text*>(editing_text_);
        text->set_content(text_buffer_.c_str());
        return true;
    }

    if (key == static_cast<int>(Key::Left) && cursor_pos_ > 0) {
        cursor_pos_--;
        return true;
    }

    if (key == static_cast<int>(Key::Right) && cursor_pos_ < text_buffer_.length()) {
        cursor_pos_++;
        return true;
    }

    if (key == static_cast<int>(Key::Home)) {
        cursor_pos_ = 0;
        return true;
    }

    if (key == static_cast<int>(Key::End)) {
        cursor_pos_ = text_buffer_.length();
        return true;
    }

    return true;
}

bool TextEditTool::on_text_input(const char* text) {
    if (!editing_text_) return false;

    text_buffer_.insert(cursor_pos_, text);
    cursor_pos_ += strlen(text);

    auto* txt = static_cast<flex::Text*>(editing_text_);
    txt->set_content(text_buffer_.c_str());
    return true;
}

void TextEditTool::render_overlay(flex::Renderer& renderer) {
    if (!editing_text_) return;
    selection_->render_selection_indicators(renderer);
}

void TextEditTool::render_screen_overlay(flex::Renderer& renderer) {
}

void TextEditTool::commit_edit() {
    if (!editing_text_) return;

    // Create undo command if text changed
    if (text_buffer_ != original_text_ && commands_) {
        // Restore original first
        auto* text = static_cast<flex::Text*>(editing_text_);
        text->set_content(original_text_.c_str());

        auto cmd = std::make_unique<EditTextCommand>(
            editing_text_, original_text_, text_buffer_);
        commands_->execute(std::move(cmd));
    }

    editing_text_ = nullptr;
    text_buffer_.clear();
    original_text_.clear();
    cursor_pos_ = 0;
}

} // namespace meta_editor
