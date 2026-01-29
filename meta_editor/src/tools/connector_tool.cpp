/*
 * Meta Editor - Connector Tool Implementation
 */

#include "meta_editor/tools/connector_tool.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include <cmath>

namespace meta_editor {

bool ConnectorTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    start_target_ = find_connection_target(world_pos);
    
    if (start_target_.node) {
        start_pos_ = start_target_.point;
    } else {
        start_pos_ = world_pos;
    }
    
    current_pos_ = start_pos_;
    is_drawing_ = true;
    return true;
}

bool ConnectorTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) {
        // Show hover feedback
        hover_target_ = find_connection_target(world_pos);
        return false;
    }
    
    hover_target_ = find_connection_target(world_pos);
    
    if (hover_target_.node && hover_target_.node != start_target_.node) {
        current_pos_ = hover_target_.point;
    } else {
        current_pos_ = world_pos;
    }
    
    return true;
}

bool ConnectorTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (!is_drawing_) return false;
    is_drawing_ = false;
    
    // Check minimum distance
    float dx = current_pos_.x() - start_pos_.x();
    float dy = current_pos_.y() - start_pos_.y();
    if (std::abs(dx) < 5 && std::abs(dy) < 5) return true;
    
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();
    
    // Create line shape
    std::string path_data = "M 0 0 L " + std::to_string(dx) + " " + std::to_string(dy);
    
    // Add arrow
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.001f) {
        float ux = dx / len;
        float uy = dy / len;
        float px = -uy;
        float py = ux;
        float arrow_size = 12.0f;
        float wing_back = arrow_size * 0.8f;
        float wing_width = arrow_size * 0.4f;
        
        float left_x = dx - ux * wing_back + px * wing_width;
        float left_y = dy - uy * wing_back + py * wing_width;
        float right_x = dx - ux * wing_back - px * wing_width;
        float right_y = dy - uy * wing_back - py * wing_width;
        
        char arrow[128];
        snprintf(arrow, sizeof(arrow), " M %.1f %.1f L %.1f %.1f M %.1f %.1f L %.1f %.1f",
                 dx, dy, left_x, left_y, dx, dy, right_x, right_y);
        path_data += arrow;
    }
    
    auto* line = flex::Shape::create(*allocator);
    line->set_path(path_data);
    line->set_position(start_pos_.x(), start_pos_.y());
    line->set_stroke(stroke_color_, stroke_width_);
    layer->add_child(line);
    
    // Create connector and bind to shapes
    if (connector_mgr_) {
        auto* conn = connector_mgr_->create_connector(line);
        
        if (start_target_.node) {
            connector_mgr_->bind_start(conn, start_target_.node, start_target_.side);
        }
        
        HitResult end_target = find_connection_target(world_pos);
        if (end_target.node && end_target.node != start_target_.node) {
            connector_mgr_->bind_end(conn, end_target.node, end_target.side);
        }
    }
    
    selection_->clear_selection();
    selection_->select(line);
    
    return true;
}

