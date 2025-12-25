/*
 * Flex Engine - Matrix and Vector Types
 *
 * Lightweight 2D math types, no external dependencies.
 * Optimized for 2D graphics rendering.
 */

#pragma once

#include <cmath>

#define DEG_TO_RAD(deg) ((deg) * (3.14159265f / 180.0f))

namespace flex {

// ============================================================================
// Vec2 - 2D Vector
// ============================================================================

struct Vec2 {
    float data[2] = {0, 0};

    Vec2() = default;
    Vec2(float x, float y) : data{x, y} {}

    float x() const { return data[0]; }
    float y() const { return data[1]; }
    float& x() { return data[0]; }
    float& y() { return data[1]; }

    Vec2 operator+(const Vec2& o) const { return {data[0] + o.data[0], data[1] + o.data[1]}; }
    Vec2 operator-(const Vec2& o) const { return {data[0] - o.data[0], data[1] - o.data[1]}; }
    Vec2 operator*(float s) const { return {data[0] * s, data[1] * s}; }
    Vec2& operator+=(const Vec2& o) { data[0] += o.data[0]; data[1] += o.data[1]; return *this; }
    Vec2& operator-=(const Vec2& o) { data[0] -= o.data[0]; data[1] -= o.data[1]; return *this; }

    float dot(const Vec2& o) const { return data[0] * o.data[0] + data[1] * o.data[1]; }
    float length() const { return std::sqrt(data[0] * data[0] + data[1] * data[1]); }
    float length_squared() const { return data[0] * data[0] + data[1] * data[1]; }
    Vec2 normalized() const { float l = length(); return l > 0 ? Vec2{data[0]/l, data[1]/l} : Vec2{}; }

    // Perpendicular vector (90 degrees counter-clockwise)
    Vec2 perp() const { return {-data[1], data[0]}; }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

// ============================================================================
// Vec3 - 3D Vector
// ============================================================================

struct Vec3 {
    float data[3] = {0, 0, 0};

    Vec3() = default;
    Vec3(float x, float y, float z) : data{x, y, z} {}

    float x() const { return data[0]; }
    float y() const { return data[1]; }
    float z() const { return data[2]; }
    float& x() { return data[0]; }
    float& y() { return data[1]; }
    float& z() { return data[2]; }

    Vec3 operator+(const Vec3& o) const { return {data[0] + o.data[0], data[1] + o.data[1], data[2] + o.data[2]}; }
    Vec3 operator-(const Vec3& o) const { return {data[0] - o.data[0], data[1] - o.data[1], data[2] - o.data[2]}; }
    Vec3 operator*(float s) const { return {data[0] * s, data[1] * s, data[2] * s}; }
};

// ============================================================================
// Vec4 - 4D Vector
// ============================================================================

struct Vec4 {
    float data[4] = {0, 0, 0, 0};

    Vec4() = default;
    Vec4(float x, float y, float z, float w) : data{x, y, z, w} {}

    float x() const { return data[0]; }
    float y() const { return data[1]; }
    float z() const { return data[2]; }
    float w() const { return data[3]; }
    float& x() { return data[0]; }
    float& y() { return data[1]; }
    float& z() { return data[2]; }
    float& w() { return data[3]; }
};

// ============================================================================
// Transform - 2D Affine Transform (3x3 matrix, row-major, bottom row implicit)
// ============================================================================
// Matrix layout: [ m[0] m[1] m[2] ]   [ sx*cos  -sy*sin  tx ]
//                [ m[3] m[4] m[5] ] = [ sx*sin   sy*cos  ty ]
//                [  0    0    1   ]   [   0        0      1 ]

struct Transform {
    float m[6] = {1, 0, 0, 0, 1, 0};  // Identity by default

    Transform() = default;

    static Transform Identity() { return Transform{}; }

    // Fast identity check - used to skip unnecessary matrix operations
    bool is_identity() const {
        return m[0] == 1.0f && m[1] == 0.0f && m[2] == 0.0f &&
               m[3] == 0.0f && m[4] == 1.0f && m[5] == 0.0f;
    }

    // Check if transform is translation-only (no rotation/scale)
    bool is_translation_only() const {
        return m[0] == 1.0f && m[1] == 0.0f &&
               m[3] == 0.0f && m[4] == 1.0f;
    }

    // Check if transform has rotation
    bool has_rotation() const {
        return m[1] != 0.0f || m[3] != 0.0f;
    }

    // Matrix element access (row, col) for compatibility
    float operator()(int row, int col) const {
        if (row == 2) return (col == 2) ? 1.0f : 0.0f;
        return m[row * 3 + col];
    }

    // Direct element access
    float& at(int row, int col) { return m[row * 3 + col]; }
    float at(int row, int col) const { return m[row * 3 + col]; }

    // Translation component
    float tx() const { return m[2]; }
    float ty() const { return m[5]; }

