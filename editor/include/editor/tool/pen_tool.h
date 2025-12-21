/*
 * Pen Tool
 *
 * Tool for creating bezier paths with anchor points and control handles.
 * Click to add corner points, drag to create curves.
 */

#pragma once

#include "../viewmodel/tool.h"
#include "../viewmodel/editor_vm.h"
#include "../command/command.h"
#include "draw_tool.h"
#include <vector>

namespace editor {

// Path point with optional bezier handles
struct PathPoint {
    Point anchor;           // Main anchor point
    Point handleIn;         // Incoming control handle
    Point handleOut;        // Outgoing control handle
    bool hasHandleIn = false;
    bool hasHandleOut = false;

    // Smooth point: handles are symmetric
    bool smooth = false;
};

// Pen Tool for drawing bezier paths
class PenTool : public Tool {
public:
    const char* name() const override { return "Pen"; }
    const char* icon() const override { return "pen"; }
    const char* tooltip() const override { return "Draw bezier paths (P)"; }

    void activate(EditorViewModel* vm) override;
    void deactivate() override;

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;

    void render(flex::Renderer& renderer) override;

private:
    void reset();
    bool hitTestPoint(const Point& p, const Point& target) const;
    std::string buildPathString() const;
    std::string buildPreviewPath() const;
    void drawAnchor(flex::Renderer& renderer, const Point& p, float size,
                    const flex::Color& fill, const flex::Color& stroke, bool highlight);
    void drawHandle(flex::Renderer& renderer, const Point& p, float size, const flex::Color& color);
    void closePath();
    void finishPath();

    std::vector<PathPoint> points_;
    Point preview_point_;
    bool is_dragging_ = false;
    bool adjusting_handles_ = false;
    bool is_closed_ = false;
};

} // namespace editor
