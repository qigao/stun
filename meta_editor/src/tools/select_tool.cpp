/*
 * Meta Editor - Select Tool Implementation
 *
 * Single responsibility: select, move, resize, rotate objects.
 * Double-click triggers tool switch to PathEdit or TextEdit.
 */
 
#include "meta_editor/tools/select_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include "meta_editor/tool_manager.h"
#include "meta_editor/tools/path_edit_tool.h"
#include "meta_editor/tools/text_edit_tool.h"
#include <cmath>
#include <algorithm>
#include <stb_sprintf.h>

namespace meta_editor {

SelectTool::SelectTool() {}

void SelectTool::activate() {
    drag_mode_ = DragMode::None;
    active_handle_ = HandleType::None;

    if (!snap_helper_ && canvas_ && selection_) {
        snap_helper_ = std::make_unique<SnapHelper>(canvas_, selection_);
    }
}

void SelectTool::deactivate() {}

bool SelectTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    bool dbl_click = is_double_click(world_pos);
    last_click_time_ = std::chrono::steady_clock::now();
    last_click_pos_ = world_pos;

    // Check resize/rotate handles first
    if (selection_->has_selection()) {
        HandleType handle = selection_->hit_test_handle(screen_pos);
        
        if (handle == HandleType::Rotate) {
            drag_mode_ = DragMode::Rotate;
            active_handle_ = handle;
            drag_start_screen_ = screen_pos;
            
            auto bounds = selection_->selection_bounds();
            rotation_center_ = canvas_->world_to_screen(
                bounds.x + bounds.width / 2,
                bounds.y + bounds.height / 2
            );
            
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
            original_bounds_ = selection_->selection_bounds();
            
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

    // Hit test for objects
    auto* hit_node = canvas_->hit_test(world_pos);

    if (hit_node) {
        bool is_locked = selection_->is_locked(hit_node);

        // Double-click: delegate to specialized tools
        if (dbl_click && !is_locked) {
            if (hit_node->type() == flex::NodeType::Text) {
                selection_->select(hit_node);
                // Switch to TextEdit tool
                if (tool_manager_) {
                    tool_manager_->set_active_tool("TextEdit");
                    if (auto* text_tool = dynamic_cast<TextEditTool*>(tool_manager_->active_tool())) {
                        text_tool->edit_text(hit_node);
                    }
                }
                return true;
            }
            
            if (hit_node->type() == flex::NodeType::Shape) {
                auto* shape = static_cast<flex::Shape*>(hit_node);
                if (shape->geometry_type() == flex::GeometryType::Path) {
                    selection_->select(hit_node);
                    // Switch to PathEdit tool
                    if (tool_manager_) {
                        tool_manager_->set_active_tool("PathEdit");
                        if (auto* path_tool = dynamic_cast<PathEditTool*>(tool_manager_->active_tool())) {
                            path_tool->edit_path(hit_node);
                        }
                    }
                    return true;
                }
            }
        }

        // Shift+Click = toggle selection
        if (shift_pressed_) {
            if (selection_->is_selected(hit_node)) {
                selection_->remove_from_selection(hit_node);
            } else {
                selection_->add_to_selection(hit_node);
            }
        } else if (!selection_->is_selected(hit_node)) {
            selection_->select(hit_node);
        }

        if (!is_locked) {
            drag_mode_ = DragMode::Move;
        }
        drag_start_world_ = world_pos;
        drag_start_screen_ = screen_pos;

        original_positions_.clear();
        for (auto* node : selection_->selection()) {
            original_positions_.push_back(flex::Vec2(node->x(), node->y()));
        }
        return true;
    }

    // Empty space - start marquee
    if (!shift_pressed_) {
        selection_->clear_selection();
    }
    drag_mode_ = DragMode::Marquee;
    drag_start_world_ = world_pos;
    marquee_end_ = world_pos;
    return true;
}

bool SelectTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (drag_mode_ == DragMode::Rotate) {
        apply_rotate(screen_pos);
        return true;
    }

    if (drag_mode_ == DragMode::Resize) {
        apply_resize(screen_pos);
        return true;
    }

    if (drag_mode_ == DragMode::Marquee) {
        marquee_end_ = world_pos;
        return true;
    }

    if (drag_mode_ == DragMode::Move) {
        auto delta = world_pos - drag_start_world_;
        const auto& selected = selection_->selection();

        flex::Bounds new_bounds = selection_->selection_bounds();
        new_bounds.x += delta.x();
        new_bounds.y += delta.y();

        if (snap_helper_) {
            std::vector<flex::Node*> excluded(selected.begin(), selected.end());
            auto snap_result = snap_helper_->snap_bounds(new_bounds, excluded);

            float snap_dx = snap_result.snapped_pos.x() - new_bounds.x + delta.x();
            float snap_dy = snap_result.snapped_pos.y() - new_bounds.y + delta.y();

            for (size_t i = 0; i < selected.size(); ++i) {
                auto original_pos = original_positions_[i];
                selected[i]->set_position(original_pos.x() + snap_dx, original_pos.y() + snap_dy);
            }
        } else {
            for (size_t i = 0; i < selected.size(); ++i) {
                auto original_pos = original_positions_[i];
                float new_x = original_pos.x() + delta.x();
                float new_y = original_pos.y() + delta.y();

                if (canvas_->is_snap_to_grid()) {
                    auto snapped = canvas_->snap_to_grid(flex::Vec2(new_x, new_y));
                    new_x = snapped.x();
                    new_y = snapped.y();
                }
                selected[i]->set_position(new_x, new_y);
            }
        }
        return true;
    }

    return false;
}

bool SelectTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (drag_mode_ == DragMode::Rotate) {
        const auto& selected = selection_->selection();
        bool rotated = false;
        std::vector<float> new_rotations;
        
        for (size_t i = 0; i < selected.size(); ++i) {
            new_rotations.push_back(selected[i]->rotation());
            if (std::abs(new_rotations[i] - original_rotations_[i]) > 0.1f) {
                rotated = true;
            }
        }

        if (rotated && commands_) {
            for (size_t i = 0; i < selected.size(); ++i) {
                selected[i]->set_rotation(original_rotations_[i]);
            }
            auto cmd = std::make_unique<RotateCommand>(
                std::vector<flex::Node*>(selected.begin(), selected.end()),
                original_rotations_, new_rotations);
            commands_->execute(std::move(cmd));
        }

        drag_mode_ = DragMode::None;
        active_handle_ = HandleType::None;
        return true;
    }

