/*
 * Meta Editor - Select Tool Implementation
 *
 * Normal mode: select, move objects
 * Path edit mode: edit bezier points and handles
 */

#include "meta_editor/tools/select_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include <flex/runtime/text.h>
#include <cmath>
#include <cstring>
#include <stb_sprintf.h>
#include <sstream>

namespace meta_editor {

SelectTool::SelectTool() {}

void SelectTool::activate() {
    mode_ = Mode::Select;
    drag_mode_ = DragMode::None;
    active_handle_ = HandleType::None;
    editing_path_ = nullptr;
    selected_point_ = -1;

    // Create snap helper if needed
    if (!snap_helper_ && canvas_ && selection_) {
        snap_helper_ = std::make_unique<SnapHelper>(canvas_, selection_);
    }
}

void SelectTool::deactivate() {
    if (mode_ == Mode::PathEdit) {
        exit_path_edit_mode();
    }
}

bool SelectTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    // Check for double-click first
    bool dbl_click = is_double_click(world_pos);
    last_click_time_ = std::chrono::steady_clock::now();
    last_click_pos_ = world_pos;

    if (mode_ == Mode::TextEdit) {
        // Click outside text = exit edit mode
        auto* hit = canvas_->hit_test(world_pos);
        if (hit != editing_text_) {
            exit_text_edit_mode();
        }
        return true;
    }

    if (mode_ == Mode::PathEdit) {
        // In path edit mode - check for point/handle hits
        int handle_in_idx = hit_test_handle_in(world_pos);
        int handle_out_idx = hit_test_handle_out(world_pos);
        int point_idx = hit_test_point(world_pos);

        if (handle_in_idx >= 0) {
            selected_point_ = handle_in_idx;
            drag_mode_ = DragMode::MoveHandleIn;
            drag_start_world_ = world_pos;
            original_point_ = path_data_.points[handle_in_idx];
            return true;
        }

        if (handle_out_idx >= 0) {
            selected_point_ = handle_out_idx;
            drag_mode_ = DragMode::MoveHandleOut;
            drag_start_world_ = world_pos;
            original_point_ = path_data_.points[handle_out_idx];
            return true;
        }

        if (point_idx >= 0) {
            selected_point_ = point_idx;
            drag_mode_ = DragMode::MovePoint;
            drag_start_world_ = world_pos;
            original_point_ = path_data_.points[point_idx];
            return true;
        }

        // Clicked outside - exit edit mode
        exit_path_edit_mode();
        return true;
    }

    // Normal select mode - check resize/rotate handles first
    if (selection_->has_selection()) {
        HandleType handle = selection_->hit_test_handle(screen_pos);
        if (handle == HandleType::Rotate) {
            drag_mode_ = DragMode::Rotate;
            active_handle_ = handle;
            drag_start_screen_ = screen_pos;
            drag_start_world_ = world_pos;

            // Store rotation center (selection center in screen space)
            auto bounds = selection_->selection_bounds();
            rotation_center_ = canvas_->world_to_screen(
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2
            );

            // Store original rotations for undo
            original_rotations_.clear();
            for (auto* node : selection_->selection()) {
                original_rotations_.push_back(node->rotation());
            }
            return true;
        }
        if (handle != HandleType::None) {
            drag_mode_ = DragMode::Resize;
            active_handle_ = handle;
            drag_start_screen_ = screen_pos;
            drag_start_world_ = world_pos;
            original_bounds_ = selection_->selection_bounds();

            // Store original positions/sizes for undo
            original_positions_.clear();
            original_sizes_.clear();
            for (auto* node : selection_->selection()) {
                original_positions_.push_back(flex::Vec2(node->x(), node->y()));
                auto bounds = node->bounds();
                original_sizes_.push_back(flex::Vec2(bounds.width, bounds.height));
            }
            return true;
        }
    }

    // Check for object hit (use world coordinates for hit test)
    auto* hit_node = canvas_->hit_test(world_pos);

