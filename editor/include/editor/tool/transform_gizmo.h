/*
 * Transform Gizmo
 *
 * Visual handles for scaling, rotating, and transforming selected objects.
 * Uses ViewTransform for proper coordinate mapping between world and screen space.
 */

#pragma once

#include "../core/types.h"
#include "../core/transform.h"
#include "../model/node.h"
#include <vector>
#include <cmath>

namespace editor {

// Handle types
enum class HandleType {
    None,
    // Corner handles (scale)
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    // Edge handles (scale one axis)
    Top,
    Right,
    Bottom,
    Left,
    // Rotation handle
    Rotate,
    // Move (center)
    Move
};

// Cursor types for handles
inline const char* handleCursor(HandleType type) {
    switch (type) {
        case HandleType::TopLeft:
        case HandleType::BottomRight:
            return "nwse-resize";
        case HandleType::TopRight:
        case HandleType::BottomLeft:
            return "nesw-resize";
        case HandleType::Top:
        case HandleType::Bottom:
            return "ns-resize";
        case HandleType::Left:
        case HandleType::Right:
            return "ew-resize";
        case HandleType::Rotate:
            return "crosshair";
        case HandleType::Move:
            return "move";
        default:
            return "default";
    }
}

// Transform gizmo state
struct TransformGizmo {
    Rect bounds;              // Bounding box in world space
    float rotation = 0;       // Current rotation (degrees)
    Point pivot;              // Transform pivot point (world space)
    bool visible = false;

    // Handle positions in world space
    Point handles[9];         // 8 scale handles + 1 rotation handle

    // Screen-space sizes (constant regardless of zoom)
    float handleSize = 8.0f;
    float rotateHandleOffset = 25.0f;

    // Update handle positions from bounds (world space)
    void updateHandles() {
        float cx = bounds.x + bounds.width / 2;
        float cy = bounds.y + bounds.height / 2;

        // Corner and edge handles (before rotation)
        Point corners[8] = {
            {bounds.x, bounds.y},                           // TopLeft
            {bounds.x + bounds.width, bounds.y},            // TopRight
            {bounds.x, bounds.y + bounds.height},           // BottomLeft
            {bounds.x + bounds.width, bounds.y + bounds.height}, // BottomRight
            {cx, bounds.y},                                 // Top
            {bounds.x + bounds.width, cy},                  // Right
            {cx, bounds.y + bounds.height},                 // Bottom
            {bounds.x, cy},                                 // Left
        };

        // Rotation handle offset - we'll compute actual position during render
        // Store a "prototype" position that will be adjusted by view scale
        Point rotHandle = {cx, bounds.y};  // Top center, offset added during render

        // Apply rotation around center
        Transform2D rot = Transform2D::rotationAround(
            rotation * 3.14159f / 180.0f, {cx, cy});

        for (int i = 0; i < 8; ++i) {
            handles[i] = rot.apply(corners[i]);
        }
        handles[8] = rot.apply(rotHandle);  // Will be offset in screen space

        pivot = {cx, cy};
    }

    // Hit test handles using world-space point
    // viewScale = 1/zoom for converting screen sizes to world sizes
    HandleType hitTest(const Point& worldPoint, float viewScale) const {
        if (!visible) return HandleType::None;

        float threshold = handleSize * 1.5f * viewScale;  // Screen-space threshold converted to world

        // Calculate rotation handle position with offset
        float rotOffset = rotateHandleOffset * viewScale;
        Point rotHandle = {
            handles[8].x + (handles[8].x - pivot.x) * 0,  // Already at top center
            handles[8].y - rotOffset  // Offset above
        };

        // Re-apply rotation to the offset handle
        float rad = rotation * 3.14159f / 180.0f;
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);
        float dx = handles[4].x - pivot.x;  // Top center offset
        float dy = -rotOffset;
        rotHandle = {
            pivot.x + dx * cosR - dy * sinR,
            pivot.y + dx * sinR + dy * cosR
        };

        // Check rotation handle first
        float rdx = worldPoint.x - rotHandle.x;
        float rdy = worldPoint.y - rotHandle.y;
        if (rdx * rdx + rdy * rdy < threshold * threshold) {
            return HandleType::Rotate;
        }

        // Check corner handles
        HandleType types[] = {
            HandleType::TopLeft, HandleType::TopRight,
            HandleType::BottomLeft, HandleType::BottomRight,
            HandleType::Top, HandleType::Right,
            HandleType::Bottom, HandleType::Left
        };

        for (int i = 0; i < 8; ++i) {
            float hdx = worldPoint.x - handles[i].x;
            float hdy = worldPoint.y - handles[i].y;
            if (hdx * hdx + hdy * hdy < threshold * threshold) {
                return types[i];
            }
        }

