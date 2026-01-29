/*
 * Meta Editor - Line Tool Implementation
 */

#include "meta_editor/tools/line_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <cmath>

namespace meta_editor {

bool LineTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_drawing_ = true;
    start_pos_ = canvas_->is_snap_to_grid() ? canvas_->snap_to_grid(world_pos) : world_pos;
    current_pos_ = start_pos_;
    return true;
}

bool LineTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) return false;

    flex::Vec2 pos = canvas_->is_snap_to_grid() ? canvas_->snap_to_grid(world_pos) : world_pos;
    current_pos_ = shift_held_ ? constrain_angle(start_pos_, pos) : pos;
    return true;
}

bool LineTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) return false;
    is_drawing_ = false;

    flex::Vec2 pos = canvas_->is_snap_to_grid() ? canvas_->snap_to_grid(world_pos) : world_pos;
    flex::Vec2 end_pos = shift_held_ ? constrain_angle(start_pos_, pos) : pos;

    float dx = end_pos.x() - start_pos_.x();
    float dy = end_pos.y() - start_pos_.y();
    if (std::abs(dx) < 2 && std::abs(dy) < 2) return true;

    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();

    // Build path with arrows
    std::string path_data = "M 0 0 L " + std::to_string(dx) + " " + std::to_string(dy);
    
    if (arrow_style_ == ArrowStyle::End || arrow_style_ == ArrowStyle::Both) {
        path_data += " " + build_arrow_path(flex::Vec2(0, 0), flex::Vec2(dx, dy), arrow_size_);
    }
    if (arrow_style_ == ArrowStyle::Start || arrow_style_ == ArrowStyle::Both) {
        path_data += " " + build_arrow_path(flex::Vec2(dx, dy), flex::Vec2(0, 0), arrow_size_);
    }

    auto* line = flex::Shape::create(*allocator);
    line->set_path(path_data);
    line->set_position(start_pos_.x(), start_pos_.y());
    line->set_stroke(stroke_color_, stroke_width_);
    layer->add_child(line);

    selection_->clear_selection();
    selection_->select(line);

    return true;
}

bool LineTool::on_key_down(int key, int mods) {
    if (key == 340 || key == 344) { // GLFW_KEY_LEFT_SHIFT or RIGHT_SHIFT
        shift_held_ = true;
        return true;
    }
    // 'A' key cycles arrow style
    if (key == 'a' || key == 'A') {
        cycle_arrow_style();
        return true;
    }
    return false;
}

bool LineTool::on_key_up(int key, int mods) {
    if (key == 340 || key == 344) {
        shift_held_ = false;
        return true;
    }
    return false;
}

void LineTool::cycle_arrow_style() {
    switch (arrow_style_) {
        case ArrowStyle::None: arrow_style_ = ArrowStyle::End; break;
        case ArrowStyle::End: arrow_style_ = ArrowStyle::Start; break;
        case ArrowStyle::Start: arrow_style_ = ArrowStyle::Both; break;
        case ArrowStyle::Both: arrow_style_ = ArrowStyle::None; break;
    }
}

void LineTool::render_overlay(flex::Renderer& renderer) {
    if (!is_drawing_) return;

    float dx = current_pos_.x() - start_pos_.x();
    float dy = current_pos_.y() - start_pos_.y();
    if (std::abs(dx) < 1 && std::abs(dy) < 1) return;

    flex::Paint stroke = flex::Paint::solid(flex::Color(0.2f, 0.6f, 0.9f, 0.8f));
    flex::Paint point_fill = flex::Paint::solid(flex::Color(1.0f, 0.4f, 0.4f, 1.0f));

    // Draw line
    char path[128];
    snprintf(path, sizeof(path), "M %.1f %.1f L %.1f %.1f",
             start_pos_.x(), start_pos_.y(), current_pos_.x(), current_pos_.y());
    renderer.stroke_path(path, stroke, 2.0f);

    // Draw arrows preview
    if (arrow_style_ == ArrowStyle::End || arrow_style_ == ArrowStyle::Both) {
        std::string arrow = build_arrow_path(start_pos_, current_pos_, arrow_size_);
        renderer.stroke_path(arrow, stroke, 2.0f);
    }
    if (arrow_style_ == ArrowStyle::Start || arrow_style_ == ArrowStyle::Both) {
        std::string arrow = build_arrow_path(current_pos_, start_pos_, arrow_size_);
        renderer.stroke_path(arrow, stroke, 2.0f);
    }

    // Draw endpoints
    renderer.draw_circle(start_pos_.x(), start_pos_.y(), 4.0f, point_fill, stroke, 1.0f);
    renderer.draw_circle(current_pos_.x(), current_pos_.y(), 4.0f, point_fill, stroke, 1.0f);
}

std::string LineTool::build_arrow_path(const flex::Vec2& from, const flex::Vec2& to, float size) const {
    float dx = to.x() - from.x();
    float dy = to.y() - from.y();
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return "";

    // Unit vector along line
    float ux = dx / len;
    float uy = dy / len;

    // Perpendicular vector
    float px = -uy;
    float py = ux;

    // Arrow tip is at 'to', wings are behind
    float wing_back = size * 0.8f;
    float wing_width = size * 0.4f;

    float tip_x = to.x();
    float tip_y = to.y();
    float left_x = tip_x - ux * wing_back + px * wing_width;
    float left_y = tip_y - uy * wing_back + py * wing_width;
    float right_x = tip_x - ux * wing_back - px * wing_width;
    float right_y = tip_y - uy * wing_back - py * wing_width;

    char buf[256];
    snprintf(buf, sizeof(buf), "M %.1f %.1f L %.1f %.1f M %.1f %.1f L %.1f %.1f",
             tip_x, tip_y, left_x, left_y,
             tip_x, tip_y, right_x, right_y);
    return buf;
}

flex::Vec2 LineTool::constrain_angle(const flex::Vec2& start, const flex::Vec2& end) const {
    float dx = end.x() - start.x();
    float dy = end.y() - start.y();
    float angle = std::atan2(dy, dx);
    float length = std::sqrt(dx * dx + dy * dy);

    float snapped = std::round(angle / (3.14159f / 4)) * (3.14159f / 4);
    return flex::Vec2(start.x() + length * std::cos(snapped),
                      start.y() + length * std::sin(snapped));
}

} // namespace meta_editor
