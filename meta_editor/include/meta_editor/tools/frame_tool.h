/*
 * Meta Editor - Frame Tool
 *
 * Creates container frames that can hold other elements.
 * Like Figma frames - useful for organizing content.
 * Press 'F' to activate.
 */

#pragma once

#include "../tool.h"
#include <flex.h>

namespace meta_editor {

class FrameTool : public Tool {
public:
    FrameTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Frame"; }
    const char* icon() const override { return "frame"; }

private:
    bool is_drawing_ = false;
    flex::Vec2 start_pos_;
    flex::Vec2 current_pos_;
};

} // namespace meta_editor
