/*
 * Flex Engine - Unified Geometry Representation
 *
 * Uses std::variant to store exactly one geometry type.
 * Provides conversion to SVG path for unified rendering.
 */

#pragma once

#define FLEX_PI 3.14159265358979f

#include "flex/core/types.h"  // For RoughOptions
#include <string>
#include <variant>
#include <cmath>
#include <algorithm>
// Use stb_sprintf for faster path generation (2-10x faster than standard snprintf)
#define STB_SPRINTF_NOUNALIGNED  // Better performance on modern CPUs
#include <stb_sprintf.h>

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
    float x = 0;       // Bounds origin x
    float y = 0;       // Bounds origin y
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

// Direction for triangle/arrow shapes
enum class Direction : uint8_t {
    Right = 0,  // Pointing right (play icon)
    Left,       // Pointing left
    Up,         // Pointing up
    Down,       // Pointing down
};

struct TriangleData {
    float width = 0;
    float height = 0;
    Direction direction = Direction::Right;
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
    RingData,
    TriangleData
>;

// ============================================================================
// Geometry to Path Conversion
// ============================================================================

namespace detail {

// ============================================================================
// Rough (Hand-drawn) Style Implementation
// ============================================================================

// Simple random number generator (Linear Congruential Generator)
struct RoughRandom {
    unsigned int state;

    explicit RoughRandom(unsigned int seed) : state(seed == 0 ? 12345 : seed) {}

    float next() {
        state = state * 1103515245 + 12345;
        return static_cast<float>(state & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
    }

    float range(float min, float max) {
        return min + next() * (max - min);
    }
};

// Apply roughness to a line segment using multi-stroke cubic or quadratic curves
// This better simulates the "shaky" hand-drawn look where each edge is drawn multiple times.
inline void rough_line(std::string& path, float x1, float y1, float x2, float y2,
                      const RoughOptions& opts, RoughRandom& rng) {
    // Determine number of strokes (default to 2 if roughness is enabled)
    int strokes = (std::max)(1, opts.stroke_count);
    if (strokes == 1 && opts.roughness > 0) strokes = 2;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);

    // Degenerate case: zero-length line
    if (length < 0.1f) return;

    // Perpendicular vector (normalized)
    float perp_x = -dy / length;
    float perp_y = dx / length;

