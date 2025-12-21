/*
 * Transform2D
 *
 * 2D affine transformation matrix for unified coordinate transforms.
 * Handles translation, scale, rotation with proper matrix composition.
 */

#pragma once

#include "types.h"

namespace editor {

// 2D Affine Transform Matrix
// | a  c  tx |
// | b  d  ty |
// | 0  0  1  |
struct Transform2D {
    float a = 1, b = 0;   // column 1: [a, b]
    float c = 0, d = 1;   // column 2: [c, d]
    float tx = 0, ty = 0; // translation: [tx, ty]

    // Factory methods
    static Transform2D identity();
    static Transform2D translation(float x, float y);
    static Transform2D scale(float s);
    static Transform2D scale(float sx, float sy);
    static Transform2D scaleAround(float sx, float sy, const Point& center);
    static Transform2D rotation(float radians);
    static Transform2D rotationAround(float radians, const Point& center);

    // Chainable operations (return new transform)
    Transform2D translated(float x, float y) const;
    Transform2D scaled(float s) const;
    Transform2D scaled(float sx, float sy) const;
    Transform2D rotated(float radians) const;

    // Apply transform to a point
    Point apply(const Point& p) const;
    void apply(Point* points, size_t count) const;

    // Apply transform to a rectangle (returns axis-aligned bounding box)
    Rect applyToRect(const Rect& r) const;

    // Determinant
    float determinant() const;

    // Inverse transform
    Transform2D inverse() const;
    Point applyInverse(const Point& p) const;

    // Matrix multiplication: this * other
    Transform2D operator*(const Transform2D& o) const;

    // Extract components
    float scaleX() const;
    float scaleY() const;
    float rotationRadians() const;
    float rotationDegrees() const;
    Point translation() const { return {tx, ty}; }

    // Comparison
    bool operator==(const Transform2D& o) const;
    bool isIdentity() const;
};

// View transform - manages viewpoint (pan/zoom) with Transform2D
class ViewTransform {
public:
    // Pan offset
    float panX() const { return pan_.x; }
    float panY() const { return pan_.y; }
    void setPan(float x, float y);

    // Zoom level
    float zoom() const { return zoom_; }
    void setZoom(float z);

    // Pan by delta
    void pan(float dx, float dy);

    // Zoom towards a screen point
    void zoomTo(float newZoom, const Point& screenCenter);

    // Reset to default
    void reset();

    // Get the view matrix (world -> screen)
    const Transform2D& matrix() const { return matrix_; }

    // Get inverse matrix (screen -> world)
    const Transform2D& inverseMatrix() const { return inverse_; }

    // Convert screen coordinates to world
    Point screenToWorld(const Point& screen) const;

    // Convert world coordinates to screen
    Point worldToScreen(const Point& world) const;

    // Convert a world-space rectangle to screen-space bounds
    Rect worldToScreenRect(const Rect& world) const;

    // Convert a screen-space rectangle to world-space bounds
    Rect screenToWorldRect(const Rect& screen) const;

    // Get scale factor for screen-space sizes
    float screenToWorldScale() const { return 1.0f / zoom_; }

private:
    void updateMatrix();

    Point pan_ = {0, 0};
    float zoom_ = 1.0f;
    Transform2D matrix_;
    Transform2D inverse_;
};

} // namespace editor