    // Apply transform to point: result = M * p
    Vec2 operator*(const Vec2& p) const {
        // Fast path: translation only
        if (is_translation_only()) {
            return Vec2(p.x() + m[2], p.y() + m[5]);
        }
        return Vec2(m[0] * p.x() + m[1] * p.y() + m[2],
                    m[3] * p.x() + m[4] * p.y() + m[5]);
    }

    // Compose transforms: result = this * other
    Transform operator*(const Transform& o) const {
        // Fast path: identity checks
        if (is_identity()) return o;
        if (o.is_identity()) return *this;

        // Fast path: both translation-only
        if (is_translation_only() && o.is_translation_only()) {
            Transform r;
            r.m[2] = m[2] + o.m[2];
            r.m[5] = m[5] + o.m[5];
            return r;
        }

        // General case
        Transform r;
        r.m[0] = m[0] * o.m[0] + m[1] * o.m[3];
        r.m[1] = m[0] * o.m[1] + m[1] * o.m[4];
        r.m[2] = m[0] * o.m[2] + m[1] * o.m[5] + m[2];
        r.m[3] = m[3] * o.m[0] + m[4] * o.m[3];
        r.m[4] = m[3] * o.m[1] + m[4] * o.m[4];
        r.m[5] = m[3] * o.m[2] + m[4] * o.m[5] + m[5];
        return r;
    }

    // Inverse transform
    Transform inverse() const {
        float det = m[0] * m[4] - m[1] * m[3];
        if (std::abs(det) < 1e-10f) return Identity();
        float inv_det = 1.0f / det;
        Transform r;
        r.m[0] = m[4] * inv_det;
        r.m[1] = -m[1] * inv_det;
        r.m[2] = (m[1] * m[5] - m[4] * m[2]) * inv_det;
        r.m[3] = -m[3] * inv_det;
        r.m[4] = m[0] * inv_det;
        r.m[5] = (m[3] * m[2] - m[0] * m[5]) * inv_det;
        return r;
    }

    // In-place translation (post-multiply)
    void translate(float x, float y) {
        m[2] += x;
        m[5] += y;
    }

    // In-place rotation (post-multiply by rotation matrix)
    void rotate(float rad) {
        if (rad == 0.0f) return;
        float c = std::cos(rad);
        float s = std::sin(rad);
        float m00 = m[0], m01 = m[1];
        float m10 = m[3], m11 = m[4];
        m[0] = m00 * c + m01 * s;
        m[1] = -m00 * s + m01 * c;
        m[3] = m10 * c + m11 * s;
        m[4] = -m10 * s + m11 * c;
    }

    // In-place scale (post-multiply by scale matrix)
    void scale(float sx, float sy) {
        m[0] *= sx; m[1] *= sy;
        m[3] *= sx; m[4] *= sy;
    }

    // Set to translation matrix
    void set_translation(float x, float y) {
        m[0] = 1; m[1] = 0; m[2] = x;
        m[3] = 0; m[4] = 1; m[5] = y;
    }

    // Set to rotation matrix (around origin)
    void set_rotation(float rad) {
        float c = std::cos(rad);
        float s = std::sin(rad);
        m[0] = c;  m[1] = -s; m[2] = 0;
        m[3] = s;  m[4] = c;  m[5] = 0;
    }

    // Set to scale matrix
    void set_scale(float sx, float sy) {
        m[0] = sx; m[1] = 0;  m[2] = 0;
        m[3] = 0;  m[4] = sy; m[5] = 0;
    }
};

// ============================================================================
// Transform Factory Functions
// ============================================================================

// Create transform from components: translate(x,y) * rotate(rad) * scale(sx,sy)
// Matrix form: [ sx*cos  -sy*sin   x ]
//              [ sx*sin   sy*cos   y ]
inline Transform create_transform(float x, float y, float rotation_deg, float scale_x, float scale_y) {
    Transform t;
    if (rotation_deg == 0.0f) {
        // Fast path: no rotation (very common case)
        t.m[0] = scale_x; t.m[1] = 0;       t.m[2] = x;
        t.m[3] = 0;       t.m[4] = scale_y; t.m[5] = y;
    } else {
        float rad = DEG_TO_RAD(rotation_deg);
        float c = std::cos(rad);
        float s = std::sin(rad);
        t.m[0] = scale_x * c;  t.m[1] = -scale_y * s; t.m[2] = x;
        t.m[3] = scale_x * s;  t.m[4] = scale_y * c;  t.m[5] = y;
    }
    return t;
}

// Create translation-only transform
inline Transform make_translation(float x, float y) {
    Transform t;
    t.m[2] = x;
    t.m[5] = y;
    return t;
}

// Create rotation-only transform (degrees)
inline Transform make_rotation(float degrees) {
    Transform t;
    float rad = DEG_TO_RAD(degrees);
    float c = std::cos(rad);
    float s = std::sin(rad);
    t.m[0] = c;  t.m[1] = -s;
    t.m[3] = s;  t.m[4] = c;
    return t;
}

// Create scale-only transform
inline Transform make_scale(float sx, float sy) {
    Transform t;
    t.m[0] = sx;
    t.m[4] = sy;
    return t;
}

} // namespace flex
