/*
 * Meta Editor - Navigator Panel
 *
 * Mini-map showing viewport position within the canvas.
 * Click to pan, drag the viewport rectangle to navigate.
 */

#pragma once

#include "panel.h"

namespace meta_editor {

class Canvas;

class NavigatorPanel : public Panel {
public:
    explicit NavigatorPanel(Canvas* canvas);

    // Render the navigator (called in screen space)
    void render(flex::Renderer& renderer) override;

    // Handle mouse events
    bool handle_click(float x, float y);
    bool handle_drag(float x, float y);
    void end_drag();

private:
    // Convert navigator coords to world coords
    flex::Vec2 navigator_to_world(float nx, float ny) const;

    Canvas* canvas_;
    bool dragging_ = false;

    // Content bounds (cached)
    flex::Bounds content_bounds_;
};

} // namespace meta_editor
