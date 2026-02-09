/*
 * Meta Editor - Top Bar Implementation
 */

#include "meta_editor/view/top_bar.h"
#include <sstream>

namespace meta_editor {

TopBar::TopBar(Editor* editor) : editor_(editor) {
    set_draggable(false);
    style_.background = {0.2f, 0.2f, 0.22f, 1.0f};
    style_.border = {0.1f, 0.1f, 0.12f, 1.0f};
    style_.corner_radius = 0.0f;
    style_.border_width = 0.0f;
    
    // Define buttons
    buttons_ = {
        {"New", "file-new", 0, 40},
        {"Open", "file-open", 0, 45},
        {"Save", "file-save", 0, 45},
        // Separator
        {"Undo", "edit-undo", 0, 45},
        {"Redo", "edit-redo", 0, 45}
    };
}

void TopBar::set_layout(float x, float y, float w) {
    set_position(x, y);
    set_size(w, content_height());
    
    // Layout buttons
    float cx = x + padding_;
    
    for (size_t i = 0; i < buttons_.size(); ++i) {
        if (i == 3) cx += 10.0f; // Gap before Undo
        
        buttons_[i].x = cx;
        cx += buttons_[i].width + gap_;
    }
}

void TopBar::draw_icon(flex::Renderer& r, const std::string& name, float cx, float cy, float size) {
    std::ostringstream ss;
    float s = size * 0.4f;
    
    if (name == "file-new") {
        // Simple page icon
        r.draw_rect(cx - s*0.7f, cy - s, s*1.4f, s*2.0f, 1, flex::Paint::none(), flex::Paint::solid({1,1,1,1}), 1.5f);
        // Plus sign
        /*
        r.draw_rect(cx - 1, cy - s*0.2f, 2, s*0.4f, 0, flex::Paint::solid({1,1,1,1}), flex::Paint::none(), 0);
        r.draw_rect(cx - s*0.2f, cy - 1, s*0.4f, 2, 0, flex::Paint::solid({1,1,1,1}), flex::Paint::none(), 0);
        */
    }
    else if (name == "file-open") {
        // Folder
        r.draw_rect(cx - s, cy - s*0.5f, s*2, s*1.4f, 1, flex::Paint::none(), flex::Paint::solid({1,1,1,1}), 1.5f);
        r.draw_rect(cx - s, cy - s*0.9f, s*0.8f, s*0.4f, 1, flex::Paint::solid({1,1,1,0.5f}), flex::Paint::none(), 0);
    }
    else if (name == "file-save") {
        // Floppy disk shape
        r.draw_rect(cx - s*0.8f, cy - s*0.8f, s*1.6f, s*1.6f, 1, flex::Paint::none(), flex::Paint::solid({1,1,1,1}), 1.5f);
        r.draw_rect(cx - s*0.4f, cy - s*0.8f, s*0.8f, s*0.6f, 0, flex::Paint::solid({1,1,1,1}), flex::Paint::none(), 0);
    }
    else if (name == "edit-undo") {
        // Left arrow
        ss << "M " << (cx + s*0.5f) << " " << (cy - s*0.5f)
           << " L " << (cx - s*0.5f) << " " << (cy)
           << " L " << (cx + s*0.5f) << " " << (cy + s*0.5f);
        r.stroke_path(ss.str(), flex::Paint::solid({1,1,1,1}), 1.5f);
    }
    else if (name == "edit-redo") {
        // Right arrow
        ss << "M " << (cx - s*0.5f) << " " << (cy - s*0.5f)
           << " L " << (cx + s*0.5f) << " " << (cy)
           << " L " << (cx - s*0.5f) << " " << (cy + s*0.5f);
        r.stroke_path(ss.str(), flex::Paint::solid({1,1,1,1}), 1.5f);
    }
}

void TopBar::render(flex::Renderer& renderer) {
    if (!visible_) return;
    
    // Draw background
    render_background(renderer);
    
    // Draw buttons
    for (const auto& btn : buttons_) {
        float cx = btn.x + btn.width / 2;
        float cy = y_ + content_height() / 2;
        
        bool hover = false; // TODO: Implement hover state if mouse pos known
        
        flex::Color bg_col = hover ? flex::Color{0.3f, 0.3f, 0.35f, 1.0f} : flex::Color{0.25f, 0.25f, 0.27f, 1.0f};
        
        renderer.draw_rect(btn.x, y_ + (content_height() - btn_height_)/2, btn.width, btn_height_, 4,
                          flex::Paint::solid(bg_col), flex::Paint::none(), 0);
                          
        draw_icon(renderer, btn.icon, btn.x + 16, cy, 20);
        
        // Label (assuming text rendering exists, but if not, omit)
        // renderer.draw_text(btn.label, btn.x + 30, cy + 4, flex::Paint::solid({0.9, 0.9, 0.9, 1}), 12);
    }
    
    // Separator line
    renderer.draw_rect(x_, y_ + content_height() - 1, width_, 1, 0, flex::Paint::solid(style_.border), flex::Paint::none(), 0);
}

bool TopBar::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;
    
    printf("[TopBar] Clicked at (%.2f, %.2f) | Bar Pos: (%.2f, %.2f) Width: %.2f\n", screen_x, screen_y, x_, y_, width_);

    // Check buttons
    for (const auto& btn : buttons_) {
        float btn_y = y_ + (content_height() - btn_height_)/2;
        printf("  Button '%s' bounds: x=[%.2f, %.2f] y=[%.2f, %.2f]\n", 
               btn.label.c_str(), btn.x, btn.x + btn.width, btn_y, btn_y + btn_height_);
        if (screen_x >= btn.x && screen_x <= btn.x + btn.width &&
            screen_y >= btn_y && screen_y <= btn_y + btn_height_) {
            
            if (btn.label == "New" && on_new) on_new();
            else if (btn.label == "Open" && on_open) on_open();
            else if (btn.label == "Save" && on_save) on_save(""); // Pass empty to prompt or current
            else if (btn.label == "Undo") editor_->command_manager()->undo();
            else if (btn.label == "Redo") editor_->command_manager()->redo();
            
            editor_->notify_change();
            return true;
        }
    }
    return true; // Consume click on bar
}

} // namespace meta_editor