    if (hit_node) {
        // Skip locked nodes for editing/moving
        bool is_locked = selection_->is_locked(hit_node);

        // Double-click handling (allowed on locked for viewing, but not editing)
        if (dbl_click && !is_locked) {
            // Double-click on Text = enter text edit mode
            if (hit_node->type() == flex::NodeType::Text) {
                selection_->select(hit_node);
                enter_text_edit_mode(hit_node);
                return true;
            }
            // Double-click on path shape = enter path edit mode
            if (hit_node->type() == flex::NodeType::Shape) {
                auto* shape = static_cast<flex::Shape*>(hit_node);
                if (shape->geometry_type() == flex::GeometryType::Path) {
                    selection_->select(hit_node);
                    enter_path_edit_mode(hit_node);
                    return true;
                }
            }
        }

        // Shift+Click = toggle selection (add/remove)
        if (shift_pressed_) {
            if (selection_->is_selected(hit_node)) {
                selection_->remove_from_selection(hit_node);
            } else {
                selection_->add_to_selection(hit_node);
            }
        } else {
            // Normal click = select only this node (unless already selected for drag)
            if (!selection_->is_selected(hit_node)) {
                selection_->select(hit_node);
            }
        }

        // Start drag only if not locked
        if (!is_locked) {
            drag_mode_ = DragMode::Move;
        }
        drag_start_world_ = world_pos;
        drag_start_screen_ = screen_pos;

        // Store original positions for undo
        original_positions_.clear();
        for (auto* node : selection_->selection()) {
            original_positions_.push_back(flex::Vec2(node->x(), node->y()));
        }

        return true;
    } else {
        // Clicked on empty space - start marquee selection
        if (!shift_pressed_) {
            selection_->clear_selection();
        }
        drag_mode_ = DragMode::Marquee;
        drag_start_world_ = world_pos;
        marquee_end_ = world_pos;
        return true;
    }
}

bool SelectTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::PathEdit) {
        if (drag_mode_ == DragMode::MovePoint && selected_point_ >= 0) {
            auto delta = world_pos - drag_start_world_;
            auto& pt = path_data_.points[selected_point_];
            pt.position = flex::Vec2(
                original_point_.position.x() + delta.x(),
                original_point_.position.y() + delta.y()
            );
            apply_path_to_node();
            return true;
        }

        if (drag_mode_ == DragMode::MoveHandleIn && selected_point_ >= 0) {
            auto& pt = path_data_.points[selected_point_];
            pt.handle_in = flex::Vec2(
                world_pos.x() - pt.position.x(),
                world_pos.y() - pt.position.y()
            );
            pt.constrain_handles(false);  // Moved handle_in
            apply_path_to_node();
            return true;
        }

        if (drag_mode_ == DragMode::MoveHandleOut && selected_point_ >= 0) {
            auto& pt = path_data_.points[selected_point_];
            pt.handle_out = flex::Vec2(
                world_pos.x() - pt.position.x(),
                world_pos.y() - pt.position.y()
            );
            pt.constrain_handles(true);  // Moved handle_out
            apply_path_to_node();
            return true;
        }

        return false;
    }

    // Rotate mode
    if (drag_mode_ == DragMode::Rotate) {
        apply_rotate(screen_pos);
        return true;
    }

    // Resize mode
    if (drag_mode_ == DragMode::Resize) {
        apply_resize(screen_pos);
        return true;
    }

    // Marquee selection mode
    if (drag_mode_ == DragMode::Marquee) {
        marquee_end_ = world_pos;
        return true;
    }

    // Normal move mode
    if (drag_mode_ == DragMode::Move) {
        auto delta = world_pos - drag_start_world_;
        const auto& selected = selection_->selection();

        // Calculate new bounds for snapping
        flex::Bounds new_bounds = selection_->selection_bounds();
        new_bounds.x += delta.x();
        new_bounds.y += delta.y();

        // Use smart guides for object snapping
        if (snap_helper_) {
            std::vector<flex::Node*> excluded(selected.begin(), selected.end());
            auto snap_result = snap_helper_->snap_bounds(new_bounds, excluded);

            // Apply snapped position
            float snap_dx = snap_result.snapped_pos.x() - new_bounds.x + delta.x();
            float snap_dy = snap_result.snapped_pos.y() - new_bounds.y + delta.y();

            for (size_t i = 0; i < selected.size(); ++i) {
                auto* node = selected[i];
                auto original_pos = original_positions_[i];
                node->set_position(original_pos.x() + snap_dx, original_pos.y() + snap_dy);
            }
        } else {
            // Fallback to grid snapping only
            for (size_t i = 0; i < selected.size(); ++i) {
                auto* node = selected[i];
                auto original_pos = original_positions_[i];
                float new_x = original_pos.x() + delta.x();
                float new_y = original_pos.y() + delta.y();

                if (canvas_->is_snap_to_grid()) {
                    auto snapped = canvas_->snap_to_grid(flex::Vec2(new_x, new_y));
                    new_x = snapped.x();
                    new_y = snapped.y();
                }

                node->set_position(new_x, new_y);
            }
        }

        return true;
    }

    return false;
}

