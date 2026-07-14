/*
 * Meta Editor - Snap Helper Implementation
 */

#include "meta_editor/snap_helper.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include <flex/runtime/node.h>
#include <flex/runtime/group.h>
#include <algorithm>
#include <cmath>

namespace meta_editor {

SnapHelper::SnapHelper(Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {}

void SnapHelper::collect_snap_targets(const std::vector<flex::Node*>& excluded) {
    targets_.clear();

    auto layers = canvas_->get_all_layers();
    for (auto* layer : layers) {
        for (auto* child : layer->children()) {
            // Skip excluded nodes (being dragged)
            bool is_excluded = false;
            for (auto* ex : excluded) {
                if (child == ex) {
                    is_excluded = true;
                    break;
                }
            }
            if (is_excluded) continue;

            auto bounds = child->bounds();
            float x = child->x();
            float y = child->y();

            SnapTarget target;
            target.left = x;
            target.right = x + bounds.width;
            target.center_x = x + bounds.width / 2;
            target.top = y;
            target.bottom = y + bounds.height;
            target.center_y = y + bounds.height / 2;
            targets_.push_back(target);
        }
    }
}

SnapResult SnapHelper::snap_point(const flex::Vec2& world_pos,
                                  const std::vector<flex::Node*>& excluded) {
    guides_.clear();
    collect_snap_targets(excluded);

    SnapResult result;
    result.snapped_pos = world_pos;

    // Convert threshold from screen to world space
    float world_threshold = threshold_ / canvas_->camera_zoom();

    float best_dx = world_threshold;
    float best_dy = world_threshold;

    // Snap to objects
    if (snap_to_objects_) {
        for (const auto& target : targets_) {
            // Check X snaps (vertical guides)
            float dx_left = std::abs(world_pos.x - target.left);
            float dx_center = std::abs(world_pos.x - target.center_x);
            float dx_right = std::abs(world_pos.x - target.right);

            if (dx_left < best_dx) {
                best_dx = dx_left;
                result.snapped_pos = flex::Vec2(target.left, result.snapped_pos.y);
                result.snapped_x = true;
                result.guide_x = target.left;
            }
            if (dx_center < best_dx) {
                best_dx = dx_center;
                result.snapped_pos = flex::Vec2(target.center_x, result.snapped_pos.y);
                result.snapped_x = true;
                result.guide_x = target.center_x;
            }
            if (dx_right < best_dx) {
                best_dx = dx_right;
                result.snapped_pos = flex::Vec2(target.right, result.snapped_pos.y);
                result.snapped_x = true;
                result.guide_x = target.right;
            }

            // Check Y snaps (horizontal guides)
            float dy_top = std::abs(world_pos.y - target.top);
            float dy_center = std::abs(world_pos.y - target.center_y);
            float dy_bottom = std::abs(world_pos.y - target.bottom);

            if (dy_top < best_dy) {
                best_dy = dy_top;
                result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.top);
                result.snapped_y = true;
                result.guide_y = target.top;
            }
            if (dy_center < best_dy) {
                best_dy = dy_center;
                result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.center_y);
                result.snapped_y = true;
                result.guide_y = target.center_y;
            }
            if (dy_bottom < best_dy) {
                best_dy = dy_bottom;
                result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.bottom);
                result.snapped_y = true;
                result.guide_y = target.bottom;
            }
        }
    }

    // Snap to grid
    if (snap_to_grid_ && canvas_->is_snap_to_grid()) {
        float grid_size = canvas_->grid_size();
        float snapped_x = std::round(world_pos.x / grid_size) * grid_size;
        float snapped_y = std::round(world_pos.y / grid_size) * grid_size;

        float dx_grid = std::abs(world_pos.x - snapped_x);
        float dy_grid = std::abs(world_pos.y - snapped_y);

        if (dx_grid < best_dx) {
            result.snapped_pos = flex::Vec2(snapped_x, result.snapped_pos.y);
            result.snapped_x = true;
            result.guide_x = snapped_x;
        }
        if (dy_grid < best_dy) {
            result.snapped_pos = flex::Vec2(result.snapped_pos.x, snapped_y);
            result.snapped_y = true;
            result.guide_y = snapped_y;
        }
    }

    // Create guide lines for rendering
    if (result.snapped_x) {
        SnapGuide guide;
        guide.type = SnapGuide::Type::Vertical;
        guide.position = result.guide_x;
        guide.start = -10000;  // Extend across canvas
        guide.end = 10000;
        guides_.push_back(guide);
    }
    if (result.snapped_y) {
        SnapGuide guide;
        guide.type = SnapGuide::Type::Horizontal;
        guide.position = result.guide_y;
        guide.start = -10000;
        guide.end = 10000;
        guides_.push_back(guide);
    }

    return result;
}

