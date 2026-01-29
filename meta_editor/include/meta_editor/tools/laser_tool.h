/*
 * Meta Editor - Laser Pointer Tool
 *
 * Temporary visual pointer for presentations. Draws a fading trail.
 * Press 'Z' to activate.
 */

#pragma once

#include "../tool.h"
#include <flex.h>
#include <vector>

namespace meta_editor {

class LaserTool : public Tool {
public:
    LaserTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void update(float dt) override;
    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Laser"; }
    const char* icon() const override { return "laser"; }

    void set_color(const flex::Color& color) { color_ = color; }

private:
    struct TrailPoint {
        flex::Vec2 pos;
        float age;
    };

    bool is_active_ = false;
    flex::Vec2 cursor_pos_;
    std::vector<TrailPoint> trail_;
    
    flex::Color color_ = {1.0f, 0.2f, 0.2f, 1.0f};  // Red
    float trail_lifetime_ = 0.5f;
    float point_radius_ = 8.0f;
};

} // namespace meta_editor