bool SelectTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (mode_ == Mode::PathEdit) {
        if (drag_mode_ == DragMode::MovePoint ||
            drag_mode_ == DragMode::MoveHandleIn ||
            drag_mode_ == DragMode::MoveHandleOut) {
            // Create EditPathCommand for undo
            std::string new_path = path_data_.to_svg_path();
            if (new_path != original_path_data_ && commands_ && editing_path_) {
                // Restore original first
                auto* shape = static_cast<flex::Shape*>(editing_path_);
                shape->set_path(original_path_data_);

                auto cmd = std::make_unique<EditPathCommand>(
                    editing_path_, original_path_data_, new_path);
                commands_->execute(std::move(cmd));

                // Update original for next edit
                original_path_data_ = new_path;
            }
            drag_mode_ = DragMode::None;
            return true;
        }
        return false;
    }

    // Rotate mode
    if (drag_mode_ == DragMode::Rotate) {
        const auto& selected = selection_->selection();

        // Check if anything actually rotated
        bool rotated = false;
        std::vector<float> new_rotations;
        for (size_t i = 0; i < selected.size(); ++i) {
            new_rotations.push_back(selected[i]->rotation());
            if (std::abs(new_rotations[i] - original_rotations_[i]) > 0.1f) {
                rotated = true;
            }
        }

        // Create RotateCommand if something rotated
        if (rotated && commands_) {
            for (size_t i = 0; i < selected.size(); ++i) {
                selected[i]->set_rotation(original_rotations_[i]);
            }

            auto cmd = std::make_unique<RotateCommand>(
                std::vector<flex::Node*>(selected.begin(), selected.end()),
                original_rotations_,
                new_rotations
            );
            commands_->execute(std::move(cmd));
        }

        drag_mode_ = DragMode::None;
        active_handle_ = HandleType::None;
        return true;
    }

    // Resize mode
    if (drag_mode_ == DragMode::Resize) {
        // TODO: Create ResizeCommand for undo
        drag_mode_ = DragMode::None;
        active_handle_ = HandleType::None;
        return true;
    }

    // Marquee selection complete
    if (drag_mode_ == DragMode::Marquee) {
        marquee_end_ = world_pos;
        auto nodes = get_nodes_in_marquee();
        for (auto* node : nodes) {
            selection_->add_to_selection(node);
        }
        drag_mode_ = DragMode::None;
        return true;
    }

    if (drag_mode_ == DragMode::Move) {
        // Clear snap guides
        if (snap_helper_) {
            snap_helper_->clear_guides();
        }

        const auto& selected = selection_->selection();

        // Check if anything actually moved
        bool moved = false;
        std::vector<flex::Vec2> new_positions;
        for (size_t i = 0; i < selected.size(); ++i) {
            new_positions.push_back(flex::Vec2(selected[i]->x(), selected[i]->y()));
            if (new_positions[i].x() != original_positions_[i].x() ||
                new_positions[i].y() != original_positions_[i].y()) {
                moved = true;
            }
        }

        // Create MoveCommand if something moved
        if (moved && commands_) {
            // Restore original positions first (command will re-apply)
            for (size_t i = 0; i < selected.size(); ++i) {
                selected[i]->set_position(original_positions_[i].x(), original_positions_[i].y());
            }

            auto cmd = std::make_unique<MoveCommand>(
                std::vector<flex::Node*>(selected.begin(), selected.end()),
                original_positions_,
                new_positions
            );
            commands_->execute(std::move(cmd));
        }

        drag_mode_ = DragMode::None;
        return true;
    }

    return false;
}

