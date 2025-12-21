/*
 * Select Tool
 *
 * Tool for selecting, moving, scaling, and rotating nodes.
 * Uses TransformGizmo for visual feedback and Transform2D for operations.
 */

#pragma once

#include "../viewmodel/tool.h"
#include "../viewmodel/editor_vm.h"
#include "../command/command.h"
#include "../command/transform_commands.h"
#include "transform_gizmo.h"
#include "snap_helper.h"

namespace editor {

// Select Tool with full transform support
class SelectTool : public Tool {
public:
    const char* name() const override { return "Select"; }
    const char* icon() const override { return "cursor"; }
    const char* tooltip() const override { return "Select and transform objects (V)"; }

    void activate(EditorViewModel* vm) override;

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;

    void render(flex::Renderer& renderer) override;

private:
    enum class Mode { None, Move, SelectBox, Resize, Rotate };

    void updateGizmo();
    void storeOriginalTransforms();
    void performMove(const MouseEvent& e);
    void performResize(const MouseEvent& e);
    void performRotate(const MouseEvent& e);
    void commitTransform(const MouseEvent& e);
    void nudgeSelection(float dx, float dy);
    Rect normalizeRect(const Rect& r);

    // State
    Mode mode_ = Mode::None;
    HandleType active_handle_ = HandleType::None;
    TransformGizmo gizmo_;

    Point drag_start_;
    Point last_pos_;
    bool is_dragging_ = false;
    Rect selection_box_;

    float start_angle_ = 0;

    std::vector<Transform2D> original_transforms_;
    Rect original_bounds_;

    // Snapping
    SnapHelper snapHelper_;
    SnapGuides activeGuides_;
};

} // namespace editor
