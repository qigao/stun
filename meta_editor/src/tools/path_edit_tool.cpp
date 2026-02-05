/*
 * Meta Editor - Path Edit Tool Implementation
 */

#include "meta_editor/tools/path_edit_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <sstream>

namespace meta_editor {

void PathEditTool::activate() {
    // If no path is being edited, this tool shouldn't be active
}

void PathEditTool::deactivate() {
    if (editing_path_) {
        commit_edit();
    }
}

void PathEditTool::edit_path(flex::Node* path_node) {
    if (!path_node || path_node->type() != flex::NodeType::Shape) return;

    auto* shape = static_cast<flex::Shape*>(path_node);
    if (shape->geometry_type() != flex::GeometryType::Path) return;

    editing_path_ = path_node;
    selected_point_ = -1;
    load_path_from_node();
}

bool PathEditTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!editing_path_) return false;

    // Check for handle hits first (smaller targets)
    int handle_in_idx = hit_test_handle_in(world_pos);
    if (handle_in_idx >= 0) {
        selected_point_ = handle_in_idx;
        drag_mode_ = DragMode::MoveHandleIn;
        drag_start_ = world_pos;
        original_point_ = path_data_.points[handle_in_idx];
        return true;
    }

    int handle_out_idx = hit_test_handle_out(world_pos);
    if (handle_out_idx >= 0) {
        selected_point_ = handle_out_idx;
        drag_mode_ = DragMode::MoveHandleOut;
        drag_start_ = world_pos;
        original_point_ = path_data_.points[handle_out_idx];
        return true;
    }

    // Check for point hit
    int point_idx = hit_test_point(world_pos);
    if (point_idx >= 0) {
        selected_point_ = point_idx;
        drag_mode_ = DragMode::MovePoint;
        drag_start_ = world_pos;
        original_point_ = path_data_.points[point_idx];
        return true;
    }

    // Clicked outside - commit and exit
    commit_edit();
    return true;
}

bool PathEditTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!editing_path_ || selected_point_ < 0) return false;

    auto& pt = path_data_.points[selected_point_];

    if (drag_mode_ == DragMode::MovePoint) {
        auto delta = world_pos - drag_start_;
        pt.position = flex::Vec2(
            original_point_.position.x() + delta.x(),
            original_point_.position.y() + delta.y()
        );
        apply_path_to_node();
        return true;
    }

    if (drag_mode_ == DragMode::MoveHandleIn) {
        pt.handle_in = flex::Vec2(
            world_pos.x() - pt.position.x(),
            world_pos.y() - pt.position.y()
        );
        pt.constrain_handles(false);
        apply_path_to_node();
        return true;
    }

    if (drag_mode_ == DragMode::MoveHandleOut) {
        pt.handle_out = flex::Vec2(
            world_pos.x() - pt.position.x(),
            world_pos.y() - pt.position.y()
        );
        pt.constrain_handles(true);
        apply_path_to_node();
        return true;
    }

    return false;
}

bool PathEditTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!editing_path_) return false;

    if (drag_mode_ != DragMode::None) {
        // Create undo command
        std::string new_path = path_data_.to_svg_path();
        if (new_path != original_path_data_ && commands_) {
            auto* shape = static_cast<flex::Shape*>(editing_path_);
            shape->set_path(original_path_data_);

            auto cmd = std::make_unique<EditPathCommand>(
                editing_path_, original_path_data_, new_path);
            commands_->execute(std::move(cmd));

            original_path_data_ = new_path;
        }
        drag_mode_ = DragMode::None;
        return true;
    }

    return false;
}

bool PathEditTool::on_key_down(int key, int mods) {
    if (!editing_path_) return false;

    if (key == static_cast<int>(Key::Escape)) {
        // Cancel - restore original
        auto* shape = static_cast<flex::Shape*>(editing_path_);
        shape->set_path(original_path_data_);
        editing_path_ = nullptr;
        path_data_.points.clear();
        return true;
    }

    // Delete selected point
    if ((key == static_cast<int>(Key::Delete) || key == static_cast<int>(Key::Backspace)) 
        && selected_point_ >= 0) {
        if (path_data_.points.size() > 2) {
            path_data_.points.erase(path_data_.points.begin() + selected_point_);
            selected_point_ = -1;
            apply_path_to_node();
        }
        return true;
    }

    return false;
}