bool SelectTool::on_key_down(int key, int mods) {
    // Track Shift via mods (more reliable than key code)
    shift_pressed_ = (mods & 0x0003) != 0;  // KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT

    if (mode_ == Mode::TextEdit) {
        if (key == 27) {  // Escape
            exit_text_edit_mode();
            return true;
        }
        if (key == 13) {  // Enter - commit and exit
            if (editing_text_) {
                auto* text = static_cast<flex::Text*>(editing_text_);
                text->set_content(text_buffer_.c_str());
            }
            exit_text_edit_mode();
            return true;
        }
        if (key == 8 && cursor_pos_ > 0) {  // Backspace
            text_buffer_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
            if (editing_text_) {
                auto* text = static_cast<flex::Text*>(editing_text_);
                text->set_content(text_buffer_.c_str());
            }
            return true;
        }
        if (key == 127 && cursor_pos_ < text_buffer_.length()) {  // Delete
            text_buffer_.erase(cursor_pos_, 1);
            if (editing_text_) {
                auto* text = static_cast<flex::Text*>(editing_text_);
                text->set_content(text_buffer_.c_str());
            }
            return true;
        }
        // Arrow keys for cursor movement
        if (key == 263 && cursor_pos_ > 0) cursor_pos_--;  // Left
        if (key == 262 && cursor_pos_ < text_buffer_.length()) cursor_pos_++;  // Right
        return true;
    }

    if (mode_ == Mode::PathEdit) {
        if (key == 27) {  // Escape
            exit_path_edit_mode();
            return true;
        }
        // Delete selected point
        if ((key == 127 || key == 8) && selected_point_ >= 0) {  // Delete or Backspace
            if (path_data_.points.size() > 2) {
                path_data_.points.erase(path_data_.points.begin() + selected_point_);
                selected_point_ = -1;
                apply_path_to_node();
            }
            return true;
        }
    }
    return false;
}

bool SelectTool::on_key_up(int key, int mods) {
    // Track Shift via mods
    shift_pressed_ = (mods & 0x0003) != 0;  // KMOD_SHIFT
    return false;
}

bool SelectTool::on_text_input(const char* text) {
    if (mode_ != Mode::TextEdit || !editing_text_) return false;
    
    text_buffer_.insert(cursor_pos_, text);
    cursor_pos_ += strlen(text);
    
    auto* txt = static_cast<flex::Text*>(editing_text_);
    txt->set_content(text_buffer_.c_str());
    return true;
}

void SelectTool::render_overlay(flex::Renderer& renderer) {
    if (mode_ == Mode::PathEdit) {
        render_path_edit_overlay(renderer);
    } else if (mode_ == Mode::TextEdit) {
        // Draw text edit cursor indicator
        selection_->render_selection_indicators(renderer);
    } else {
        selection_->render_selection_indicators(renderer);
        render_snap_guides(renderer);
        if (drag_mode_ == DragMode::Marquee) {
            render_marquee(renderer);
        }
    }
}

// ============================================================================
// Path Edit Mode
// ============================================================================

void SelectTool::enter_path_edit_mode(flex::Node* path_node) {
    editing_path_ = path_node;
    mode_ = Mode::PathEdit;
    selected_point_ = -1;
    load_path_from_node();
}

void SelectTool::exit_path_edit_mode() {
    editing_path_ = nullptr;
    mode_ = Mode::Select;
    selected_point_ = -1;
    path_data_.points.clear();
}

