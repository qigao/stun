/*
 * Meta Editor - Bottom Bar Implementation
 */

#include "meta_editor/view/bottom_bar.h"
#include <iomanip>
#include <sstream>

namespace meta_editor {

BottomBar::BottomBar(Editor* editor) : editor_(editor) {
    set_draggable(false);
    style_.background = {0.2f, 0.2f, 0.22f, 1.0f};
    style_.border = {0.1f, 0.1f, 0.12f, 1.0f};
    style_.corner_radius = 0.0f;
    style_.border_width = 0.0f;
}

void BottomBar::set_layout(float x, float y, float w) {
    set_position(x, y);
    set_size(w, content_height());
    
    // Layout buttons from right
    float rx = x + w - 10;
    
    snap_btn_w_ = 80;
    rx -= snap_btn_w_;
    snap_btn_x_ = rx;
    
    rx -= 10;
    grid_btn_w_ = 80;
    rx -= grid_btn_w_;
    grid_btn_x_ = rx;
}

void BottomBar::render(flex::Renderer& renderer) {
    if (!visible_) return;
    
    render_background(renderer);
    
    // Separator line top
    renderer.draw_rect(x_, y_, width_, 1, 0, flex::Paint::solid(style_.border), flex::Paint::none(), 0);
    
    // 1. Zoom Level (Left)
    /*
    std::ostringstream ss_zoom;
    ss_zoom << "Zoom: " << (int)(editor_->canvas()->camera_zoom() * 100) << "%";
    // renderer.draw_text(ss_zoom.str(), x_ + 10, y_ + 20, ...); // Text not available in this minimal set, implying iconic
    */
    
    // 2. Grid Toggle
    bool grid_on = editor_->canvas()->is_snap_to_grid(); // Reuse this for both visibility/snap usually, or separate
    flex::Color btn_col = grid_on ? flex::Color{0.4f, 0.4f, 0.8f, 1.0f} : flex::Color{0.3f, 0.3f, 0.33f, 1.0f};
    
    renderer.draw_rect(grid_btn_x_, y_ + 4, grid_btn_w_, height_ - 8, 4, flex::Paint::solid(btn_col), flex::Paint::none(), 0);
    // renderer.draw_text("GRID", ...);
    
    // Draw grid icon
    {
        float cx = grid_btn_x_ + grid_btn_w_/2;
        float cy = y_ + height_/2;
        float s = 6;
        renderer.draw_rect(cx - s, cy - s, s*2, s*2, 0, flex::Paint::none(), flex::Paint::solid({1,1,1,1}), 1);
        renderer.draw_line(cx, cy - s, cx, cy + s, flex::Paint::solid({1,1,1,1}), 1);
        renderer.draw_line(cx - s, cy, cx + s, cy, flex::Paint::solid({1,1,1,1}), 1);
    }
    
    // 3. Snap Toggle
    // (Similar implementation)
}

bool BottomBar::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;
    
    if (screen_x >= grid_btn_x_ && screen_x <= grid_btn_x_ + grid_btn_w_) {
        editor_->canvas()->set_snap_to_grid(!editor_->canvas()->is_snap_to_grid());
        editor_->canvas()->set_grid_visible(editor_->canvas()->is_snap_to_grid()); // Link for demo
        editor_->notify_change();
        return true;
    }
    
    return true;
}

void BottomBar::update() {
    // Poll anything if needed
}

} // namespace meta_editor
