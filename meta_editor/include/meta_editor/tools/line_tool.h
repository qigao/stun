/*
 * Meta Editor - Line Tool
 *
 * Click-drag to draw lines. Shift constrains to 45° angles.
 * Supports arrow endpoints for whiteboard/diagram use.
 */

#pragma once

#include "../tool.h"
#include <flex.h>

namespace meta_editor {

enum class ArrowStyle : uint8_t {
    None,       // No arrow
    End,        // Arrow at end point
    Start,      // Arrow at start point
    Both        // Arrows at both ends
};

class LineTool : public Tool {
public:
    LineTool() = default;

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    bool on_key_up(int key, int mods) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Line"; }
    const char* icon() const override { return "line"; }

    void set_stroke_color(const flex::Color& color) { stroke_color_ = color; }
    void set_stroke_width(float width) { stroke_width_ = width; }
    void set_arrow_style(ArrowStyle style) { arrow_style_ = style; }
    ArrowStyle arrow_style() const { return arrow_style_; }

    void cycle_arrow_style();

private:
    flex::Vec2 constrain_angle(const flex::Vec2& start, const flex::Vec2& end) const;
    std::string build_arrow_path(const flex::Vec2& from, const flex::Vec2& to, float size) const;

    flex::Vec2 start_pos_;
    flex::Vec2 current_pos_;
    bool is_drawing_ = false;
    bool shift_held_ = false;

    flex::Color stroke_color_ = flex::Color::Black;
    float stroke_width_ = 2.0f;
    ArrowStyle arrow_style_ = ArrowStyle::End;  // Default: arrow at end
    float arrow_size_ = 12.0f;
};

} // namespace meta_editor
