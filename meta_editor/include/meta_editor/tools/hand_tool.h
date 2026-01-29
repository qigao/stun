/*
 * Meta Editor - Hand Tool
 *
 * Pan the canvas by dragging. Essential for whiteboard navigation.
 * Press 'H' to activate, or hold Space temporarily while using other tools.
 */

#pragma once

#include "../tool.h"
#include <flex.h>

namespace meta_editor {

class HandTool : public Tool {
public:
    HandTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override {}

    const char* name() const override { return "Hand"; }
    const char* icon() const override { return "hand"; }

private:
    bool is_panning_ = false;
    flex::Vec2 last_screen_pos_;
};

} // namespace meta_editor