SnapResult SnapHelper::snap_bounds(const flex::Bounds& bounds,
                                   const std::vector<flex::Node*>& excluded) {
    guides_.clear();
    collect_snap_targets(excluded);

    SnapResult result;
    result.snapped_pos = flex::Vec2(bounds.x, bounds.y);

    float world_threshold = threshold_ / canvas_->camera_zoom();
    float best_dx = world_threshold;
    float best_dy = world_threshold;

    float my_left = bounds.x;
    float my_right = bounds.x + bounds.width;
    float my_center_x = bounds.x + bounds.width / 2;
    float my_top = bounds.y;
    float my_bottom = bounds.y + bounds.height;
    float my_center_y = bounds.y + bounds.height / 2;

    if (snap_to_objects_) {
        for (const auto& target : targets_) {
            // Left edge snaps
            float dx = std::abs(my_left - target.left);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.left, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.left; }
            dx = std::abs(my_left - target.center_x);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.center_x, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.center_x; }
            dx = std::abs(my_left - target.right);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.right, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.right; }

            // Center X snaps
            dx = std::abs(my_center_x - target.left);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.left - bounds.width / 2, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.left; }
            dx = std::abs(my_center_x - target.center_x);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.center_x - bounds.width / 2, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.center_x; }
            dx = std::abs(my_center_x - target.right);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.right - bounds.width / 2, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.right; }

            // Right edge snaps
            dx = std::abs(my_right - target.left);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.left - bounds.width, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.left; }
            dx = std::abs(my_right - target.center_x);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.center_x - bounds.width, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.center_x; }
            dx = std::abs(my_right - target.right);
            if (dx < best_dx) { best_dx = dx; result.snapped_pos = flex::Vec2(target.right - bounds.width, result.snapped_pos.y); result.snapped_x = true; result.guide_x = target.right; }

            // Top edge snaps
            float dy = std::abs(my_top - target.top);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.top); result.snapped_y = true; result.guide_y = target.top; }
            dy = std::abs(my_top - target.center_y);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.center_y); result.snapped_y = true; result.guide_y = target.center_y; }
            dy = std::abs(my_top - target.bottom);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.bottom); result.snapped_y = true; result.guide_y = target.bottom; }

            // Center Y snaps
            dy = std::abs(my_center_y - target.top);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.top - bounds.height / 2); result.snapped_y = true; result.guide_y = target.top; }
            dy = std::abs(my_center_y - target.center_y);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.center_y - bounds.height / 2); result.snapped_y = true; result.guide_y = target.center_y; }
            dy = std::abs(my_center_y - target.bottom);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.bottom - bounds.height / 2); result.snapped_y = true; result.guide_y = target.bottom; }

            // Bottom edge snaps
            dy = std::abs(my_bottom - target.top);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.top - bounds.height); result.snapped_y = true; result.guide_y = target.top; }
            dy = std::abs(my_bottom - target.center_y);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.center_y - bounds.height); result.snapped_y = true; result.guide_y = target.center_y; }
            dy = std::abs(my_bottom - target.bottom);
            if (dy < best_dy) { best_dy = dy; result.snapped_pos = flex::Vec2(result.snapped_pos.x, target.bottom - bounds.height); result.snapped_y = true; result.guide_y = target.bottom; }
        }
    }

    // Snap to grid
    if (snap_to_grid_ && canvas_->is_snap_to_grid()) {
        float grid_size = canvas_->grid_size();
        float snapped_x = std::round(bounds.x / grid_size) * grid_size;
        float snapped_y = std::round(bounds.y / grid_size) * grid_size;

        if (std::abs(bounds.x - snapped_x) < best_dx) {
            result.snapped_pos = flex::Vec2(snapped_x, result.snapped_pos.y);
            result.snapped_x = true;
        }
        if (std::abs(bounds.y - snapped_y) < best_dy) {
            result.snapped_pos = flex::Vec2(result.snapped_pos.x, snapped_y);
            result.snapped_y = true;
        }
    }

    // Create guides
    if (result.snapped_x) {
        SnapGuide guide;
        guide.type = SnapGuide::Type::Vertical;
        guide.position = result.guide_x;
        guide.start = -10000;
        guide.end = 10000;
        guides_.push_back(guide);
    }
    if (result.snapped_y) {
        SnapGuide guide;
        guide.type = SnapGuide::Type::Horizontal;
        guide.position = result.guide_y;
        guide.start = -10000;
        guide.end = 10000;
        guides_.push_back(guide);
    }

    return result;
}

} // namespace meta_editor