    char buf[256];
    for (int i = 0; i < strokes; ++i) {
        // Jitter endpoints slightly for each stroke
        float jitter = opts.roughness * 0.3f;
        float sx = x1 + rng.range(-jitter, jitter);
        float sy = y1 + rng.range(-jitter, jitter);
        float ex = x2 + rng.range(-jitter, jitter);
        float ey = y2 + rng.range(-jitter, jitter);

        // Characteristic hand-drawn bow (curvature)
        // Alternating bow direction for multiple strokes creates that "sketchy" overlap
        float bow_max = (std::min)(length * 0.15f, 2.5f) * opts.bowing;
        float bow = (i % 2 == 0) ? rng.range(0.2f, 1.0f) : rng.range(-1.0f, -0.2f);
        bow *= bow_max;

        // Midpoint with bow and additional roughness
        float mx = (sx + ex) * 0.5f + perp_x * bow + rng.range(-opts.roughness, opts.roughness) * 0.5f;
        float my = (sy + ey) * 0.5f + perp_y * bow + rng.range(-opts.roughness, opts.roughness) * 0.5f;

        // Each stroke starts with a MoveTo
        stbsp_snprintf(buf, sizeof(buf), "M %.4g %.4g Q %.4g %.4g %.4g %.4g ",
                       sx, sy, mx, my, ex, ey);
        path += buf;
    }
}

inline std::string rect_to_path(float w, float h, float r) {
    char buf[512];
    if (r <= 0) {
        // Simple rectangle: M 0,0 H w V h H 0 Z
        stbsp_snprintf(buf, sizeof(buf),
            "M 0 0 H %.4g V %.4g H 0 Z", w, h);
    } else {
        // Rounded rectangle with arc corners
        // Clamp radius to half the smaller dimension
        float max_r = (std::min)(w, h) / 2.0f;
        if (r > max_r) r = max_r;

        stbsp_snprintf(buf, sizeof(buf),
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
    stbsp_snprintf(buf, sizeof(buf),
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
    stbsp_snprintf(buf, sizeof(buf),
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

    const float pi = FLEX_PI;
    const float angle_step = 2.0f * pi / sides;
    // Use flat-top orientation for all polygons (top edge is horizontal)
    // This is more intuitive than pointy-top
    const float start_angle = -pi / 2.0f + angle_step / 2.0f;

    char buf[64];
    for (int i = 0; i < sides; ++i) {
        float angle = start_angle + i * angle_step;
        float x = r * std::cos(angle);
        float y = r * std::sin(angle);

        if (i == 0) {
            stbsp_snprintf(buf, sizeof(buf), "M %.4g %.4g ", x, y);
        } else {
            stbsp_snprintf(buf, sizeof(buf), "L %.4g %.4g ", x, y);
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

    const float pi = FLEX_PI;
    const float angle_step = pi / points;  // Half step between outer and inner
    const float start_angle = -pi / 2.0f;  // Start at top

    char buf[64];
    for (int i = 0; i < points * 2; ++i) {
        float angle = start_angle + i * angle_step;
        float r = (i % 2 == 0) ? outer_r : inner_r;
        float x = r * std::cos(angle);
        float y = r * std::sin(angle);

        if (i == 0) {
            stbsp_snprintf(buf, sizeof(buf), "M %.4g %.4g ", x, y);
        } else {
            stbsp_snprintf(buf, sizeof(buf), "L %.4g %.4g ", x, y);
        }
        path += buf;
    }
    path += "Z";
    return path;
}

inline std::string line_to_path(float x2, float y2) {
    char buf[128];
    stbsp_snprintf(buf, sizeof(buf), "M 0 0 L %.4g %.4g", x2, y2);
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
    stbsp_snprintf(buf, sizeof(buf),
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

inline std::string triangle_to_path(float w, float h, Direction dir) {
    if (w <= 0 || h <= 0) return "";
    char buf[256];

    // Triangle centered at origin with specified width/height
    // Direction determines which way the triangle points
    switch (dir) {
        case Direction::Right:
            // Pointing right: tip at (w/2, 0), base at left
            stbsp_snprintf(buf, sizeof(buf),
                "M %.4g 0 L %.4g %.4g L %.4g %.4g Z",
                w / 2, -w / 2, -h / 2, -w / 2, h / 2);
            break;
        case Direction::Left:
            // Pointing left: tip at (-w/2, 0), base at right
            stbsp_snprintf(buf, sizeof(buf),
                "M %.4g 0 L %.4g %.4g L %.4g %.4g Z",
                -w / 2, w / 2, -h / 2, w / 2, h / 2);
            break;
        case Direction::Up:
            // Pointing up: tip at (0, -h/2), base at bottom
            stbsp_snprintf(buf, sizeof(buf),
                "M 0 %.4g L %.4g %.4g L %.4g %.4g Z",
                -h / 2, -w / 2, h / 2, w / 2, h / 2);
            break;
        case Direction::Down:
            // Pointing down: tip at (0, h/2), base at top
            stbsp_snprintf(buf, sizeof(buf),
                "M 0 %.4g L %.4g %.4g L %.4g %.4g Z",
                h / 2, -w / 2, -h / 2, w / 2, -h / 2);
            break;
    }
    return buf;
}

} // namespace detail

// ============================================================================
// Rough (Hand-drawn) Style Functions
// ============================================================================

// Rough rectangle
inline std::string rect_to_path_rough(float w, float h, float r, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::rect_to_path(w, h, r);

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(2048);

    if (r <= 0) {
        // Simple rectangle with 4 rough multi-strokes
        detail::rough_line(path, 0, 0, w, 0, opts, rng);      // Top
        detail::rough_line(path, w, 0, w, h, opts, rng);      // Right
        detail::rough_line(path, w, h, 0, h, opts, rng);      // Bottom
        detail::rough_line(path, 0, h, 0, 0, opts, rng);      // Left
    } else {
        // Rounded corners approximated with rough line segments
        float max_r = (std::min)(w, h) / 2.0f;
        if (r > max_r) r = max_r;

        detail::rough_line(path, r, 0, w - r, 0, opts, rng);    // Top
        detail::rough_line(path, w - r, 0, w, r, opts, rng);    // Top-right corner
        detail::rough_line(path, w, r, w, h - r, opts, rng);    // Right
        detail::rough_line(path, w, h - r, w - r, h, opts, rng); // Bottom-right corner
        detail::rough_line(path, w - r, h, r, h, opts, rng);    // Bottom
        detail::rough_line(path, r, h, 0, h - r, opts, rng);    // Bottom-left corner
        detail::rough_line(path, 0, h - r, 0, r, opts, rng);    // Left
        detail::rough_line(path, 0, r, r, 0, opts, rng);        // Top-left corner
    }

    return path;
}

// Rough circle (using 4 Bezier segments, multi-stroke)
inline std::string circle_to_path_rough(float r, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::circle_to_path(r);
    if (r <= 0) return "";

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(1024);

    int strokes = (std::max)(2, opts.stroke_count);
    const float kappa = 0.55228f;
    const float pi = FLEX_PI;

    for (int s = 0; s < strokes; ++s) {
        // Randomize circle parameters for each stroke
        float r_off = opts.roughness * 0.5f;
        float current_r = r + rng.range(-r_off, r_off);
        float current_cx = rng.range(-r_off, r_off);
        float current_cy = rng.range(-r_off, r_off);
        float start_angle = rng.range(0, pi * 2);

        char buf[128];
        float px = current_cx + current_r * std::cos(start_angle);
        float py = current_cy + current_r * std::sin(start_angle);
        stbsp_snprintf(buf, sizeof(buf), "M %.4g %.4g ", px, py);
        path += buf;

        for (int i = 1; i <= 4; i++) {
            float a1 = start_angle + (i - 1) * (pi / 2);
            float a2 = start_angle + i * (pi / 2);

            float p1x = current_cx + current_r * std::cos(a1);
            float p1y = current_cy + current_r * std::sin(a1);
            float p3x = current_cx + current_r * std::cos(a2);
            float p3y = current_cy + current_r * std::sin(a2);

            float dx1 = -current_r * std::sin(a1) * kappa;
            float dy1 = current_r * std::cos(a1) * kappa;
            float dx2 = -current_r * std::sin(a2) * kappa;
            float dy2 = current_r * std::cos(a2) * kappa;

            float cp1x = p1x + dx1 + rng.range(-opts.roughness, opts.roughness);
            float cp1y = p1y + dy1 + rng.range(-opts.roughness, opts.roughness);
            float cp2x = p3x - dx2 + rng.range(-opts.roughness, opts.roughness);
            float cp2y = p3y - dy2 + rng.range(-opts.roughness, opts.roughness);

            // Slightly overshoot last segment
            if (i == 4) {
                float os = rng.range(-0.1f, 0.2f);
                if (os > 0) {
                    p3x += (-current_r * std::sin(a2)) * os;
                    p3y += (current_r * std::cos(a2)) * os;
                }
            }

            stbsp_snprintf(buf, sizeof(buf), "C %.4g %.4g %.4g %.4g %.4g %.4g ",
                          cp1x, cp1y, cp2x, cp2y, p3x, p3y);
            path += buf;
        }
    }

    return path;
}

// Rough ellipse (using 4 Bezier segments, multi-stroke)
inline std::string ellipse_to_path_rough(float rx, float ry, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::ellipse_to_path(rx, ry);
    if (rx <= 0 || ry <= 0) return "";

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(1024);

    int strokes = (std::max)(2, opts.stroke_count);
    const float kappa = 0.55228f;
    const float pi = FLEX_PI;

    for (int s = 0; s < strokes; ++s) {
        float r_off = opts.roughness * 0.5f;
        float current_rx = rx + rng.range(-r_off, r_off);
        float current_ry = ry + rng.range(-r_off, r_off);
        float current_cx = rng.range(-r_off, r_off);
        float current_cy = rng.range(-r_off, r_off);
        float start_angle = rng.range(0, pi * 2);

        char buf[128];
        float px = current_cx + current_rx * std::cos(start_angle);
        float py = current_cy + current_ry * std::sin(start_angle);
        stbsp_snprintf(buf, sizeof(buf), "M %.4g %.4g ", px, py);
        path += buf;

        for (int i = 1; i <= 4; i++) {
            float a1 = start_angle + (i - 1) * (pi / 2);
            float a2 = start_angle + i * (pi / 2);

            float p1x = current_cx + current_rx * std::cos(a1);
            float p1y = current_cy + current_ry * std::sin(a1);
            float p3x = current_cx + current_rx * std::cos(a2);
            float p3y = current_cy + current_ry * std::sin(a2);

            float dx1 = -current_rx * std::sin(a1) * kappa;
            float dy1 = current_ry * std::cos(a1) * kappa;
            float dx2 = -current_rx * std::sin(a2) * kappa;
            float dy2 = current_ry * std::cos(a2) * kappa;

            float cp1x = p1x + dx1 + rng.range(-opts.roughness, opts.roughness);
            float cp1y = p1y + dy1 + rng.range(-opts.roughness, opts.roughness);
            float cp2x = p3x - dx2 + rng.range(-opts.roughness, opts.roughness);
            float cp2y = p3y - dy2 + rng.range(-opts.roughness, opts.roughness);

            if (i == 4) {
                float os = rng.range(-0.1f, 0.2f);
                if (os > 0) {
                    p3x += (-current_rx * std::sin(a2)) * os;
                    p3y += (current_ry * std::cos(a2)) * os;
                }
            }

            stbsp_snprintf(buf, sizeof(buf), "C %.4g %.4g %.4g %.4g %.4g %.4g ",
                          cp1x, cp1y, cp2x, cp2y, p3x, p3y);
            path += buf;
        }
    }

    return path;
}

// Rough polygon (multi-stroke)
inline std::string polygon_to_path_rough(int sides, float r, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::polygon_to_path(sides, r);
    if (sides < 3 || r <= 0) return "";

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(sides * 256);

    const float pi = FLEX_PI;
    const float angle_step = 2.0f * pi / sides;
    const float start_angle = -pi / 2.0f + angle_step / 2.0f;

    for (int i = 0; i < sides; ++i) {
        float a1 = start_angle + i * angle_step;
        float a2 = start_angle + (i + 1) * angle_step;

        float x0 = r * std::cos(a1);
        float y0 = r * std::sin(a1);
        float x1 = r * std::cos(a2);
        float y1 = r * std::sin(a2);

        detail::rough_line(path, x0, y0, x1, y1, opts, rng);
    }

    return path;
}

// Rough star (multi-stroke)
inline std::string star_to_path_rough(int points, float outer_r, float inner_r, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::star_to_path(points, outer_r, inner_r);
    if (points < 3 || outer_r <= 0) return "";
    if (inner_r <= 0) inner_r = outer_r * 0.4f;

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(points * 512);

    const float pi = FLEX_PI;
    const float angle_step = pi / points;
    const float start_angle = -pi / 2.0f;

    for (int i = 0; i < points * 2; ++i) {
        float a1 = start_angle + i * angle_step;
        float a2 = start_angle + (i + 1) * angle_step;

        float r1 = (i % 2 == 0) ? outer_r : inner_r;
        float r2 = ((i + 1) % 2 == 0) ? outer_r : inner_r;

        float x0 = r1 * std::cos(a1);
        float y0 = r1 * std::sin(a1);
        float x1 = r2 * std::cos(a2);
        float y1 = r2 * std::sin(a2);

        detail::rough_line(path, x0, y0, x1, y1, opts, rng);
    }

    return path;
}

// Rough line (multi-stroke)
inline std::string line_to_path_rough(float x2, float y2, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::line_to_path(x2, y2);

    detail::RoughRandom rng(opts.seed);
    std::string path;
    path.reserve(512);

    detail::rough_line(path, 0, 0, x2, y2, opts, rng);
    return path;
}

// Rough ring (two multi-stroke rough circles)
inline std::string ring_to_path_rough(float outer_r, float inner_r, const RoughOptions& opts) {
    if (opts.roughness <= 0) return detail::ring_to_path(outer_r, inner_r);
    if (outer_r <= 0) return "";

    if (inner_r <= 0 || inner_r >= outer_r) {
        return circle_to_path_rough(outer_r, opts);
    }

    // Concatenate outer and inner paths.
    // Complex paths use the even-odd fill rule across renderer backends.
    return circle_to_path_rough(outer_r, opts) + " " + circle_to_path_rough(inner_r, opts);
}

namespace detail {

// ============================================================================
// Hachure (Sketchy Fill) Generator
// ============================================================================

// Intersect a horizontal line at y with an edge (x1,y1)-(x2,y2)
inline bool intersect_line_y(float y, float x1, float y1, float x2, float y2, float& out_x) {
    if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
        out_x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
        return true;
    }
    return false;
}

// Generate hachure lines for a polygon
inline void hachure_fill(std::string& path, const std::vector<Vec2>& points, const RoughOptions& opts, RoughRandom& rng) {
    if (points.size() < 3) return;

    float angle_rad = opts.hachure_angle * (3.1415926f / 180.0f);
    float cos_a = std::cos(-angle_rad);
    float sin_a = std::sin(-angle_rad);

    // 1. Rotate points (store as simple floats to avoid Eigen overhead)
    std::vector<std::pair<float, float>> rotated;
    rotated.reserve(points.size());
    float min_y = 1e10f, max_y = -1e10f;
    for (const auto& p : points) {
        float rx = p.x * cos_a - p.y * sin_a;
        float ry = p.x * sin_a + p.y * cos_a;
        rotated.push_back({rx, ry});
        min_y = (std::min)(min_y, ry);
        max_y = (std::max)(max_y, ry);
    }

    // 2. Generate scanlines
    float gap = (std::max)(1.0f, opts.hachure_gap);
    for (float y = min_y + gap; y < max_y; y += gap) {
        std::vector<float> intersections;
        for (size_t i = 0; i < rotated.size(); ++i) {
            size_t j = (i + 1) % rotated.size();
            float ix;
            if (intersect_line_y(y, rotated[i].first, rotated[i].second, rotated[j].first, rotated[j].second, ix)) {
                intersections.push_back(ix);
            }
        }
        std::sort(intersections.begin(), intersections.end());

        // 3. Draw shaky lines between intersection pairs
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            float x1 = intersections[i];
            float x2 = intersections[i+1];

            // Rotate back and draw
            float cos_inv = std::cos(angle_rad);
            float sin_inv = std::sin(angle_rad);

            float sx = x1 * cos_inv - y * sin_inv;
            float sy = x1 * sin_inv + y * cos_inv;
            float ex = x2 * cos_inv - y * sin_inv;
            float ey = x2 * sin_inv + y * cos_inv;

            rough_line(path, sx, sy, ex, ey, opts, rng);
        }
    }
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
        else if constexpr (std::is_same_v<T, TriangleData>) {
            return detail::triangle_to_path(g.width, g.height, g.direction);
        }
        else {
            return "";
        }
    }, geom);
}

// Main conversion function with rough style support
inline std::string geometry_to_path(const Geometry& geom, const RoughOptions& rough) {
    return std::visit([&rough](auto&& g) -> std::string {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, std::monostate>) return "";
        else if constexpr (std::is_same_v<T, RectData>) return rect_to_path_rough(g.width, g.height, g.corner_radius, rough);
        else if constexpr (std::is_same_v<T, CircleData>) return circle_to_path_rough(g.radius, rough);
        else if constexpr (std::is_same_v<T, EllipseData>) return ellipse_to_path_rough(g.rx, g.ry, rough);
        else if constexpr (std::is_same_v<T, PolygonData>) return polygon_to_path_rough(g.sides, g.radius, rough);
        else if constexpr (std::is_same_v<T, PathData>) return g.d;
        else if constexpr (std::is_same_v<T, StarData>) return star_to_path_rough(g.points, g.outer_radius, g.inner_radius, rough);
        else if constexpr (std::is_same_v<T, LineData>) return line_to_path_rough(g.x2, g.y2, rough);
        else if constexpr (std::is_same_v<T, RingData>) return ring_to_path_rough(g.outer_radius, g.inner_radius, rough);
        else if constexpr (std::is_same_v<T, TriangleData>) return detail::triangle_to_path(g.width, g.height, g.direction);
        return "";
    }, geom);
}

// Generate fill path (hachure lines)
inline std::string geometry_to_fill_path(const Geometry& geom, const RoughOptions& rough) {
    if (rough.fill_style == RoughFillStyle::Solid || rough.roughness <= 0) return "";

    return std::visit([&rough](auto&& g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        detail::RoughRandom rng(rough.seed + 5000); // Use different seed for fill
        std::string path;
        std::vector<Vec2> points;

        if constexpr (std::is_same_v<T, RectData>) {
            points = {Vec2(0,0), Vec2(g.width,0), Vec2(g.width, g.height), Vec2(0, g.height)};
        } else if constexpr (std::is_same_v<T, PolygonData>) {
            float angle_step = 2.0f * 3.14159f / g.sides;
            float start_angle = -3.14159f / 2.0f + angle_step / 2.0f;
            for (int i = 0; i < g.sides; ++i) {
                float a = start_angle + i * angle_step;
                points.push_back(Vec2(g.radius * std::cos(a), g.radius * std::sin(a)));
            }
        } else if constexpr (std::is_same_v<T, StarData>) {
            float angle_step = 3.14159f / g.points;
            for (int i = 0; i < g.points * 2; ++i) {
                float r = (i % 2 == 0) ? g.outer_radius : g.inner_radius;
                float a = -3.14159f/2.0f + i * angle_step;
                points.push_back(Vec2(r * std::cos(a), r * std::sin(a)));
            }
        } else if constexpr (std::is_same_v<T, CircleData>) {
            // Approximate circle with 16-sided polygon for hachure
            for (int i = 0; i < 16; ++i) {
                float a = i * (2.0f * 3.14159f / 16.0f);
                points.push_back(Vec2(g.radius * std::cos(a), g.radius * std::sin(a)));
            }
        } else if constexpr (std::is_same_v<T, EllipseData>) {
            for (int i = 0; i < 16; ++i) {
                float a = i * (2.0f * 3.14159f / 16.0f);
                points.push_back(Vec2(g.rx * std::cos(a), g.ry * std::sin(a)));
            }
        }

        if (!points.empty()) {
            detail::hachure_fill(path, points, rough, rng);
            if (rough.fill_style == RoughFillStyle::CrossHatch) {
                RoughOptions opts2 = rough;
                opts2.hachure_angle += 90;
                detail::hachure_fill(path, points, opts2, rng);
            }
        }
        return path;
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