    if (drag_mode_ == DragMode::Resize) {
        drag_mode_ = DragMode::None;
        active_handle_ = HandleType::None;
        return true;
    }

    if (drag_mode_ == DragMode::Marquee) {
        marquee_end_ = world_pos;
        for (auto* node : get_nodes_in_marquee()) {
            selection_->add_to_selection(node);
        }
        drag_mode_ = DragMode::None;
        return true;
    }

    if (drag_mode_ == DragMode::Move) {
        if (snap_helper_) {
            snap_helper_->clear_guides();
        }

        const auto& selected = selection_->selection();
        bool moved = false;
        std::vector<flex::Vec2> new_positions;
        
        for (size_t i = 0; i < selected.size(); ++i) {
            new_positions.push_back(flex::Vec2(selected[i]->x(), selected[i]->y()));
            if (new_positions[i].x() != original_positions_[i].x() ||
                new_positions[i].y() != original_positions_[i].y()) {
                moved = true;
            }
        }

        if (moved && commands_) {
            for (size_t i = 0; i < selected.size(); ++i) {
                selected[i]->set_position(original_positions_[i].x(), original_positions_[i].y());
            }
            auto cmd = std::make_unique<MoveCommand>(
                std::vector<flex::Node*>(selected.begin(), selected.end()),
                original_positions_, new_positions);
            commands_->execute(std::move(cmd));
        }

        drag_mode_ = DragMode::None;
        return true;
    }

