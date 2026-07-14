/*
 * Meta Editor - Laser Pointer Tool Implementation
 */
#include <algorithm>
#include "meta_editor/tools/laser_tool.h"

namespace meta_editor {

bool LaserTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_active_ = true;
    cursor_pos_ = world_pos;
    trail_.clear();
    trail_.push_back({world_pos, 0.0f});
    return true;
}

bool LaserTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    cursor_pos_ = world_pos;
    if (is_active_) {
        trail_.push_back({world_pos, 0.0f});
    }
    return true;
}

bool LaserTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_active_ = false;
    return true;
}

void LaserTool::update(float dt) {
    // Age all trail points
    for (auto& p : trail_) {
        p.age += dt;
    }
    
    // Remove old points
    trail_.erase(
        std::remove_if(trail_.begin(), trail_.end(),
            [this](const TrailPoint& p) { return p.age > trail_lifetime_; }),
        trail_.end()
    );
}

void LaserTool::render_overlay(flex::Renderer& renderer) {
    // Draw trail with fading opacity
    for (size_t i = 1; i < trail_.size(); ++i) {
        float alpha = 1.0f - (trail_[i].age / trail_lifetime_);
        alpha = std::max(0.0f, std::min(1.0f, alpha));
        
        flex::Color c = color_;
        c.a = alpha * 0.6f;
        flex::Paint stroke = flex::Paint::solid(c);
        
        char path[128];
        snprintf(path, sizeof(path), "M %.1f %.1f L %.1f %.1f",
                 trail_[i-1].pos.x, trail_[i-1].pos.y,
                 trail_[i].pos.x, trail_[i].pos.y);
        renderer.stroke_path(path, stroke, 4.0f);
    }
    
    // Draw cursor dot
    flex::Paint fill = flex::Paint::solid(color_);
    flex::Paint glow = flex::Paint::solid(flex::Color{color_.r, color_.g, color_.b, 0.3f});
    
    // Glow effect
    renderer.draw_circle(cursor_pos_.x, cursor_pos_.y, point_radius_ * 2, glow, flex::Paint::none(), 0);
    // Main dot
    renderer.draw_circle(cursor_pos_.x, cursor_pos_.y, point_radius_, fill, flex::Paint::none(), 0);
}

} // namespace meta_editor
