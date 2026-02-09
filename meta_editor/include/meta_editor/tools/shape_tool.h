/*
 * Meta Editor - Shape Tool
 * 
 * Tool for creating basic shapes (rect, circle, etc.)
 */

#pragma once

#include "../tool.h"
#include <flex.h>

namespace meta_editor {

/**
 * ShapeTool - Basic shape creation tool
 * 
 * Features:
 * - Drag to create shape
 * - Shift to constrain proportions
 * - Alt to draw from center
 */
class ShapeTool : public Tool {
public:
    enum class ShapeType {
        Rectangle,
        Circle,
        Ellipse,
        Polygon,
        Star,
        Triangle
    };
    
    explicit ShapeTool(ShapeType type);
    
    // Tool interface
    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_key_down(int key, int mods) override;
    
    void render_overlay(flex::Renderer& renderer) override;
    
    const char* name() const override;
    const char* icon() const override;

private:
    void create_shape();
    
    ShapeType shape_type_;
    flex::Vec2 start_pos_;
    flex::Vec2 current_pos_;
    bool is_drawing_ = false;
    bool constrain_proportions_ = false;
    bool draw_from_center_ = false;
    
    flex::Node* preview_shape_ = nullptr;
};

} // namespace meta_editor
