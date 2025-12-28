/*
 * Meta Editor - Path Editing Structures
 *
 * PathEditPoint represents a bezier curve control point with handles.
 */

#pragma once

#include <flex/runtime/types.h>
#include <vector>
#include <string>

namespace meta_editor {

/**
 * Point type determines how handles behave when one is moved.
 */
enum class PointType {
    Corner,     // Handles are independent
    Smooth,     // Handles stay collinear but can have different lengths
    Symmetric   // Handles are collinear and same length
};

/**
 * A single point on a bezier path with optional control handles.
 */
struct PathEditPoint {
    flex::Vec2 position;     // Anchor point position
    flex::Vec2 handle_in;    // Handle for incoming curve (relative to position)
    flex::Vec2 handle_out;   // Handle for outgoing curve (relative to position)
    PointType type = PointType::Corner;
    bool selected = false;

    PathEditPoint() = default;
    PathEditPoint(float x, float y) : position(x, y) {}
    PathEditPoint(const flex::Vec2& pos) : position(pos) {}

    // Absolute handle positions
    flex::Vec2 handle_in_abs() const { return position + handle_in; }
    flex::Vec2 handle_out_abs() const { return position + handle_out; }

    // Check if point has curve handles
    bool has_handle_in() const { return handle_in.x() != 0 || handle_in.y() != 0; }
    bool has_handle_out() const { return handle_out.x() != 0 || handle_out.y() != 0; }
    bool is_curve_point() const { return has_handle_in() || has_handle_out(); }

    // Apply handle constraints based on point type
    void constrain_handles(bool moved_out) {
        if (type == PointType::Corner) return;

        if (moved_out) {
            // Adjust handle_in based on handle_out
            if (type == PointType::Symmetric) {
                handle_in = flex::Vec2(-handle_out.x(), -handle_out.y());
            } else if (type == PointType::Smooth) {
                float len = std::sqrt(handle_in.x() * handle_in.x() + handle_in.y() * handle_in.y());
                float out_len = std::sqrt(handle_out.x() * handle_out.x() + handle_out.y() * handle_out.y());
                if (out_len > 0.001f) {
                    handle_in = flex::Vec2(
                        -handle_out.x() / out_len * len,
                        -handle_out.y() / out_len * len
                    );
                }
            }
        } else {
            // Adjust handle_out based on handle_in
            if (type == PointType::Symmetric) {
                handle_out = flex::Vec2(-handle_in.x(), -handle_in.y());
            } else if (type == PointType::Smooth) {
                float len = std::sqrt(handle_out.x() * handle_out.x() + handle_out.y() * handle_out.y());
                float in_len = std::sqrt(handle_in.x() * handle_in.x() + handle_in.y() * handle_in.y());
                if (in_len > 0.001f) {
                    handle_out = flex::Vec2(
                        -handle_in.x() / in_len * len,
                        -handle_in.y() / in_len * len
                    );
                }
            }
        }
    }
};

/**
 * A complete path with multiple points.
 */
struct PathData {
    std::vector<PathEditPoint> points;
    bool closed = false;

    // Convert to SVG path string
    std::string to_svg_path() const {
        if (points.empty()) return "";

        std::string path;

        for (size_t i = 0; i < points.size(); ++i) {
            const auto& pt = points[i];

            if (i == 0) {
                // Move to first point
                path += "M " + std::to_string(pt.position.x()) + " " + std::to_string(pt.position.y());
            } else {
                const auto& prev = points[i - 1];

                if (prev.has_handle_out() || pt.has_handle_in()) {
                    // Cubic bezier curve
                    auto c1 = prev.handle_out_abs();
                    auto c2 = pt.handle_in_abs();
                    path += " C " + std::to_string(c1.x()) + " " + std::to_string(c1.y()) +
                            " " + std::to_string(c2.x()) + " " + std::to_string(c2.y()) +
                            " " + std::to_string(pt.position.x()) + " " + std::to_string(pt.position.y());
                } else {
                    // Straight line
                    path += " L " + std::to_string(pt.position.x()) + " " + std::to_string(pt.position.y());
                }
            }
        }

        if (closed && points.size() > 1) {
            const auto& last = points.back();
            const auto& first = points.front();

            if (last.has_handle_out() || first.has_handle_in()) {
                auto c1 = last.handle_out_abs();
                auto c2 = first.handle_in_abs();
                path += " C " + std::to_string(c1.x()) + " " + std::to_string(c1.y()) +
                        " " + std::to_string(c2.x()) + " " + std::to_string(c2.y()) +
                        " " + std::to_string(first.position.x()) + " " + std::to_string(first.position.y());
            }
            path += " Z";
        }

        return path;
    }

    // Calculate bounding box
    flex::Bounds bounds() const {
        if (points.empty()) return {0, 0, 0, 0};

        float min_x = points[0].position.x();
        float min_y = points[0].position.y();
        float max_x = min_x;
        float max_y = min_y;

        for (const auto& pt : points) {
            min_x = std::min(min_x, pt.position.x());
            min_y = std::min(min_y, pt.position.y());
            max_x = std::max(max_x, pt.position.x());
            max_y = std::max(max_y, pt.position.y());

            // Include handles in bounds
            if (pt.has_handle_in()) {
                auto h = pt.handle_in_abs();
                min_x = std::min(min_x, h.x());
                min_y = std::min(min_y, h.y());
                max_x = std::max(max_x, h.x());
                max_y = std::max(max_y, h.y());
            }
            if (pt.has_handle_out()) {
                auto h = pt.handle_out_abs();
                min_x = std::min(min_x, h.x());
                min_y = std::min(min_y, h.y());
                max_x = std::max(max_x, h.x());
                max_y = std::max(max_y, h.y());
            }
        }

        return {min_x, min_y, max_x - min_x, max_y - min_y};
    }
};

} // namespace meta_editor
