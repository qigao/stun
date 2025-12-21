/*
 * Navigator Panel (Mini-map)
 *
 * Shows thumbnail overview of the document with viewport indicator.
 * Allows quick navigation by clicking/dragging on the mini-map.
 * Supports dragging to reposition.
 */

#pragma once

#include "../core/types.h"
#include <flex/renderer.h>

namespace editor {

class EditorViewModel;

// Navigator panel style
struct NavigatorPanelStyle {
    float width = 200;
    float height = 180;
    float title_height = 24;
    float padding = 8;

    Color background = {0.15f, 0.15f, 0.15f, 0.95f};
    Color title_bg = {0.2f, 0.2f, 0.2f, 1.0f};
    Color title_text = {0.8f, 0.8f, 0.8f, 1.0f};
    Color canvas_bg = {0.25f, 0.25f, 0.25f, 1.0f};
    Color viewport_border = {0.4f, 0.7f, 1.0f, 1.0f};
    Color viewport_fill = {0.4f, 0.7f, 1.0f, 0.15f};
    Color shape_fill = {0.5f, 0.5f, 0.5f, 0.8f};
};

class NavigatorPanel {
public:
    NavigatorPanel();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return style_.height; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isPanelDragging() const { return panel_dragging_; }

    NavigatorPanelStyle& style() { return style_; }

    void render(flex::Renderer& renderer);
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);

private:
    Rect getCanvasArea() const;
    Rect getViewportRect() const;
    void navigateTo(float mx, float my);
    bool isInTitleBar(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;

    // Panel drag state
    bool draggable_ = true;
    bool panel_dragging_ = false;
    float panel_drag_offset_x_ = 0;
    float panel_drag_offset_y_ = 0;

    // Viewport navigation drag state
    bool nav_dragging_ = false;
    float nav_offset_x_ = 0;
    float nav_offset_y_ = 0;

    NavigatorPanelStyle style_;
};

} // namespace editor
