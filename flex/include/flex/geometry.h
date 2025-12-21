/*
 * Flex Engine - Unified Geometry Representation
 *
 * Uses std::variant to store exactly one geometry type.
 * Provides conversion to SVG path for unified rendering.
 */

#pragma once

#include <string>
#include <variant>
#include <cstdio>
#include <cmath>

namespace flex {

// ============================================================================
// Geometry Data Types - Compact, single-purpose structs
// ============================================================================

struct RectData {
    float width = 0;
    float height = 0;
    float corner_radius = 0;
};

struct CircleData {
    float radius = 0;
};

struct EllipseData {
    float rx = 0;
    float ry = 0;
};

struct PolygonData {
    int sides = 3;
    float radius = 0;
};

struct PathData {
    std::string d;
    float width = 0;   // Cached bounds (set by user or parsed)
    float height = 0;
};

struct StarData {
    int points = 5;         // Number of points
    float outer_radius = 0; // Outer radius (to points)
    float inner_radius = 0; // Inner radius (between points)
};

struct LineData {
    float x2 = 0;           // End point (start is always 0,0)
    float y2 = 0;
};

struct RingData {
    float outer_radius = 0;
    float inner_radius = 0;
};

// ============================================================================
// Geometry Variant - Only stores ONE geometry type at a time
// ============================================================================

using Geometry = std::variant<
    std::monostate,  // None/empty
    RectData,
    CircleData,
    EllipseData,
    PolygonData,
    PathData,
    StarData,
    LineData,
    RingData
>;

// ============================================================================
// Geometry to Path Conversion
// ============================================================================

namespace detail {

inline std::string rect_to_path(float w, float h, float r) {
    char buf[512];
    if (r <= 0) {
        // Simple rectangle: M 0,0 H w V h H 0 Z
        snprintf(buf, sizeof(buf),
            "M 0 0 H %.4g V %.4g H 0 Z", w, h);
    } else {
        // Rounded rectangle with arc corners
        // Clamp radius to half the smaller dimension
        float max_r = std::min(w, h) / 2.0f;
        if (r > max_r) r = max_r;

        snprintf(buf, sizeof(buf),
            "M %.4g 0 "
            "H %.4g "
            "A %.4g %.4g 0 0 1 %.4g %.4g "
            "V %.4g "
            "A %.4g %.4g 0 0 1 %.4g %.4g "
            "H %.4g "
            "A %.4g %.4g 0 0 1 0 %.4g "
            "V %.4g "
            "A %.4g %.4g 0 0 1 %.4g 0 "
            "Z",
            r,              // M start x
            w - r,          // H to
            r, r, w, r,     // Arc to top-right
            h - r,          // V to
            r, r, w - r, h, // Arc to bottom-right
            r,              // H to
            r, r, h - r,    // Arc to bottom-left
            r,              // V to
            r, r, r         // Arc to top-left
        );
    }
    return buf;
}

inline std::string circle_to_path(float r) {
    if (r <= 0) return "";
    char buf[256];
    // Circle as two arcs: M r,0 A r,r 0 1,1 -r,0 A r,r 0 1,1 r,0 Z
    // Centered at origin
    snprintf(buf, sizeof(buf),
        "M %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "Z",
        r,           // M start
        r, r, -r,    // First arc (top half)
        r, r, r      // Second arc (bottom half)
    );
    return buf;
}

inline std::string ellipse_to_path(float rx, float ry) {
    if (rx <= 0 || ry <= 0) return "";
    char buf[256];
    // Ellipse as two arcs, centered at origin
    snprintf(buf, sizeof(buf),
        "M %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "Z",
        rx,             // M start
        rx, ry, -rx,    // First arc
        rx, ry, rx      // Second arc
    );
    return buf;
}

inline std::string polygon_to_path(int sides, float r) {
    if (sides < 3 || r <= 0) return "";

    std::string path;
    path.reserve(sides * 24);

    const float pi = 3.14159265358979f;
    const float angle_step = 2.0f * pi / sides;
    // Start at top (rotate -90 degrees)
    const float start_angle = -pi / 2.0f;

    char buf[64];
    for (int i = 0; i < sides; ++i) {
        float angle = start_angle + i * angle_step;
        float x = r * std::cos(angle);
        float y = r * std::sin(angle);

        if (i == 0) {
            snprintf(buf, sizeof(buf), "M %.4g %.4g ", x, y);
        } else {
            snprintf(buf, sizeof(buf), "L %.4g %.4g ", x, y);
        }
        path += buf;
    }
    path += "Z";
    return path;
}

inline std::string star_to_path(int points, float outer_r, float inner_r) {
    if (points < 3 || outer_r <= 0) return "";
    if (inner_r <= 0) inner_r = outer_r * 0.4f;  // Default inner radius

    std::string path;
    path.reserve(points * 48);

    const float pi = 3.14159265358979f;
    const float angle_step = pi / points;  // Half step between outer and inner
    const float start_angle = -pi / 2.0f;  // Start at top

    char buf[64];
    for (int i = 0; i < points * 2; ++i) {
        float angle = start_angle + i * angle_step;
        float r = (i % 2 == 0) ? outer_r : inner_r;
        float x = r * std::cos(angle);
        float y = r * std::sin(angle);

        if (i == 0) {
            snprintf(buf, sizeof(buf), "M %.4g %.4g ", x, y);
        } else {
            snprintf(buf, sizeof(buf), "L %.4g %.4g ", x, y);
        }
        path += buf;
    }
    path += "Z";
    return path;
}

inline std::string line_to_path(float x2, float y2) {
    char buf[128];
    snprintf(buf, sizeof(buf), "M 0 0 L %.4g %.4g", x2, y2);
    return buf;
}

inline std::string ring_to_path(float outer_r, float inner_r) {
    if (outer_r <= 0) return "";
    if (inner_r <= 0 || inner_r >= outer_r) {
        // Just a circle
        return circle_to_path(outer_r);
    }

    char buf[512];
    // Outer circle (clockwise) + inner circle (counter-clockwise)
    // This creates a ring using the even-odd fill rule
    snprintf(buf, sizeof(buf),
        "M %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "A %.4g %.4g 0 1 1 %.4g 0 "
        "Z "
        "M %.4g 0 "
        "A %.4g %.4g 0 1 0 %.4g 0 "
        "A %.4g %.4g 0 1 0 %.4g 0 "
        "Z",
        // Outer circle (clockwise, sweep=1)
        outer_r,
        outer_r, outer_r, -outer_r,
        outer_r, outer_r, outer_r,
        // Inner circle (counter-clockwise, sweep=0)
        inner_r,
        inner_r, inner_r, -inner_r,
        inner_r, inner_r, inner_r
    );
    return buf;
}

} // namespace detail

// Main conversion function
inline std::string geometry_to_path(const Geometry& geom) {
    return std::visit([](auto&& g) -> std::string {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "";
        }
        else if constexpr (std::is_same_v<T, RectData>) {
            return detail::rect_to_path(g.width, g.height, g.corner_radius);
        }
        else if constexpr (std::is_same_v<T, CircleData>) {
            return detail::circle_to_path(g.radius);
        }
        else if constexpr (std::is_same_v<T, EllipseData>) {
            return detail::ellipse_to_path(g.rx, g.ry);
        }
        else if constexpr (std::is_same_v<T, PolygonData>) {
            return detail::polygon_to_path(g.sides, g.radius);
        }
        else if constexpr (std::is_same_v<T, PathData>) {
            return g.d;
        }
        else if constexpr (std::is_same_v<T, StarData>) {
            return detail::star_to_path(g.points, g.outer_radius, g.inner_radius);
        }
        else if constexpr (std::is_same_v<T, LineData>) {
            return detail::line_to_path(g.x2, g.y2);
        }
        else if constexpr (std::is_same_v<T, RingData>) {
            return detail::ring_to_path(g.outer_radius, g.inner_radius);
        }
        else {
            return "";
        }
    }, geom);
}

// Helper to check if geometry is empty
inline bool geometry_empty(const Geometry& geom) {
    return std::holds_alternative<std::monostate>(geom);
}

// Helper to get geometry type index (for debugging)
inline size_t geometry_type_index(const Geometry& geom) {
    return geom.index();
}

} // namespace flex
