/*
 * Meta Editor - Pen Tool
 *
 * Tool for drawing bezier paths.
 * Click = corner point, drag = smooth curve with handles.
 */

#pragma once

#include "../tool.h"
#include "../path_edit.h"
#include <flex.h>

namespace meta_editor {

class PenTool : public Tool {
public:
    PenTool();

    void activate() override;
    void deactivate() override;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Pen"; }
    const char* icon() const override { return "pen"; }

private:
    void finish_path();
    void cancel_path();
    void close_path();
    bool is_near_first_point(const flex::Vec2& pos, float threshold = 10.0f) const;

    PathData path_;
    flex::Vec2 current_point_;
    flex::Vec2 drag_start_;
    bool is_drawing_ = false;
    bool is_dragging_ = false;  // True when dragging to create handles
};

} // namespace meta_editor
