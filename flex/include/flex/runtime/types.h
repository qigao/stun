/*
 * Flex Engine - Core Type Definitions
 *
 * Simplified version - No variant, no complexity
 * Direct use of native types, no Eigen dependency
 */

#pragma once

#ifdef _WIN32 
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <string_view>

#include "flex/runtime/matrix.h"  // Vec2, Vec3, Vec4, Transform, create_transform

namespace flex {

// ============================================================================
// Hashing & Symbols
// ============================================================================

// FNV-1a 32-bit Hash
inline constexpr uint32_t hash_str(const char* s, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint8_t>(s[i]);
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t hash_str(const char* s) {
    return hash_str(s, std::strlen(s));
}

inline uint32_t hash_str(const std::string& s) {
    return hash_str(s.c_str(), s.size());
}

struct Symbol {
    uint32_t id;

    constexpr Symbol() : id(0) {}
    constexpr Symbol(uint32_t id) : id(id) {}
    Symbol(const char* s) : id(hash_str(s)) {}
    Symbol(const std::string& s) : id(hash_str(s)) {}
    Symbol(std::string_view s) : id(hash_str(s.data(), s.size())) {}

    bool operator==(const Symbol& other) const { return id == other.id; }
    bool operator!=(const Symbol& other) const { return id != other.id; }
};

struct SymbolHash {
    std::size_t operator()(const Symbol& s) const { return s.id; }
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
        switch (type) {
            case EasingType::Linear:
                return t;
            case EasingType::Ease:
            case EasingType::EaseIn:
            case EasingType::EaseOut:
            case EasingType::EaseInOut:
            case EasingType::CubicBezier:
                return cubic_bezier(t, p1, p2, p3, p4);
            default:
                return t;
        }
    }

private:
    // Cubic Bezier implementation (CSS Animations spec compatible)
    // P0=(0,0), P1=(x1,y1), P2=(x2,y2), P3=(1,1)
    static float cubic_bezier(float t, float x1, float y1, float x2, float y2) {
        // Clamp input
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        // Newton-Raphson iteration to solve x(t) = input_t
        float t_guess = t;
        for (int i = 0; i < 8; ++i) {
            float x = bezier_x(t_guess, x1, x2);
            float dx = bezier_x_derivative(t_guess, x1, x2);

            if (dx < 1e-6f) break;

            float x_error = x - t;
            if (x_error < 1e-6f && x_error > -1e-6f) break;

            t_guess -= x_error / dx;
        }

        // Calculate y(t) using the solved t
        return bezier_y(t_guess, y1, y2);
    }

    // Cubic Bezier x component: x(t) = 3(1-t)²t·x1 + 3(1-t)t²·x2 + t³
    static float bezier_x(float t, float x1, float x2) {
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        return 3.0f * mt2 * t * x1 + 3.0f * mt * t2 * x2 + t3;
    }

    // Derivative of x(t)
    static float bezier_x_derivative(float t, float x1, float x2) {
        float mt = 1.0f - t;
        return 3.0f * mt * mt * x1 + 6.0f * mt * t * (x2 - x1) + 3.0f * t * t * (1.0f - x2);
    }

    // Cubic Bezier y component: y(t) = 3(1-t)²t·y1 + 3(1-t)t²·y2 + t³
    static float bezier_y(float t, float y1, float y2) {
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        return 3.0f * mt2 * t * y1 + 3.0f * mt * t2 * y2 + t3;
    }
};

// ============================================================================
// Rough (Hand-drawn) Style Options
// ============================================================================

enum class RoughFillStyle : uint8_t {
    Solid,      // Standard solid fill
    Hachure,    // Sketchy diagonal lines
    ZigZag,     // Shaky zig-zag fill
    CrossHatch  // Crossed sketchy lines
};

struct RoughOptions {
    float roughness = 0;      // 0-10, amount of randomness (0 = disabled)
    float bowing = 1.0f;      // 0-10, curvature of lines
    int stroke_count = 2;     // 1-5, number of strokes per shape
    unsigned int seed = 0;    // Random seed for reproducibility

    // Fill options
    RoughFillStyle fill_style = RoughFillStyle::Solid;
    float fill_weight = 1.0f;      // Width of fill strokes
    float hachure_angle = -45.0f;  // Angle of fill lines
    float hachure_gap = 4.0f;      // Gap between fill lines
    float stroke_width_randomness = 0.0f; // 0-1, adds "salt" to line thicknesses

    // Quick constructors
    static RoughOptions disabled() { return {0, 0, 1, 0}; }
    static RoughOptions sketch() { return {1.5f, 1.0f, 2, 0, RoughFillStyle::Hachure, 1.0f, -45.0f, 4.0f, 0.2f}; }
    static RoughOptions rough() { return {3.0f, 1.5f, 2, 0, RoughFillStyle::Hachure, 1.2f, -45.0f, 5.0f, 0.4f}; }
    static RoughOptions very_rough() { return {5.0f, 2.0f, 3, 0, RoughFillStyle::CrossHatch, 1.5f, -45.0f, 6.0f, 0.6f}; }
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