    return false;
}

bool SelectTool::on_key_down(int key, int mods) {
    shift_pressed_ = has_shift(mods);
    return false;
}

bool SelectTool::on_key_up(int key, int mods) {
    shift_pressed_ = has_shift(mods);
    return false;
}

void SelectTool::render_overlay(flex::Renderer& renderer) {
    selection_->render_selection_indicators(renderer);
    render_snap_guides(renderer);

    if (drag_mode_ == DragMode::Marquee) {
        render_marquee(renderer);
    }
}

void SelectTool::render_screen_overlay(flex::Renderer& renderer) {
}

bool SelectTool::is_double_click(const flex::Vec2& pos) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time_).count();

    float dx = pos.x() - last_click_pos_.x();
    float dy = pos.y() - last_click_pos_.y();
    float dist_sq = dx * dx + dy * dy;

    return (elapsed < 400 && dist_sq < 100.0f);
}

void SelectTool::apply_rotate(const flex::Vec2& screen_pos) {
    float dx = screen_pos.x() - rotation_center_.x();
    float dy = screen_pos.y() - rotation_center_.y();
    float current_angle = std::atan2(dy, dx) * 180.0f / 3.14159f;

    float start_dx = drag_start_screen_.x() - rotation_center_.x();
    float start_dy = drag_start_screen_.y() - rotation_center_.y();
    float start_angle = std::atan2(start_dy, start_dx) * 180.0f / 3.14159f;

    float delta_angle = current_angle - start_angle;

    if (shift_pressed_) {
        delta_angle = std::round(delta_angle / 15.0f) * 15.0f;
    }

    const auto& selected = selection_->selection();
    for (size_t i = 0; i < selected.size(); ++i) {
        selected[i]->set_rotation(original_rotations_[i] + delta_angle);
    }
}

void SelectTool::apply_resize(const flex::Vec2& screen_pos) {
    flex::Vec2 world_pos = canvas_->screen_to_world(screen_pos);

    if (canvas_->is_snap_to_grid()) {
        world_pos = canvas_->snap_to_grid(world_pos);
    }

    flex::Vec2 start_world = canvas_->screen_to_world(drag_start_screen_);
    float dx = world_pos.x() - start_world.x();
    float dy = world_pos.y() - start_world.y();

    float ox = original_bounds_.x;
    float oy = original_bounds_.y;
    float ow = original_bounds_.width;
    float oh = original_bounds_.height;
    float nx = ox, ny = oy, nw = ow, nh = oh;

    switch (active_handle_) {
        case HandleType::TopLeft:     nx = ox + dx; ny = oy + dy; nw = ow - dx; nh = oh - dy; break;
        case HandleType::TopCenter:   ny = oy + dy; nh = oh - dy; break;
        case HandleType::TopRight:    ny = oy + dy; nw = ow + dx; nh = oh - dy; break;
        case HandleType::RightCenter: nw = ow + dx; break;
        case HandleType::BottomRight: nw = ow + dx; nh = oh + dy; break;
        case HandleType::BottomCenter: nh = oh + dy; break;
        case HandleType::BottomLeft:  nx = ox + dx; nw = ow - dx; nh = oh + dy; break;
        case HandleType::LeftCenter:  nx = ox + dx; nw = ow - dx; break;
        default: break;
    }

    // Constrain proportions if Shift pressed
    if (shift_pressed_ && ow > 0 && oh > 0) {
        float aspect = ow / oh;
        bool is_corner = (active_handle_ == HandleType::TopLeft ||
                         active_handle_ == HandleType::TopRight ||
                         active_handle_ == HandleType::BottomLeft ||
                         active_handle_ == HandleType::BottomRight);
        if (is_corner) {
            if (std::abs(nw / aspect) > std::abs(nh)) {
                nh = nw / aspect;
            } else {
                nw = nh * aspect;
            }
            if (active_handle_ == HandleType::TopLeft) { nx = ox + ow - nw; ny = oy + oh - nh; }
            else if (active_handle_ == HandleType::TopRight) { ny = oy + oh - nh; }
            else if (active_handle_ == HandleType::BottomLeft) { nx = ox + ow - nw; }
        }
    }

    // Minimum size
    const float min_size = 5.0f;
    if (nw < min_size) {
        if (active_handle_ == HandleType::TopLeft || active_handle_ == HandleType::BottomLeft ||
            active_handle_ == HandleType::LeftCenter) {
            nx = ox + ow - min_size;
        }
        nw = min_size;
    }
    if (nh < min_size) {
        if (active_handle_ == HandleType::TopLeft || active_handle_ == HandleType::TopCenter ||
            active_handle_ == HandleType::TopRight) {
            ny = oy + oh - min_size;
        }
        nh = min_size;
    }

    float scale_x = nw / ow;
    float scale_y = nh / oh;

    const auto& selected = selection_->selection();
    for (size_t i = 0; i < selected.size(); ++i) {
        auto* node = selected[i];
        const auto& orig_pos = original_positions_[i];
        const auto& orig_size = original_sizes_[i];

        float rel_x = ow > 0 ? (orig_pos.x() - ox) / ow : 0;
        float rel_y = oh > 0 ? (orig_pos.y() - oy) / oh : 0;
        node->set_position(nx + rel_x * nw, ny + rel_y * nh);

        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            float new_w = orig_size.x() * scale_x;
            float new_h = orig_size.y() * scale_y;
            float new_radius = (orig_size.x() + orig_size.y()) / 4 * (scale_x + scale_y) / 2;

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
                default: break;
            }
        }
    }
}

