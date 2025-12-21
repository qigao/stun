/*
 * Flex Engine - Core Type Definitions
 *
 * Simplified version - No variant, no complexity
 * Direct use of native types
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <array>

namespace flex {

// ============================================================================
// Simple Value Types - Direct use of native types
// ============================================================================

// No enum class ValueType needed!
// No complex Value union needed!
// Just use native C++ types directly!

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

// Transform matrix [a, b, c, d, e, f]
struct Transform {
    float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    Transform() = default;
    Transform(float a, float b, float c, float d, float e, float f)
        : a(a), b(b), c(c), d(d), e(e), f(f) {}
};

// ============================================================================
// Easing Functions - Simplified
// ============================================================================

enum class EasingType {
    Linear,
    Ease,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicBezier,
};

struct Easing {
    EasingType type = EasingType::Linear;
    float p1 = 0, p2 = 0, p3 = 0, p4 = 0;  // Bezier control points

    static Easing linear() { return {EasingType::Linear}; }
    static Easing ease() { return {EasingType::Ease, 0.25f, 0.1f, 0.25f, 1.0f}; }
    static Easing ease_in() { return {EasingType::EaseIn, 0.42f, 0.0f, 1.0f, 1.0f}; }
    static Easing ease_out() { return {EasingType::EaseOut, 0.0f, 0.0f, 0.58f, 1.0f}; }
    static Easing ease_in_out() { return {EasingType::EaseInOut, 0.42f, 0.0f, 0.58f, 1.0f}; }

    // Evaluate easing at time t [0, 1]
    float evaluate(float t) const {
        // Simple linear interpolation for now
        switch (type) {
            case EasingType::Linear:
                return t;
            case EasingType::Ease:
                return t * t * (3.0f - 2.0f * t);
            case EasingType::EaseIn:
                return t * t;
            case EasingType::EaseOut:
                return t * (2.0f - t);
            case EasingType::EaseInOut:
                return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
            case EasingType::CubicBezier:
                // For simplicity, just return linear for now
                // TODO: Implement proper cubic bezier
                return t;
            default:
                return t;
        }
    }
};

// ============================================================================
// Color Utilities
// ============================================================================

struct Color {
    float r = 0, g = 0, b = 0, a = 1;

    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    // Predefined colors
    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Transparent;

    // Parse from hex string (#RGB, #RRGGBB, #RRGGBBAA)
    static Color from_hex(const char* hex);

    // Convert to packed RGBA (0xRRGGBBAA)
    uint32_t to_rgba32() const;

    // Convert from packed RGBA
    static Color from_rgba32(uint32_t rgba);
};

// Simple implementations for color utilities
inline Color Color::from_hex(const char* hex) {
    // Simple hex parser - expects #RRGGBB format
    if (!hex || hex[0] != '#') return Color::White;

    uint32_t r = 255, g = 255, b = 255;

    int len = strlen(hex);
    if (len == 7) {  // #RRGGBB
        sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    }

    return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

inline uint32_t Color::to_rgba32() const {
    uint32_t r = static_cast<uint32_t>(r * 255);
    uint32_t g = static_cast<uint32_t>(g * 255);
    uint32_t b = static_cast<uint32_t>(b * 255);
    uint32_t a = static_cast<uint32_t>(a * 255);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

inline Color Color::from_rgba32(uint32_t rgba) {
    float r = ((rgba >> 24) & 0xFF) / 255.0f;
    float g = ((rgba >> 16) & 0xFF) / 255.0f;
    float b = ((rgba >> 8) & 0xFF) / 255.0f;
    float a = (rgba & 0xFF) / 255.0f;
    return Color(r, g, b, a);
}

// Predefined colors - defined inline
inline const Color Color::White(1.0f, 1.0f, 1.0f, 1.0f);
inline const Color Color::Black(0.0f, 0.0f, 0.0f, 1.0f);
inline const Color Color::Red(1.0f, 0.0f, 0.0f, 1.0f);
inline const Color Color::Green(0.0f, 1.0f, 0.0f, 1.0f);
inline const Color Color::Blue(0.0f, 0.0f, 1.0f, 1.0f);
inline const Color Color::Yellow(1.0f, 1.0f, 0.0f, 1.0f);
inline const Color Color::Transparent(0.0f, 0.0f, 0.0f, 0.0f);

// ============================================================================
// Gradient Types - Simplified
// ============================================================================

struct ColorStop {
    float offset = 0;  // Position along gradient [0, 1]
    Color color;

    ColorStop() = default;
    ColorStop(float off, const Color& c) : offset(off), color(c) {}
};

enum class GradientType {
    None,
    Linear,
    Radial,
};

struct LinearGradient {
    float x1 = 0, y1 = 0;  // Start point
    float x2 = 1, y2 = 0;  // End point (default horizontal)
    std::vector<ColorStop> stops;

    LinearGradient() = default;
    LinearGradient(float x1_, float y1_, float x2_, float y2_)
        : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}

    void add_stop(float offset, const Color& color) {
        stops.emplace_back(offset, color);
    }
};

struct RadialGradient {
    float cx = 0.5f, cy = 0.5f;  // Center point
    float radius = 0.5f;          // Radius
    float fx = 0.5f, fy = 0.5f;  // Focal point (optional, defaults to center)
    std::vector<ColorStop> stops;

    RadialGradient() = default;
    RadialGradient(float cx_, float cy_, float r_)
        : cx(cx_), cy(cy_), radius(r_), fx(cx_), fy(cy_) {}

    void add_stop(float offset, const Color& color) {
        stops.emplace_back(offset, color);
    }
};

// ============================================================================
// Paint - Unified fill/stroke representation
// ============================================================================

struct Paint {
    enum class Type : uint8_t { Solid, Linear, Radial };

    Type type = Type::Solid;
    Color color;
    LinearGradient linear;
    RadialGradient radial;

    Paint() = default;
    explicit Paint(const Color& c) : type(Type::Solid), color(c) {}
    explicit Paint(const LinearGradient& g) : type(Type::Linear), linear(g) {}
    explicit Paint(const RadialGradient& g) : type(Type::Radial), radial(g) {}

    static Paint solid(const Color& c) { return Paint(c); }
    static Paint solid(uint32_t rgba) { return Paint(Color::from_rgba32(rgba)); }
};

// ============================================================================
// Effect Types - Shadows and Filters
// ============================================================================

struct Shadow {
    float offset_x = 0;     // Horizontal offset
    float offset_y = 0;     // Vertical offset
    float blur = 0;         // Blur radius
    float spread = 0;       // Spread radius (expand/contract)
    Color color{0, 0, 0, 0.5f};  // Shadow color with alpha
    bool inset = false;     // Inner shadow (vs drop shadow)

    Shadow() = default;
    Shadow(float ox, float oy, float b, const Color& c)
        : offset_x(ox), offset_y(oy), blur(b), color(c) {}

    // Factory methods
    static Shadow drop(float ox, float oy, float blur, const Color& c) {
        return Shadow(ox, oy, blur, c);
    }

    static Shadow inner(float ox, float oy, float blur, const Color& c) {
        Shadow s(ox, oy, blur, c);
        s.inset = true;
        return s;
    }

    bool is_none() const { return blur == 0 && offset_x == 0 && offset_y == 0 && spread == 0; }
};

struct BlurFilter {
    float radius = 0;       // Gaussian blur radius

    BlurFilter() = default;
    explicit BlurFilter(float r) : radius(r) {}

    bool is_none() const { return radius <= 0; }
};

// ============================================================================
// Layout Types - Flexbox-style layout
// ============================================================================

enum class LayoutMode : uint8_t {
    None,   // Manual positioning (default)
    Flex,   // Flexbox layout
};

enum class FlexDirection : uint8_t {
    Row,            // Left to right (default)
    RowReverse,     // Right to left
    Column,         // Top to bottom
    ColumnReverse,  // Bottom to top
};

enum class JustifyContent : uint8_t {
    Start,          // Pack items at start (default)
    End,            // Pack items at end
    Center,         // Pack items at center
    SpaceBetween,   // Even spacing, first/last at edges
    SpaceAround,    // Even spacing with half-size at edges
    SpaceEvenly,    // Even spacing including edges
};

enum class AlignItems : uint8_t {
    Start,      // Align at cross-axis start (default)
    End,        // Align at cross-axis end
    Center,     // Align at cross-axis center
    Stretch,    // Stretch to fill cross-axis
};

enum class AlignSelf : uint8_t {
    Auto,       // Use parent's align_items (default)
    Start,
    End,
    Center,
    Stretch,
};

enum class FlexWrap : uint8_t {
    NoWrap,     // Single line (default)
    Wrap,       // Multiple lines
};

// ============================================================================
// Dirty Flags - Track what needs to be updated
// ============================================================================

enum class DirtyFlags : uint32_t {
    None        = 0,
    Transform   = 1 << 0,   // Position, rotation, scale changed
    Visual      = 1 << 1,   // Opacity, visibility, effects changed
    Content     = 1 << 2,   // Geometry, fill, stroke changed (subclass)
    Bounds      = 1 << 3,   // Bounds need recalculation
    Layout      = 1 << 4,   // Layout properties changed
    Children    = 1 << 5,   // Child list changed (Group only)
    All         = 0x3F,     // All flags set
};

// Bitwise operators for DirtyFlags
inline DirtyFlags operator|(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline DirtyFlags operator&(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline DirtyFlags operator~(DirtyFlags a) {
    return static_cast<DirtyFlags>(~static_cast<uint32_t>(a));
}
inline DirtyFlags& operator|=(DirtyFlags& a, DirtyFlags b) {
    a = a | b;
    return a;
}
inline DirtyFlags& operator&=(DirtyFlags& a, DirtyFlags b) {
    a = a & b;
    return a;
}
inline bool has_flag(DirtyFlags flags, DirtyFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Cull Result - Result of culling check
// ============================================================================

enum class CullResult : uint8_t {
    Visible,        // Node is visible
    Hidden,         // Node is not visible (visibility=false)
    Transparent,    // Node is fully transparent (opacity=0)
    OutOfView,      // Node is outside viewport
    Culled,         // Node should be skipped (combined)
};

// ============================================================================
// Geometry Types
// ============================================================================

enum class GeometryType {
    None,
    Rect,
    Circle,
    Ellipse,
    Path,
    Polygon,
    Star,
    Line,
    Ring,
};

// ============================================================================
// Bounds - Simple bounding box
// ============================================================================

struct Bounds {
    float x = 0, y = 0;
    float width = 0, height = 0;

    Bounds() = default;
    Bounds(float x, float y, float w, float h)
        : x(x), y(y), width(w), height(h) {}

    bool contains(float px, float py) const {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }

    bool intersects(const Bounds& other) const {
        return !(x + width < other.x || other.x + other.width < x ||
                 y + height < other.y || other.y + other.height < y);
    }

    bool valid() const {
        return width > 0 && height > 0;
    }
};

} // namespace flex