    // Comparison operators
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

// Simple implementations for color utilities
inline Color Color::from_hex(const char* hex) {
    // Hex parser - supports #RGB, #RRGGBB, #RRGGBBAA formats
    if (!hex || hex[0] != '#') return Color::Black;

    uint32_t r = 0, g = 0, b = 0, a = 255;
    size_t len = strlen(hex) - 1;  // Exclude '#'

    if (len == 6) {  // #RRGGBB
        sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    } else if (len == 8) {  // #RRGGBBAA
        sscanf(hex + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
    } else if (len == 3) {  // #RGB -> expand to #RRGGBB
        unsigned int r4, g4, b4;
        sscanf(hex + 1, "%1x%1x%1x", &r4, &g4, &b4);
        r = r4 * 17;  // 0xF -> 0xFF
        g = g4 * 17;
        b = b4 * 17;
    } else if (len == 4) {  // #RGBA -> expand to #RRGGBBAA
        unsigned int r4, g4, b4, a4;
        sscanf(hex + 1, "%1x%1x%1x%1x", &r4, &g4, &b4, &a4);
        r = r4 * 17;
        g = g4 * 17;
        b = b4 * 17;
        a = a4 * 17;
    }

    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline uint32_t Color::to_rgba32() const {
    uint32_t ri = static_cast<uint32_t>(r * 255);
    uint32_t gi = static_cast<uint32_t>(g * 255);
    uint32_t bi = static_cast<uint32_t>(b * 255);
    uint32_t ai = static_cast<uint32_t>(a * 255);
    return (ri << 24) | (gi << 16) | (bi << 8) | ai;
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
    enum class Type : uint8_t { None, Solid, Linear, Radial };

    Type type = Type::None;
    Color color;
    LinearGradient linear;
    RadialGradient radial;

    Paint() = default;
    explicit Paint(const Color& c) : type(Type::Solid), color(c) {}
    explicit Paint(const LinearGradient& g) : type(Type::Linear), linear(g) {}
    explicit Paint(const RadialGradient& g) : type(Type::Radial), radial(g) {}

    static Paint none() { return Paint(); }
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

// CSS Position property
enum class PositionMode : uint8_t {
    Static,     // Normal flow (default)
    Relative,   // Offset from normal position
    Absolute,   // Removed from flow, relative to positioned ancestor
    Fixed,      // Removed from flow, relative to viewport
};

// CSS box-sizing property
enum class BoxSizing : uint8_t {
    ContentBox,  // width/height = content only (default)
    BorderBox,   // width/height = content + padding + border
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

// Anchor point for positioning - determines which point of the element x,y refers to
enum class Anchor : uint8_t {
    TopLeft,     // Default: x,y is top-left corner
    Top,         // x is center-x, y is top
    TopRight,    // x,y is top-right corner
    Left,        // x is left, y is center-y
    Center,      // x,y is center point
    Right,       // x is right, y is center-y
    BottomLeft,  // x,y is bottom-left corner
    Bottom,      // x is center-x, y is bottom
    BottomRight, // x,y is bottom-right corner
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
    WorldBounds = 1 << 6,   // World-space bounds need recalculation
    All         = 0x7F,     // All flags set
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
// Animation Property IDs - for fast property dispatch
// ============================================================================

enum class PropertyID : uint16_t {
    Unknown = 0,
    // Transform
    X, Y, Rotation, Scale, ScaleX, ScaleY,
    // Size
    Width, Height, Radius,
    // Visual
    Opacity, Visible,
    // Shape
    Fill, FillOpacity, Stroke, StrokeWidth,
    // Text
    Text, Content, FontSize, TextColor,
    // Color (generic)
    Color,
};

// Forward declaration for AnimValue (full definition after Color)
// AnimValue = variant<float, std::string, Color>

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
    Triangle,
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

    // Transform AABB by a 2D affine transform (results in a new AABB)
    Bounds transformed(const Transform& t) const {
        if (!valid()) return *this;

        // Transform 4 corners using direct array access
        float x0 = x, x1 = x + width;
        float y0 = y, y1 = y + height;

        // c0 = transform(x0, y0)
        float c0x = t.m[0] * x0 + t.m[1] * y0 + t.m[2];
        float c0y = t.m[3] * x0 + t.m[4] * y0 + t.m[5];

        // c1 = transform(x1, y0)
        float c1x = t.m[0] * x1 + t.m[1] * y0 + t.m[2];
        float c1y = t.m[3] * x1 + t.m[4] * y0 + t.m[5];

        // c2 = transform(x0, y1)
        float c2x = t.m[0] * x0 + t.m[1] * y1 + t.m[2];
        float c2y = t.m[3] * x0 + t.m[4] * y1 + t.m[5];

        // c3 = transform(x1, y1)
        float c3x = t.m[0] * x1 + t.m[1] * y1 + t.m[2];
        float c3y = t.m[3] * x1 + t.m[4] * y1 + t.m[5];

        // Find min/max
        float min_x = (std::min)({c0x, c1x, c2x, c3x});
        float max_x = (std::max)({c0x, c1x, c2x, c3x});
        float min_y = (std::min)({c0y, c1y, c2y, c3y});
        float max_y = (std::max)({c0y, c1y, c2y, c3y});

        return Bounds(min_x, min_y, max_x - min_x, max_y - min_y);
    }
};

// ============================================================================
// Animation Value - variant type for animated properties
// ============================================================================

} // namespace flex

#include <variant>

namespace flex {

using AnimValue = std::variant<float, std::string, Color>;

// Helper to get PropertyID from string (fast path using first char switch)
PropertyID get_property_id(const char* prop);

} // namespace flex