void SelectTool::render_snap_guides(flex::Renderer& renderer) {
    if (!snap_helper_) return;

    const auto& guides = snap_helper_->guides();
    if (guides.empty()) return;

    flex::Paint guide_paint = flex::Paint::solid(flex::Color(0.0f, 0.8f, 1.0f, 0.8f));

    for (const auto& guide : guides) {
        std::string path;
        if (guide.type == SnapGuide::Type::Vertical) {
            path = "M " + std::to_string(guide.position) + " -10000 L " + 
                   std::to_string(guide.position) + " 10000";
        } else {
            path = "M -10000 " + std::to_string(guide.position) + " L 10000 " + 
                   std::to_string(guide.position);
        }
        renderer.stroke_path(path, guide_paint, 1.0f);
    }
}

void SelectTool::render_marquee(flex::Renderer& renderer) {
    float x = std::min(drag_start_world_.x(), marquee_end_.x());
    float y = std::min(drag_start_world_.y(), marquee_end_.y());
    float w = std::abs(marquee_end_.x() - drag_start_world_.x());
    float h = std::abs(marquee_end_.y() - drag_start_world_.y());

    flex::Paint fill = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.9f, 0.15f});
    flex::Paint stroke = flex::Paint::solid(flex::Color{0.3f, 0.5f, 0.9f, 0.8f});

    renderer.draw_rect(x, y, w, h, 0, fill, stroke, 1.0f);

    char dim_text[32];
    stbsp_snprintf(dim_text, sizeof(dim_text), "%.0f x %.0f", w, h);
    renderer.draw_text(dim_text, x + w / 2 - 20, y - 14,
                      "Arial", 11.0f, false, flex::Color(0.4f, 0.4f, 0.4f, 1.0f));
}

std::vector<flex::Node*> SelectTool::get_nodes_in_marquee() {
    std::vector<flex::Node*> result;

    float x1 = std::min(drag_start_world_.x(), marquee_end_.x());
    float y1 = std::min(drag_start_world_.y(), marquee_end_.y());
    float x2 = std::max(drag_start_world_.x(), marquee_end_.x());
    float y2 = std::max(drag_start_world_.y(), marquee_end_.y());

    flex::Bounds marquee{x1, y1, x2 - x1, y2 - y1};

    for (auto* layer : canvas_->get_all_layers()) {
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