        // Check if inside bounds (for move)
        if (bounds.contains(worldPoint)) {
            return HandleType::Move;
        }

        return HandleType::None;
    }

    // Render the gizmo
    // The renderer is already in world-space coordinates (camera transform applied)
    void render(flex::Renderer& renderer, const ViewTransform& view) const {
        if (!visible) return;

        float viewScale = view.screenToWorldScale();
        float lineWidth = 1.5f * viewScale;
        float hs = handleSize * viewScale;
        float rotOffset = rotateHandleOffset * viewScale;

        // Selection color
        flex::Color selectColor = {0.0f, 0.6f, 1.0f, 1.0f};
        flex::Color handleFill = {1.0f, 1.0f, 1.0f, 1.0f};
        flex::Color rotateFill = {0.4f, 0.8f, 0.4f, 1.0f};

        // Draw bounding box (rotated)
        std::string boxPath =
            "M " + std::to_string(handles[0].x) + " " + std::to_string(handles[0].y) +
            " L " + std::to_string(handles[1].x) + " " + std::to_string(handles[1].y) +
            " L " + std::to_string(handles[3].x) + " " + std::to_string(handles[3].y) +
            " L " + std::to_string(handles[2].x) + " " + std::to_string(handles[2].y) +
            " Z";
        renderer.stroke_path(boxPath, flex::Paint::solid(selectColor), lineWidth);

        // Calculate rotation handle with offset
        Point topMid = {(handles[0].x + handles[1].x) / 2, (handles[0].y + handles[1].y) / 2};
        float rad = rotation * 3.14159f / 180.0f;
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);

        // Offset perpendicular to top edge (outward)
        Point rotHandle = {
            topMid.x - rotOffset * sinR,
            topMid.y - rotOffset * cosR
        };

        // Draw line to rotation handle
        std::string rotLine =
            "M " + std::to_string(topMid.x) + " " + std::to_string(topMid.y) +
            " L " + std::to_string(rotHandle.x) + " " + std::to_string(rotHandle.y);
        renderer.stroke_path(rotLine, flex::Paint::solid(selectColor), lineWidth);

        // Draw corner handles (squares)
        auto drawSquareHandle = [&](const Point& pos, const flex::Color& fill) {
            float x = pos.x - hs / 2;
            float y = pos.y - hs / 2;
            std::string path =
                "M " + std::to_string(x) + " " + std::to_string(y) +
                " h " + std::to_string(hs) +
                " v " + std::to_string(hs) +
                " h " + std::to_string(-hs) + " Z";
            renderer.fill_path(path, flex::Paint::solid(fill));
            renderer.stroke_path(path, flex::Paint::solid(selectColor), lineWidth);
        };

        // Draw circular handle
        auto drawCircleHandle = [&](const Point& pos, const flex::Color& fill) {
            float r = hs / 2;
            float k = 0.5522848f * r;
            std::string path =
                "M " + std::to_string(pos.x) + " " + std::to_string(pos.y - r) +
                " C " + std::to_string(pos.x + k) + " " + std::to_string(pos.y - r) +
                " " + std::to_string(pos.x + r) + " " + std::to_string(pos.y - k) +
                " " + std::to_string(pos.x + r) + " " + std::to_string(pos.y) +
                " C " + std::to_string(pos.x + r) + " " + std::to_string(pos.y + k) +
                " " + std::to_string(pos.x + k) + " " + std::to_string(pos.y + r) +
                " " + std::to_string(pos.x) + " " + std::to_string(pos.y + r) +
                " C " + std::to_string(pos.x - k) + " " + std::to_string(pos.y + r) +
                " " + std::to_string(pos.x - r) + " " + std::to_string(pos.y + k) +
                " " + std::to_string(pos.x - r) + " " + std::to_string(pos.y) +
                " C " + std::to_string(pos.x - r) + " " + std::to_string(pos.y - k) +
                " " + std::to_string(pos.x - k) + " " + std::to_string(pos.y - r) +
                " " + std::to_string(pos.x) + " " + std::to_string(pos.y - r) + " Z";
            renderer.fill_path(path, flex::Paint::solid(fill));
            renderer.stroke_path(path, flex::Paint::solid(selectColor), lineWidth);
        };

        // Draw corner handles
        for (int i = 0; i < 4; ++i) {
            drawSquareHandle(handles[i], handleFill);
        }

        // Draw edge handles
        for (int i = 4; i < 8; ++i) {
            drawSquareHandle(handles[i], handleFill);
        }

        // Draw rotation handle (circle)
        drawCircleHandle(rotHandle, rotateFill);
    }

    // Legacy render method for backwards compatibility
    void render(flex::Renderer& renderer, float zoom) const {
        ViewTransform view;
        view.setZoom(zoom);
        render(renderer, view);
    }
};

} // namespace editor