void PathEditTool::render_overlay(flex::Renderer& renderer) {
    if (!editing_path_) return;

    flex::Paint point_fill = flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
    flex::Paint point_stroke = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint selected_fill = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint handle_line = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 0.7f));
    flex::Paint handle_fill = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));

    for (size_t i = 0; i < path_data_.points.size(); ++i) {
        const auto& pt = path_data_.points[i];

        // Draw handle lines and circles
        if (pt.has_handle_in()) {
            auto h = pt.handle_in_abs();
            std::string line = "M " + std::to_string(pt.position.x()) + " " + std::to_string(pt.position.y()) +
                              " L " + std::to_string(h.x()) + " " + std::to_string(h.y());
            renderer.stroke_path(line, handle_line, 1.0f);
            renderer.draw_circle(h.x(), h.y(), 4.0f, handle_fill, point_stroke, 1.0f);
        }
        if (pt.has_handle_out()) {
            auto h = pt.handle_out_abs();
            std::string line = "M " + std::to_string(pt.position.x()) + " " + std::to_string(pt.position.y()) +
                              " L " + std::to_string(h.x()) + " " + std::to_string(h.y());
            renderer.stroke_path(line, handle_line, 1.0f);
            renderer.draw_circle(h.x(), h.y(), 4.0f, handle_fill, point_stroke, 1.0f);
        }

        // Draw anchor point
        bool is_selected = (static_cast<int>(i) == selected_point_);
        float radius = is_selected ? 6.0f : 5.0f;
        auto& fill = is_selected ? selected_fill : point_fill;
        renderer.draw_circle(pt.position.x(), pt.position.y(), radius, fill, point_stroke, 1.5f);
    }
}

void PathEditTool::load_path_from_node() {
    if (!editing_path_ || editing_path_->type() != flex::NodeType::Shape) return;

    auto* shape = static_cast<flex::Shape*>(editing_path_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;

    original_path_data_ = shape->path().d;
    path_data_.points.clear();
    path_data_.closed = false;

    const std::string& d = original_path_data_;
    std::istringstream iss(d);
    char cmd;
    float x, y, x1, y1, x2, y2;
    PathEditPoint* last_point = nullptr;

    while (iss >> cmd) {
        switch (cmd) {
            case 'M': case 'm':
                iss >> x >> y;
                path_data_.points.push_back(PathEditPoint(x, y));
                last_point = &path_data_.points.back();
                break;

            case 'L': case 'l':
                iss >> x >> y;
                if (cmd == 'l' && last_point) {
                    x += last_point->position.x();
                    y += last_point->position.y();
                }
                path_data_.points.push_back(PathEditPoint(x, y));
                last_point = &path_data_.points.back();
                break;

            case 'C': case 'c': {
                iss >> x1 >> y1 >> x2 >> y2 >> x >> y;
                if (cmd == 'c' && last_point) {
                    x1 += last_point->position.x();
                    y1 += last_point->position.y();
                    x2 += last_point->position.x();
                    y2 += last_point->position.y();
                    x += last_point->position.x();
                    y += last_point->position.y();
                }
                if (last_point) {
                    last_point->handle_out = flex::Vec2(
                        x1 - last_point->position.x(),
                        y1 - last_point->position.y()
                    );
                    if (last_point->type == PointType::Corner) {
                        last_point->type = PointType::Smooth;
                    }
                }
                PathEditPoint pt(x, y);
                pt.handle_in = flex::Vec2(x2 - x, y2 - y);
                pt.type = PointType::Smooth;
                path_data_.points.push_back(pt);
                last_point = &path_data_.points.back();
                break;
            }

            case 'Z': case 'z':
                path_data_.closed = true;
                break;
        }
    }
}

void PathEditTool::apply_path_to_node() {
    if (!editing_path_ || editing_path_->type() != flex::NodeType::Shape) return;

    auto* shape = static_cast<flex::Shape*>(editing_path_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;

    shape->set_path(path_data_.to_svg_path());
}

void PathEditTool::commit_edit() {
    if (!editing_path_) return;

    std::string new_path = path_data_.to_svg_path();
    if (new_path != original_path_data_ && commands_) {
        auto* shape = static_cast<flex::Shape*>(editing_path_);
        shape->set_path(original_path_data_);

        auto cmd = std::make_unique<EditPathCommand>(
            editing_path_, original_path_data_, new_path);
        commands_->execute(std::move(cmd));
    }

    editing_path_ = nullptr;
    path_data_.points.clear();
    selected_point_ = -1;
}

int PathEditTool::hit_test_point(const flex::Vec2& pos, float threshold) {
    float thresh_sq = threshold * threshold;
    for (size_t i = 0; i < path_data_.points.size(); ++i) {
        const auto& pt = path_data_.points[i];
        float dx = pos.x() - pt.position.x();
        float dy = pos.y() - pt.position.y();
        if (dx * dx + dy * dy < thresh_sq) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PathEditTool::hit_test_handle_in(const flex::Vec2& pos, float threshold) {
    float thresh_sq = threshold * threshold;
    for (size_t i = 0; i < path_data_.points.size(); ++i) {
        const auto& pt = path_data_.points[i];
        if (pt.has_handle_in()) {
            auto h = pt.handle_in_abs();
            float dx = pos.x() - h.x();
            float dy = pos.y() - h.y();
            if (dx * dx + dy * dy < thresh_sq) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

int PathEditTool::hit_test_handle_out(const flex::Vec2& pos, float threshold) {
    float thresh_sq = threshold * threshold;
    for (size_t i = 0; i < path_data_.points.size(); ++i) {
        const auto& pt = path_data_.points[i];
        if (pt.has_handle_out()) {
            auto h = pt.handle_out_abs();
            float dx = pos.x() - h.x();
            float dy = pos.y() - h.y();
            if (dx * dx + dy * dy < thresh_sq) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

} // namespace meta_editor
