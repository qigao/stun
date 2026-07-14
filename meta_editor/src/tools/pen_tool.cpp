/*
 * Meta Editor - Pen Tool Implementation
 *
 * Bezier path drawing:
 * - Click = corner point
 * - Click+drag = smooth point with symmetric handles
 * - Click near first point = close path
 * - Enter = finish path
 * - Escape = cancel
 */

#include "meta_editor/tools/pen_tool.h"
#include "meta_editor/canvas.h"
#include <cmath>

namespace meta_editor {

PenTool::PenTool() {}

void PenTool::activate() {
    is_drawing_ = false;
    is_dragging_ = false;
    path_.points.clear();
    path_.closed = false;
}

void PenTool::deactivate() {
    if (is_drawing_) {
        cancel_path();
    }
}

bool PenTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    // Check if clicking near first point to close path
    if (is_drawing_ && path_.points.size() >= 2 && is_near_first_point(world_pos)) {
        close_path();
        return true;
    }

    // Start new point
    drag_start_ = world_pos;
    is_dragging_ = true;

    // Add new point
    PathEditPoint pt(world_pos);
    pt.type = PointType::Corner;  // Will become Smooth if user drags
    path_.points.push_back(pt);

    is_drawing_ = true;
    current_point_ = world_pos;

    return true;
}

bool PenTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    current_point_ = world_pos;

    if (is_dragging_ && !path_.points.empty()) {
        // User is dragging - create handles
        auto& pt = path_.points.back();
        flex::Vec2 delta(world_pos.x - drag_start_.x, world_pos.y - drag_start_.y);

        // Create symmetric handles
        pt.handle_out = delta;
        pt.handle_in = flex::Vec2(-delta.x, -delta.y);
        pt.type = PointType::Symmetric;

        return true;
    }

    return is_drawing_;
}

bool PenTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (is_dragging_) {
        is_dragging_ = false;

        // If barely dragged, keep as corner point
        if (!path_.points.empty()) {
            auto& pt = path_.points.back();
            float drag_dist = std::sqrt(
                pt.handle_out.x * pt.handle_out.x +
                pt.handle_out.y * pt.handle_out.y
            );
            if (drag_dist < 5.0f) {
                pt.handle_in = flex::Vec2(0, 0);
                pt.handle_out = flex::Vec2(0, 0);
                pt.type = PointType::Corner;
            }
        }
        return true;
    }
    return false;
}

bool PenTool::on_key_down(int key, int mods) {
    // Enter to finish, Escape to cancel
    if (key == 13 || key == '\r') {  // Enter
        finish_path();
        return true;
    } else if (key == 27) {  // Escape
        cancel_path();
        return true;
    }
    return false;
}

void PenTool::render_overlay(flex::Renderer& renderer) {
    if (!is_drawing_ || path_.points.empty()) return;

    // Colors
    flex::Paint path_stroke = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 0.9f));
    flex::Paint point_fill = flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
    flex::Paint point_stroke = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint handle_stroke = flex::Paint::solid(flex::Color(0.6f, 0.6f, 0.6f, 0.8f));
    flex::Paint handle_fill = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint close_indicator = flex::Paint::solid(flex::Color(0.2f, 0.8f, 0.2f, 0.5f));

    // Build and draw the path preview
    std::string preview_path = path_.to_svg_path();

    // Add preview line to current mouse position
    if (!is_dragging_ && !path_.points.empty()) {
        const auto& last = path_.points.back();
        if (last.has_handle_out()) {
            // Curve preview (use handle_out reflected as control point)
            auto c1 = last.handle_out_abs();
            preview_path += " C " + std::to_string(c1.x) + " " + std::to_string(c1.y) +
                           " " + std::to_string(current_point_.x) + " " + std::to_string(current_point_.y) +
                           " " + std::to_string(current_point_.x) + " " + std::to_string(current_point_.y);
        } else {
            preview_path += " L " + std::to_string(current_point_.x) + " " + std::to_string(current_point_.y);
        }
    }

    renderer.stroke_path(preview_path, path_stroke, 2.0f);

    // Draw handles and points
    for (size_t i = 0; i < path_.points.size(); ++i) {
        const auto& pt = path_.points[i];

        // Draw handle lines
        if (pt.has_handle_in()) {
            auto h = pt.handle_in_abs();
            std::string line = "M " + std::to_string(pt.position.x) + " " + std::to_string(pt.position.y) +
                              " L " + std::to_string(h.x) + " " + std::to_string(h.y);
            renderer.stroke_path(line, handle_stroke, 1.0f);
            renderer.draw_circle(h.x, h.y, 3.0f, handle_fill, handle_stroke, 1.0f);
        }
        if (pt.has_handle_out()) {
            auto h = pt.handle_out_abs();
            std::string line = "M " + std::to_string(pt.position.x) + " " + std::to_string(pt.position.y) +
                              " L " + std::to_string(h.x) + " " + std::to_string(h.y);
            renderer.stroke_path(line, handle_stroke, 1.0f);
            renderer.draw_circle(h.x, h.y, 3.0f, handle_fill, handle_stroke, 1.0f);
        }

        // Draw anchor point
        float pt_radius = (i == 0) ? 5.0f : 4.0f;
        renderer.draw_circle(pt.position.x, pt.position.y, pt_radius, point_fill, point_stroke, 1.5f);
    }

    // Show close indicator when near first point
    if (path_.points.size() >= 2 && is_near_first_point(current_point_)) {
        const auto& first = path_.points.front();
        renderer.draw_circle(first.position.x, first.position.y, 8.0f, close_indicator, point_stroke, 2.0f);
    }
}

void PenTool::finish_path() {
    if (path_.points.size() < 2) {
        cancel_path();
        return;
    }

    // Create path shape
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();

    auto* shape = flex::PathShape::create(*allocator);
    shape->set_path_data(path_.to_svg_path());
    shape->set_stroke(flex::Color::Black, 2.0f);
    shape->set_rough(flex::RoughOptions::sketch());  // Hand-drawn style

    if (path_.closed) {
        shape->set_fill(flex::Color(0.5f, 0.7f, 0.9f, 0.5f));
    }

    layer->add_child(shape);

    // TODO: Create CreateShapeCommand for undo

    // Reset
    is_drawing_ = false;
    is_dragging_ = false;
    path_.points.clear();
    path_.closed = false;
}

void PenTool::cancel_path() {
    is_drawing_ = false;
    is_dragging_ = false;
    path_.points.clear();
    path_.closed = false;
}

void PenTool::close_path() {
    path_.closed = true;
    finish_path();
}

bool PenTool::is_near_first_point(const flex::Vec2& pos, float threshold) const {
    if (path_.points.empty()) return false;

    const auto& first = path_.points.front();
    float dx = pos.x - first.position.x;
    float dy = pos.y - first.position.y;
    return (dx * dx + dy * dy) < (threshold * threshold);
}

} // namespace meta_editor