void SelectTool::enter_text_edit_mode(flex::Node* text_node) {
    if (!text_node || text_node->type() != flex::NodeType::Text) return;
    
    editing_text_ = text_node;
    mode_ = Mode::TextEdit;
    
    auto* text = static_cast<flex::Text*>(text_node);
    text_buffer_ = text->content();
    cursor_pos_ = text_buffer_.length();
}

void SelectTool::exit_text_edit_mode() {
    editing_text_ = nullptr;
    mode_ = Mode::Select;
    text_buffer_.clear();
    cursor_pos_ = 0;
}

void SelectTool::load_path_from_node() {
    if (!editing_path_ || editing_path_->type() != flex::NodeType::Shape) return;

    auto* shape = static_cast<flex::Shape*>(editing_path_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;

    original_path_data_ = shape->path().d;

    // Parse SVG path data into PathEditPoints
    // This is a simplified parser - handles M, L, C, Z commands
    path_data_.points.clear();
    path_data_.closed = false;

    const std::string& d = original_path_data_;
    std::istringstream iss(d);
    char cmd;
    float x, y, x1, y1, x2, y2;

    PathEditPoint* last_point = nullptr;

    while (iss >> cmd) {
        switch (cmd) {
            case 'M':
            case 'm':
                iss >> x >> y;
                path_data_.points.push_back(PathEditPoint(x, y));
                last_point = &path_data_.points.back();
                break;

            case 'L':
            case 'l':
                iss >> x >> y;
                if (cmd == 'l' && last_point) {
                    x += last_point->position.x();
                    y += last_point->position.y();
                }
                path_data_.points.push_back(PathEditPoint(x, y));
                last_point = &path_data_.points.back();
                break;

            case 'C':
            case 'c': {
                iss >> x1 >> y1 >> x2 >> y2 >> x >> y;
                if (cmd == 'c' && last_point) {
                    x1 += last_point->position.x();
                    y1 += last_point->position.y();
                    x2 += last_point->position.x();
                    y2 += last_point->position.y();
                    x += last_point->position.x();
                    y += last_point->position.y();
                }
                // Set handle_out on previous point
                if (last_point) {
                    last_point->handle_out = flex::Vec2(
                        x1 - last_point->position.x(),
                        y1 - last_point->position.y()
                    );
                    if (last_point->type == PointType::Corner) {
                        last_point->type = PointType::Smooth;
                    }
                }
                // Add new point with handle_in
                PathEditPoint pt(x, y);
                pt.handle_in = flex::Vec2(x2 - x, y2 - y);
                pt.type = PointType::Smooth;
                path_data_.points.push_back(pt);
                last_point = &path_data_.points.back();
                break;
            }

            case 'Z':
            case 'z':
                path_data_.closed = true;
                break;
        }
    }
}

void SelectTool::apply_path_to_node() {
    if (!editing_path_ || editing_path_->type() != flex::NodeType::Shape) return;

    auto* shape = static_cast<flex::Shape*>(editing_path_);
    if (shape->geometry_type() != flex::GeometryType::Path) return;

    shape->set_path(path_data_.to_svg_path());
}

int SelectTool::hit_test_point(const flex::Vec2& pos, float threshold) {
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

int SelectTool::hit_test_handle_in(const flex::Vec2& pos, float threshold) {
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

int SelectTool::hit_test_handle_out(const flex::Vec2& pos, float threshold) {
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

void SelectTool::render_path_edit_overlay(flex::Renderer& renderer) {
    // Colors
    flex::Paint point_fill = flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 1.0f));
    flex::Paint point_stroke = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint selected_fill = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));
    flex::Paint handle_line = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 0.7f));
    flex::Paint handle_fill = flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.9f, 1.0f));

    // Draw handles and points
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

bool SelectTool::is_double_click(const flex::Vec2& pos) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time_).count();

    float dx = pos.x() - last_click_pos_.x();
    float dy = pos.y() - last_click_pos_.y();
    float dist_sq = dx * dx + dy * dy;

    // Double-click: within 400ms and 10 pixels
    return (elapsed < 400 && dist_sq < 100.0f);
}

