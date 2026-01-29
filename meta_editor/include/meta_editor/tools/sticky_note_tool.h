/*
 * Meta Editor - Sticky Note Tool
 *
 * Creates colored note cards with text. Essential for whiteboard brainstorming.
 * Click to place, then type. Press Enter to finish.
 */

#pragma once

#include "../tool.h"
#include <flex.h>

namespace meta_editor {

class StickyNoteTool : public Tool {
public:
    StickyNoteTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    bool on_text_input(const char* text) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "StickyNote"; }
    const char* icon() const override { return "sticky-note"; }

    // Preset colors (cycle with 1-5 keys)
    void set_color_index(int idx);
    int color_index() const { return color_index_; }

private:
    void create_note();
    void finish_editing();
    
    static constexpr int NUM_COLORS = 5;
    static const flex::Color COLORS[NUM_COLORS];

    enum class Mode { Place, Edit };
    Mode mode_ = Mode::Place;
    
    flex::Vec2 preview_pos_;
    flex::Vec2 note_pos_;
    bool show_preview_ = false;
    
    std::string text_buffer_;
    int color_index_ = 0;  // Yellow default
    
    flex::Group* editing_note_ = nullptr;
    flex::Text* editing_text_ = nullptr;
    
    float note_width_ = 200.0f;
    float note_min_height_ = 100.0f;
    float padding_ = 12.0f;
    float font_size_ = 16.0f;
};

} // namespace meta_editor