void ConnectorTool::render_overlay(flex::Renderer& renderer) {
    flex::Paint stroke = flex::Paint::solid(flex::Color(0.2f, 0.6f, 0.9f, 0.8f));
    flex::Paint point_fill = flex::Paint::solid(flex::Color(0.2f, 0.8f, 0.2f, 1.0f));
    flex::Paint hover_fill = flex::Paint::solid(flex::Color(0.2f, 0.8f, 0.2f, 0.5f));
    
    // Draw connection point indicators on hovered shape
    if (hover_target_.node && !is_drawing_) {
        auto bounds = hover_target_.node->world_bounds();
        float cx = bounds.x + bounds.width / 2;
        float cy = bounds.y + bounds.height / 2;
        
        // Draw all 4 connection points
        renderer.draw_circle(cx, bounds.y, 6.0f, hover_fill, stroke, 1.0f);  // Top
        renderer.draw_circle(cx, bounds.y + bounds.height, 6.0f, hover_fill, stroke, 1.0f);  // Bottom
        renderer.draw_circle(bounds.x, cy, 6.0f, hover_fill, stroke, 1.0f);  // Left
        renderer.draw_circle(bounds.x + bounds.width, cy, 6.0f, hover_fill, stroke, 1.0f);  // Right
    }
    
    if (!is_drawing_) return;
    
    // Draw preview line
    float dx = current_pos_.x() - start_pos_.x();
    float dy = current_pos_.y() - start_pos_.y();
    
    char path[128];
    snprintf(path, sizeof(path), "M %.1f %.1f L %.1f %.1f",
             start_pos_.x(), start_pos_.y(), current_pos_.x(), current_pos_.y());
    renderer.stroke_path(path, stroke, 2.0f);
    
    // Draw arrow preview
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 10.0f) {
        float ux = dx / len;
        float uy = dy / len;
        float px = -uy;
        float py = ux;
        float arrow_size = 12.0f;
        float wing_back = arrow_size * 0.8f;
        float wing_width = arrow_size * 0.4f;
        
        float tip_x = current_pos_.x();
        float tip_y = current_pos_.y();
        float left_x = tip_x - ux * wing_back + px * wing_width;
        float left_y = tip_y - uy * wing_back + py * wing_width;
        float right_x = tip_x - ux * wing_back - px * wing_width;
        float right_y = tip_y - uy * wing_back - py * wing_width;
        
        char arrow[256];
        snprintf(arrow, sizeof(arrow), "M %.1f %.1f L %.1f %.1f M %.1f %.1f L %.1f %.1f",
                 tip_x, tip_y, left_x, left_y, tip_x, tip_y, right_x, right_y);
        renderer.stroke_path(arrow, stroke, 2.0f);
    }
    
    // Draw start point
    renderer.draw_circle(start_pos_.x(), start_pos_.y(), 5.0f, point_fill, stroke, 1.0f);
    
    // Draw end point (highlight if snapping to shape)
    if (hover_target_.node && hover_target_.node != start_target_.node) {
        renderer.draw_circle(current_pos_.x(), current_pos_.y(), 6.0f, point_fill, stroke, 2.0f);
    } else {
        renderer.draw_circle(current_pos_.x(), current_pos_.y(), 4.0f, 
                            flex::Paint::solid(flex::Color(1.0f, 0.4f, 0.4f, 1.0f)), stroke, 1.0f);
    }
}

ConnectorTool::HitResult ConnectorTool::find_connection_target(const flex::Vec2& world_pos) {
    HitResult result;
    
    auto* hit = canvas_->hit_test(world_pos);
    if (!hit || hit->type() == flex::NodeType::Group) {
        return result;
    }
    
    result.node = hit;
    result.side = closest_side(hit, world_pos);
    result.point = Connector::get_connection_point(hit, result.side);
    
    return result;
}

ConnectionSide ConnectorTool::closest_side(flex::Node* node, const flex::Vec2& pos) {
    auto bounds = node->world_bounds();
    float cx = bounds.x + bounds.width / 2;
    float cy = bounds.y + bounds.height / 2;
    
    // Calculate distance to each side's center
    float dist_top = std::abs(pos.y() - bounds.y) + std::abs(pos.x() - cx) * 0.5f;
    float dist_bottom = std::abs(pos.y() - (bounds.y + bounds.height)) + std::abs(pos.x() - cx) * 0.5f;
    float dist_left = std::abs(pos.x() - bounds.x) + std::abs(pos.y() - cy) * 0.5f;
    float dist_right = std::abs(pos.x() - (bounds.x + bounds.width)) + std::abs(pos.y() - cy) * 0.5f;
    
    float min_dist = dist_top;
    ConnectionSide side = ConnectionSide::Top;
    
    if (dist_bottom < min_dist) { min_dist = dist_bottom; side = ConnectionSide::Bottom; }
    if (dist_left < min_dist) { min_dist = dist_left; side = ConnectionSide::Left; }
    if (dist_right < min_dist) { min_dist = dist_right; side = ConnectionSide::Right; }
    
    return side;
}

} // namespace meta_editor