void SelectTool::apply_rotate(const flex::Vec2& screen_pos) {
    // Calculate angle from center to current mouse position
    float dx = screen_pos.x() - rotation_center_.x();
    float dy = screen_pos.y() - rotation_center_.y();
    float current_angle = std::atan2(dy, dx) * 180.0f / 3.14159f;

    // Calculate angle from center to drag start
    float start_dx = drag_start_screen_.x() - rotation_center_.x();
    float start_dy = drag_start_screen_.y() - rotation_center_.y();
    float start_angle = std::atan2(start_dy, start_dx) * 180.0f / 3.14159f;

    float delta_angle = current_angle - start_angle;

    // Snap to 15° increments if Shift is held
    if (shift_pressed_) {
        delta_angle = std::round(delta_angle / 15.0f) * 15.0f;
    }

    // Apply rotation to all selected nodes
    const auto& selected = selection_->selection();
    for (size_t i = 0; i < selected.size(); ++i) {
        float new_rotation = original_rotations_[i] + delta_angle;
        selected[i]->set_rotation(new_rotation);
    }
}

void SelectTool::apply_resize(const flex::Vec2& screen_pos) {
    // Convert screen delta to world delta
    flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

    // Snap the target position if grid is enabled
    if (canvas_->is_snap_to_grid()) {
        world_pos = canvas_->snap_to_grid(world_pos);
    }

    flex::Vec2 start_world = canvas_->screen_to_world(drag_start_screen_);
    float dx = world_pos.x() - start_world.x();
    float dy = world_pos.y() - start_world.y();

    // Original bounds
    float ox = original_bounds_.x;
    float oy = original_bounds_.y;
    float ow = original_bounds_.width;
    float oh = original_bounds_.height;

    // New bounds (modified based on handle)
    float nx = ox, ny = oy, nw = ow, nh = oh;

    switch (active_handle_) {
        case HandleType::TopLeft:
            nx = ox + dx;
            ny = oy + dy;
            nw = ow - dx;
            nh = oh - dy;
            break;
        case HandleType::TopCenter:
            ny = oy + dy;
            nh = oh - dy;
            break;
        case HandleType::TopRight:
            ny = oy + dy;
            nw = ow + dx;
            nh = oh - dy;
            break;
        case HandleType::RightCenter:
            nw = ow + dx;
            break;
        case HandleType::BottomRight:
            nw = ow + dx;
            nh = oh + dy;
            break;
        case HandleType::BottomCenter:
            nh = oh + dy;
            break;
        case HandleType::BottomLeft:
            nx = ox + dx;
            nw = ow - dx;
            nh = oh + dy;
            break;
        case HandleType::LeftCenter:
            nx = ox + dx;
            nw = ow - dx;
            break;
        default:
            break;
    }

    // Constrain proportions if Shift pressed
    if (shift_pressed_ && ow > 0 && oh > 0) {
        float aspect = ow / oh;
        // Adjust based on handle type
        if (active_handle_ == HandleType::TopLeft ||
            active_handle_ == HandleType::TopRight ||
            active_handle_ == HandleType::BottomLeft ||
            active_handle_ == HandleType::BottomRight) {
            // Corner handles - constrain to aspect ratio
            if (std::abs(nw / aspect) > std::abs(nh)) {
                nh = nw / aspect;
            } else {
                nw = nh * aspect;
            }
            // Adjust position for corners that affect origin
            if (active_handle_ == HandleType::TopLeft) {
                nx = ox + ow - nw;
                ny = oy + oh - nh;
            } else if (active_handle_ == HandleType::TopRight) {
                ny = oy + oh - nh;
            } else if (active_handle_ == HandleType::BottomLeft) {
                nx = ox + ow - nw;
            }
        }
    }

    // Ensure minimum size
    const float min_size = 5.0f;
    if (nw < min_size) {
        if (active_handle_ == HandleType::TopLeft ||
            active_handle_ == HandleType::BottomLeft ||
            active_handle_ == HandleType::LeftCenter) {
            nx = ox + ow - min_size;
        }
        nw = min_size;
    }
    if (nh < min_size) {
        if (active_handle_ == HandleType::TopLeft ||
            active_handle_ == HandleType::TopCenter ||
            active_handle_ == HandleType::TopRight) {
            ny = oy + oh - min_size;
        }
        nh = min_size;
    }

    // Calculate scale factors
    float scale_x = nw / ow;
    float scale_y = nh / oh;

    // Apply to all selected nodes
    const auto& selected = selection_->selection();
    for (size_t i = 0; i < selected.size(); ++i) {
        auto* node = selected[i];
        const auto& orig_pos = original_positions_[i];
        const auto& orig_size = original_sizes_[i];

        // Calculate new position (relative to original bounds)
        float rel_x = ow > 0 ? (orig_pos.x() - ox) / ow : 0;
        float rel_y = oh > 0 ? (orig_pos.y() - oy) / oh : 0;
        float new_x = nx + rel_x * nw;
        float new_y = ny + rel_y * nh;

        node->set_position(new_x, new_y);

        // Apply scale to geometry
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            float new_w = orig_size.x() * scale_x;
            float new_h = orig_size.y() * scale_y;

            // For radial shapes, original radius = orig_size / 2
            float orig_radius = (orig_size.x() + orig_size.y()) / 4;
            float new_radius = orig_radius * (scale_x + scale_y) / 2;

            switch (shape->geometry_type()) {
                case flex::GeometryType::Rect: {
                    auto r = shape->rect();
                    shape->set_rect(new_w, new_h, r.corner_radius * (scale_x + scale_y) / 2);
                    break;
                }
                case flex::GeometryType::Circle:
                    shape->set_circle(new_radius);
                    break;
                case flex::GeometryType::Ellipse:
                    shape->set_ellipse(orig_size.x() / 2 * scale_x, orig_size.y() / 2 * scale_y);
                    break;
                case flex::GeometryType::Polygon: {
                    auto p = shape->polygon();
                    shape->set_polygon(p.sides, new_radius);
                    break;
                }
                case flex::GeometryType::Star: {
                    auto s = shape->star();
                    float ratio = s.outer_radius > 0 ? s.inner_radius / s.outer_radius : 0.5f;
                    shape->set_star(s.points, new_radius, new_radius * ratio);
                    break;
                }
                case flex::GeometryType::Triangle:
                    shape->set_triangle(new_w, new_h, shape->triangle().direction);
                    break;
                case flex::GeometryType::Line:
                    shape->set_line(orig_size.x() * scale_x, orig_size.y() * scale_y);
                    break;
                case flex::GeometryType::Ring: {
                    auto r = shape->ring();
                    float ratio = r.outer_radius > 0 ? r.inner_radius / r.outer_radius : 0.5f;
                    shape->set_ring(new_radius, new_radius * ratio);
                    break;
                }
                case flex::GeometryType::Path:
                    shape->set_path(shape->path().d, new_w, new_h);
                    break;
                default:
                    break;
            }
        }
    }
}

