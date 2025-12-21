/*
 * Path Editor
 *
 * Tool for editing path nodes and bezier handles.
 * Allows selecting, moving, adding, and deleting path points.
 */

#pragma once

#include "../core/types.h"
#include "../model/node.h"
#include <flex/renderer.h>
#include <vector>

namespace editor {

class EditorViewModel;

// Path point types
enum class PointType {
    Corner,     // Sharp corner, independent handles
    Smooth,     // Smooth curve, aligned handles
    Symmetric   // Symmetric handles (equal length)
};

// Editable path point
struct PathEditPoint {
    Point position;
    Point handleIn;     // Control point before
    Point handleOut;    // Control point after
    PointType type = PointType::Corner;
    bool selected = false;
};

// Handle being dragged
enum class PathHandleType {
    None,
    Point,
    HandleIn,
    HandleOut
};

// Path editor style
struct PathEditorStyle {
    float point_size = 8;
    float handle_size = 6;
    float handle_line_width = 1;
    float hit_radius = 10;

    Color point_normal = {1.0f, 1.0f, 1.0f, 1.0f};
    Color point_selected = {0.3f, 0.6f, 1.0f, 1.0f};
    Color point_hover = {0.5f, 0.8f, 1.0f, 1.0f};
    Color handle_color = {0.4f, 0.7f, 1.0f, 1.0f};
    Color handle_line = {0.4f, 0.7f, 1.0f, 0.6f};
    Color path_color = {0.2f, 0.5f, 0.9f, 1.0f};
};

class PathEditor {
public:
    PathEditor();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Set path to edit (extracts points from ShapeNode)
    void setEditingNode(ShapeNode* node);
    ShapeNode* editingNode() const { return editing_node_; }
    bool isEditing() const { return editing_node_ != nullptr; }

    // Exit editing mode
    void endEditing();

    // Path points
    const std::vector<PathEditPoint>& points() const { return points_; }
    void setPoints(const std::vector<PathEditPoint>& pts);

    // Selection
    void selectPoint(size_t index, bool addToSelection = false);
    void selectAll();
    void deselectAll();
    std::vector<size_t> selectedIndices() const;

    // Point operations
    void deleteSelectedPoints();
    void setSelectedPointType(PointType type);
    void insertPointAt(const Point& position, size_t afterIndex);

    // Style
    PathEditorStyle& style() { return style_; }

    // Rendering (overlay on canvas)
    void render(flex::Renderer& renderer, const Transform2D& viewTransform);

    // Input handling
    bool onMouseDown(const Point& worldPos, int button, bool shift);
    bool onMouseMove(const Point& worldPos);
    bool onMouseUp(const Point& worldPos, int button);
    bool onKeyDown(int key);

private:
    struct HitResult {
        int pointIndex = -1;
        PathHandleType handleType = PathHandleType::None;
    };

    HitResult hitTest(const Point& worldPos) const;
    void updateNodeFromPoints();
    void applyHandleConstraints(size_t pointIndex, PathHandleType movedHandle);

    EditorViewModel* vm_ = nullptr;
    ShapeNode* editing_node_ = nullptr;

    std::vector<PathEditPoint> points_;
    int hovered_point_ = -1;
    PathHandleType hovered_handle_ = PathHandleType::None;

    // Drag state
    bool dragging_ = false;
    int drag_point_ = -1;
    PathHandleType drag_handle_ = PathHandleType::None;
    Point drag_start_;

    PathEditorStyle style_;
};

} // namespace editor
