/*
 * Transform2D Implementation
 */

#include <editor/core/transform.h>
#include <cmath>
#include <algorithm>

namespace editor {

// ============================================================================
// Transform2D
// ============================================================================

Transform2D Transform2D::identity() {
    return {1, 0, 0, 1, 0, 0};
}

Transform2D Transform2D::translation(float x, float y) {
    return {1, 0, 0, 1, x, y};
}

Transform2D Transform2D::scale(float s) {
    return {s, 0, 0, s, 0, 0};
}

Transform2D Transform2D::scale(float sx, float sy) {
    return {sx, 0, 0, sy, 0, 0};
}

Transform2D Transform2D::scaleAround(float sx, float sy, const Point& center) {
    return translation(center.x, center.y)
        .scaled(sx, sy)
        .translated(-center.x, -center.y);
}

Transform2D Transform2D::rotation(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {c, s, -s, c, 0, 0};
}

Transform2D Transform2D::rotationAround(float radians, const Point& center) {
    return translation(center.x, center.y)
        .rotated(radians)
        .translated(-center.x, -center.y);
}

Transform2D Transform2D::translated(float x, float y) const {
    return *this * translation(x, y);
}

Transform2D Transform2D::scaled(float s) const {
    return *this * scale(s);
}

Transform2D Transform2D::scaled(float sx, float sy) const {
    return *this * scale(sx, sy);
}

Transform2D Transform2D::rotated(float radians) const {
    return *this * rotation(radians);
}

Point Transform2D::apply(const Point& p) const {
    return {
        a * p.x + c * p.y + tx,
        b * p.x + d * p.y + ty
    };
}

void Transform2D::apply(Point* points, size_t count) const {
    for (size_t i = 0; i < count; ++i) {
        points[i] = apply(points[i]);
    }
}

Rect Transform2D::applyToRect(const Rect& r) const {
    Point corners[4] = {
        {r.x, r.y},
        {r.x + r.width, r.y},
        {r.x + r.width, r.y + r.height},
        {r.x, r.y + r.height}
    };

    for (int i = 0; i < 4; ++i) {
        corners[i] = apply(corners[i]);
    }

    float minX = corners[0].x, maxX = corners[0].x;
    float minY = corners[0].y, maxY = corners[0].y;
    for (int i = 1; i < 4; ++i) {
        minX = std::min(minX, corners[i].x);
        maxX = std::max(maxX, corners[i].x);
        minY = std::min(minY, corners[i].y);
        maxY = std::max(maxY, corners[i].y);
    }

    return {minX, minY, maxX - minX, maxY - minY};
}

float Transform2D::determinant() const {
    return a * d - b * c;
}

Transform2D Transform2D::inverse() const {
    float det = determinant();
    if (std::abs(det) < 1e-10f) {
        return identity();
    }

    float invDet = 1.0f / det;
    return {
        d * invDet,
        -b * invDet,
        -c * invDet,
        a * invDet,
        (c * ty - d * tx) * invDet,
        (b * tx - a * ty) * invDet
    };
}

Point Transform2D::applyInverse(const Point& p) const {
    return inverse().apply(p);
}

Transform2D Transform2D::operator*(const Transform2D& o) const {
    return {
        a * o.a + c * o.b,
        b * o.a + d * o.b,
        a * o.c + c * o.d,
        b * o.c + d * o.d,
        a * o.tx + c * o.ty + tx,
        b * o.tx + d * o.ty + ty
    };
}

float Transform2D::scaleX() const {
    return std::sqrt(a * a + b * b);
}

float Transform2D::scaleY() const {
    return std::sqrt(c * c + d * d);
}

float Transform2D::rotationRadians() const {
    return std::atan2(b, a);
}

float Transform2D::rotationDegrees() const {
    return rotationRadians() * 180.0f / 3.14159265f;
}

bool Transform2D::operator==(const Transform2D& o) const {
    return a == o.a && b == o.b && c == o.c && d == o.d && tx == o.tx && ty == o.ty;
}

bool Transform2D::isIdentity() const {
    return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0;
}

// ============================================================================
// ViewTransform
// ============================================================================

void ViewTransform::setPan(float x, float y) {
    pan_ = {x, y};
    updateMatrix();
}

void ViewTransform::setZoom(float z) {
    zoom_ = std::clamp(z, 0.1f, 10.0f);
    updateMatrix();
}

void ViewTransform::pan(float dx, float dy) {
    pan_.x += dx;
    pan_.y += dy;
    updateMatrix();
}

void ViewTransform::zoomTo(float newZoom, const Point& screenCenter) {
    Point worldCenter = screenToWorld(screenCenter);
    zoom_ = std::clamp(newZoom, 0.1f, 10.0f);
    pan_.x = screenCenter.x - worldCenter.x * zoom_;
    pan_.y = screenCenter.y - worldCenter.y * zoom_;
    updateMatrix();
}

void ViewTransform::reset() {
    pan_ = {0, 0};
    zoom_ = 1.0f;
    updateMatrix();
}

Point ViewTransform::screenToWorld(const Point& screen) const {
    return inverse_.apply(screen);
}

Point ViewTransform::worldToScreen(const Point& world) const {
    return matrix_.apply(world);
}

Rect ViewTransform::worldToScreenRect(const Rect& world) const {
    return matrix_.applyToRect(world);
}

Rect ViewTransform::screenToWorldRect(const Rect& screen) const {
    return inverse_.applyToRect(screen);
}

void ViewTransform::updateMatrix() {
    matrix_ = Transform2D::translation(pan_.x, pan_.y).scaled(zoom_);
    inverse_ = matrix_.inverse();
}

} // namespace editor
