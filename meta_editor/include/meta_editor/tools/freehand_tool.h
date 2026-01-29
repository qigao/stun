/*
 * Meta Editor - Freehand Tool
 *
 * Free drawing with mouse/pen. Creates smooth paths from raw input.
 * Essential for whiteboard sketching.
 */

#pragma once

#include "../tool.h"
#include <flex.h>
#include <vector>

namespace meta_editor {

class FreehandTool : public Tool {
public:
    FreehandTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Freehand"; }
    const char* icon() const override { return "pencil"; }

    void set_stroke_color(const flex::Color& color) { stroke_color_ = color; }
    void set_stroke_width(float width) { stroke_width_ = width; }
    
    // Smoothing: 0 = raw points, 1 = maximum smoothing
    void set_smoothing(float s) { smoothing_ = s; }

private:
    std::string points_to_path() const;
    void simplify_points();

    std::vector<flex::Vec2> points_;
    bool is_drawing_ = false;

    flex::Color stroke_color_ = flex::Color::Black;
    float stroke_width_ = 2.0f;
    float smoothing_ = 0.5f;
    float min_distance_ = 3.0f;  // Minimum distance between points
};

} // namespace meta_editor
