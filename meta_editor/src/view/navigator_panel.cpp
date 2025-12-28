/*
 * Meta Editor - Navigator Panel Implementation
 */

#include "meta_editor/view/navigator_panel.h"
#include "meta_editor/canvas.h"
#include <algorithm>
#include <cmath>

namespace meta_editor {

NavigatorPanel::NavigatorPanel(Canvas* canvas) : canvas_(canvas) {
    // Set default size
    width_ = 150;
    height_ = 100;

    // Darker style for navigator
    style_.background = {0.15f, 0.15f, 0.15f, 0.9f};
    style_.corner_radius = 4.0f;
}

void NavigatorPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    // Panel background using base class
    render_background(renderer);

    // Calculate content bounds (world space)
    // For now, use a fixed world area centered at origin
    float world_extent = 1000.0f;
    content_bounds_ = {-world_extent/2, -world_extent/2, world_extent, world_extent};

    // Scale factor from world to navigator
    float scale_x = (width_ - 8) / content_bounds_.width;
    float scale_y = (height_ - 8) / content_bounds_.height;
    float scale = std::min(scale_x, scale_y);

    float nav_content_w = content_bounds_.width * scale;
    float nav_content_h = content_bounds_.height * scale;
    float nav_content_x = x_ + 4 + (width_ - 8 - nav_content_w) / 2;
    float nav_content_y = y_ + 4 + (height_ - 8 - nav_content_h) / 2;

    // Draw content area background
    flex::Paint content_bg = flex::Paint::solid(flex::Color(0.2f, 0.2f, 0.2f, 1.0f));
    renderer.draw_rect(nav_content_x, nav_content_y, nav_content_w, nav_content_h, 2, content_bg, flex::Paint::none(), 0);

    // Draw shapes as tiny dots/rectangles
    auto* root = canvas_->content_root();
    if (root) {
        flex::Paint shape_fill = flex::Paint::solid(flex::Color(0.5f, 0.7f, 0.9f, 0.6f));
        for (auto* child : root->children()) {
            if (child->type() == flex::NodeType::Group) {
                auto* group = static_cast<flex::Group*>(child);
                for (auto* node : group->children()) {
                    auto bounds = node->world_bounds();
                    float sx = nav_content_x + (bounds.x - content_bounds_.x) * scale;
                    float sy = nav_content_y + (bounds.y - content_bounds_.y) * scale;
                    float sw = std::max(2.0f, bounds.width * scale);
                    float sh = std::max(2.0f, bounds.height * scale);
                    renderer.draw_rect(sx, sy, sw, sh, 1, shape_fill, flex::Paint::none(), 0);
                }
            }
        }
    }

    // Calculate viewport rectangle in navigator space
    float vp_world_x = -canvas_->camera_pan_x() / canvas_->camera_zoom();
    float vp_world_y = -canvas_->camera_pan_y() / canvas_->camera_zoom();
    float vp_world_w = canvas_->width() / canvas_->camera_zoom();
    float vp_world_h = canvas_->height() / canvas_->camera_zoom();

    float vp_nav_x = nav_content_x + (vp_world_x - content_bounds_.x) * scale;
    float vp_nav_y = nav_content_y + (vp_world_y - content_bounds_.y) * scale;
    float vp_nav_w = vp_world_w * scale;
    float vp_nav_h = vp_world_h * scale;

    // Draw viewport rectangle
    flex::Paint vp_fill = flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 0.1f));
    flex::Paint vp_border = flex::Paint::solid(flex::Color(0.4f, 0.6f, 1.0f, 0.8f));
    renderer.draw_rect(vp_nav_x, vp_nav_y, vp_nav_w, vp_nav_h, 0, vp_fill, vp_border, 1.5f);
}

bool NavigatorPanel::handle_click(float mx, float my) {
    if (!contains(mx, my)) return false;

    // Start dragging
    dragging_ = true;
    return handle_drag(mx, my);
}

bool NavigatorPanel::handle_drag(float mx, float my) {
    if (!dragging_ || !visible_) return false;

    // Convert navigator position to world position
    auto world_pos = navigator_to_world(mx, my);

    // Center viewport on this world position
    float vp_world_w = canvas_->width() / canvas_->camera_zoom();
    float vp_world_h = canvas_->height() / canvas_->camera_zoom();

    float new_pan_x = -(world_pos.x() - vp_world_w / 2) * canvas_->camera_zoom();
    float new_pan_y = -(world_pos.y() - vp_world_h / 2) * canvas_->camera_zoom();

    // Calculate delta and apply
    float dx = new_pan_x - canvas_->camera_pan_x();
    float dy = new_pan_y - canvas_->camera_pan_y();
    canvas_->pan(dx, dy);

    return true;
}

void NavigatorPanel::end_drag() {
    dragging_ = false;
}

flex::Vec2 NavigatorPanel::navigator_to_world(float nx, float ny) const {
    // Scale factor from world to navigator
    float scale_x = (width_ - 8) / content_bounds_.width;
    float scale_y = (height_ - 8) / content_bounds_.height;
    float scale = std::min(scale_x, scale_y);

    float nav_content_w = content_bounds_.width * scale;
    float nav_content_h = content_bounds_.height * scale;
    float nav_content_x = x_ + 4 + (width_ - 8 - nav_content_w) / 2;
    float nav_content_y = y_ + 4 + (height_ - 8 - nav_content_h) / 2;

    // Convert from navigator to world
    float world_x = content_bounds_.x + (nx - nav_content_x) / scale;
    float world_y = content_bounds_.y + (ny - nav_content_y) / scale;

    return {world_x, world_y};
}

} // namespace meta_editor
