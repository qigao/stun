/*
 * Meta Editor - Text Tool
 *
 * Tool for creating and editing text elements.
 */

#pragma once

#include "../tool.h"
#include <flex.h>
#include <string>

namespace meta_editor {

/**
 * TextTool - Create and edit text elements
 *
 * Click to place text, type to edit.
 */
class TextTool : public Tool {
public:
    TextTool();

    void activate() override;
    void deactivate() override;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    bool on_key_up(int key, int mods) override;
    bool on_text_input(const char* text) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Text"; }
    const char* icon() const override { return "text"; }

private:
    enum class Mode {
        Place,      // Click to place new text
        Edit        // Editing existing text
    };

    Mode mode_ = Mode::Place;
    flex::Text* editing_text_ = nullptr;
    std::string text_buffer_;
    int cursor_pos_ = 0;
    float cursor_blink_ = 0;

    // Preview position before click
    flex::Vec2 preview_pos_;
    bool show_preview_ = false;

    void start_editing(flex::Text* text);
    void finish_editing();
    void insert_char(const char* text);
    void delete_char();
    void move_cursor(int delta);
};

} // namespace meta_editor
