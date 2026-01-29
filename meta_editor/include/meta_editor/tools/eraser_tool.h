/*
 * Meta Editor - Eraser Tool
 *
 * Click or drag to delete shapes. Press 'X' to activate.
 */

#pragma once

#include "../tool.h"
#include <flex.h>
#include <vector>

namespace meta_editor {

class EraserTool : public Tool {
public:
    EraserTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Eraser"; }
    const char* icon() const override { return "eraser"; }

    void set_size(float size) { size_ = size; }
    float size() const { return size_; }

private:
    void erase_at(const flex::Vec2& world_pos);
    
    bool is_erasing_ = false;
    flex::Vec2 cursor_pos_;
    float size_ = 20.0f;
};

} // namespace meta_editor