void SelectTool::render_snap_guides(flex::Renderer& renderer) {
    if (!snap_helper_) return;

    const auto& guides = snap_helper_->guides();
    if (guides.empty()) return;

    // Cyan dashed lines for snap guides (draw in world coordinates)
    flex::Paint guide_paint = flex::Paint::solid(flex::Color(0.0f, 0.8f, 1.0f, 0.8f));

    for (const auto& guide : guides) {
        if (guide.type == SnapGuide::Type::Vertical) {
            // Vertical line at world x position
            std::string path = "M " + std::to_string(guide.position) + " -10000" +
                              " L " + std::to_string(guide.position) + " 10000";
            renderer.stroke_path(path, guide_paint, 1.0f);
        } else {
            // Horizontal line at world y position
            std::string path = "M -10000 " + std::to_string(guide.position) +
                              " L 10000 " + std::to_string(guide.position);
            renderer.stroke_path(path, guide_paint, 1.0f);
        }
    }
}

void SelectTool::render_marquee(flex::Renderer& renderer) {
    // Draw in world coordinates (camera transform is applied by caller)
    float x = std::min(drag_start_world_.x(), marquee_end_.x());
    float y = std::min(drag_start_world_.y(), marquee_end_.y());
    float w = std::abs(marquee_end_.x() - drag_start_world_.x());
    float h = std::abs(marquee_end_.y() - drag_start_world_.y());

    // Semi-transparent fill
    flex::Paint fill = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.9f, 0.15f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.9f, 0.8f});
    flex::Paint guide_stroke = flex::Paint::solid(flex::Color(0.3f, 0.5f, 0.9f, 0.6f));
    flex::Paint point_fill = flex::Paint::solid(flex::Color(0.3f, 0.5f, 0.9f, 1.0f));
    flex::Paint dim_stroke = flex::Paint::solid(flex::Color(0.5f, 0.5f, 0.5f, 0.6f));

    // Draw marquee rectangle
    renderer.draw_rect(x, y, w, h, 0, fill, stroke, 1.0f);

    // Draw corner points (start -> end)
    renderer.draw_circle(drag_start_world_.x(), drag_start_world_.y(), 4.0f, point_fill, stroke, 1.0f);
    renderer.draw_circle(marquee_end_.x(), marquee_end_.y(), 4.0f, point_fill, stroke, 1.0f);

    // Draw diagonal guide line
    std::string diag = "M " + std::to_string(drag_start_world_.x()) + " " + std::to_string(drag_start_world_.y()) +
                      " L " + std::to_string(marquee_end_.x()) + " " + std::to_string(marquee_end_.y());
    renderer.stroke_path(diag, guide_stroke, 1.0f);

    // Draw dimension lines
    float dim_offset = 8.0f;
    // Width dimension (top)
    std::string w_dim = "M " + std::to_string(x) + " " + std::to_string(y - dim_offset) +
                       " L " + std::to_string(x + w) + " " + std::to_string(y - dim_offset);
    renderer.stroke_path(w_dim, dim_stroke, 1.0f);
    // Height dimension (right)
    std::string h_dim = "M " + std::to_string(x + w + dim_offset) + " " + std::to_string(y) +
                       " L " + std::to_string(x + w + dim_offset) + " " + std::to_string(y + h);
    renderer.stroke_path(h_dim, dim_stroke, 1.0f);

    // Draw dimension text (in world coordinates)
    char dim_text[32];
    stbsp_snprintf(dim_text, sizeof(dim_text), "%.0f x %.0f", w, h);
    renderer.draw_text(dim_text, x + w / 2 - 20, y - dim_offset - 14,
                      "Arial", 11.0f, false, flex::Color(0.4f, 0.4f, 0.4f, 1.0f));
}

std::vector<flex::Node*> SelectTool::get_nodes_in_marquee() {
    std::vector<flex::Node*> result;

    float x1 = std::min(drag_start_world_.x(), marquee_end_.x());
    float y1 = std::min(drag_start_world_.y(), marquee_end_.y());
    float x2 = std::max(drag_start_world_.x(), marquee_end_.x());
    float y2 = std::max(drag_start_world_.y(), marquee_end_.y());

    flex::Bounds marquee{x1, y1, x2 - x1, y2 - y1};

    auto layers = canvas_->get_all_layers();
    for (auto* layer : layers) {
        if (!layer->visible()) continue;

        for (auto* child : layer->children()) {
            if (!child->visible()) continue;

            auto nb = child->world_bounds();
            bool intersects = !(nb.x + nb.width < marquee.x ||
                               nb.x > marquee.x + marquee.width ||
                               nb.y + nb.height < marquee.y ||
                               nb.y > marquee.y + marquee.height);

            if (intersects) {
                result.push_back(child);
            }
        }
    }

    return result;
}

} // namespace meta_editor
