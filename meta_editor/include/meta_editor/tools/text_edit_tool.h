/*
 * Meta Editor - Text Edit Tool
 *
 * Dedicated tool for editing text content.
 * Activated by double-clicking a text node in SelectTool.
 */

#pragma once

#include "../tool.h"
#include "../core/key_codes.h"
#include <flex.h>
#include <string>

namespace meta_editor {

class TextEditTool : public Tool {
public:
    void activate() override;
    void deactivate() override;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    bool on_text_input(const char* text) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "TextEdit"; }
    const char* icon() const override { return "text-cursor"; }

    // Entry point - called from SelectTool on double-click
    void edit_text(flex::Node* text_node);
    bool is_editing() const { return editing_text_ != nullptr; }
    flex::Node* editing_node() const { return editing_text_; }

private:
    flex::Node* editing_text_ = nullptr;
    std::string text_buffer_;
    std::string original_text_;
    size_t cursor_pos_ = 0;

    void commit_edit();
};

} // namespace meta_editor
