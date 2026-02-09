#include "meta_editor/view/cad_top_bar.h"
#include "meta_editor/view/icon_system.h"
#include <cstdio>

namespace meta_editor {

CADTopBar::CADTopBar() {
    set_draggable(false);
    
    // Group 1: File/Print
    groups_.push_back({"File", {"shape-file", "world-folder", "world-folder-o", "map-print", "file-upload"}, 10, 0});
    // Group 2: Edit
    groups_.push_back({"Edit", {"undo", "redo", "content-cut", "copy-point", "content-paste"}, 0, 0});
    // Group 3: View
    groups_.push_back({"View", {"zoom-in", "zoom-out", "zoom-out-map", "pan-tool"}, 0, 0});

    // Row 2 properties
    properties_ = {
        {"Layer:", "0", 10, 150},
        {"Color:", "ByLayer", 170, 120},
        {"Line type:", "ByLayer", 300, 150},
        {"Line weight:", "ByLayer", 460, 150}
    };
}

void CADTopBar::set_layout(float x, float y, float w) {
    set_position(x, y);
    set_size(w, content_height());
    
    // Distribute groups in row 1
    float cx = x + 10;
    for (auto& g : groups_) {
        g.x_start = cx;
        cx += g.icons.size() * (icon_size_ + 8) + 16;
        g.x_end = cx - 8;
    }
}

void CADTopBar::render(flex::Renderer& r) {
    if (!visible_) return;

    // Draw main BG
    r.draw_rect(x_, y_, width_, height_, 0, flex::Paint::solid({0.15f, 0.15f, 0.17f, 1.0f}), flex::Paint::none(), 0);

    render_row1(r);
    render_row2(r);
    
    // Bottom border
    r.draw_rect(x_, y_ + height_ - 1, width_, 1, 0, flex::Paint::solid({0.1f, 0.1f, 0.12f, 1.0f}), flex::Paint::none(), 0);
}

void CADTopBar::render_row1(flex::Renderer& r) {
    float ry = y_ + 4;
    
    for (const auto& g : groups_) {
        // Group highlight
        r.draw_rect(g.x_start, ry, g.x_end - g.x_start, row1_h_ - 8, 4, flex::Paint::solid({0.18f, 0.18f, 0.20f, 1.0f}), flex::Paint::none(), 0);
        
        float cx = g.x_start + 4;
        for (const auto& icon : g.icons) {
            // Icon box
            r.draw_rect(cx, ry + 2, icon_size_, icon_size_, 2, flex::Paint::solid({0.25f, 0.25f, 0.27f, 1.0f}), flex::Paint::none(), 0);
            
            // Icon
            if (icons_) {
                icons_->render_icon(r, icon, cx + 4, ry + 6, icon_size_ - 8, {0.9f, 0.9f, 0.9f, 1.0f});
            } else {
                // Simple placeholder sketch
                r.draw_rect(cx + 4, ry + 6, icon_size_ - 8, icon_size_ - 8, 1, flex::Paint::none(), flex::Paint::solid({0.9f, 0.9f, 0.9f, 1.0f}), 1.2f);
            }
            
            cx += icon_size_ + 8;
        }
    }
    
    // Separator
    r.draw_rect(x_, y_ + row1_h_, width_, 1, 0, flex::Paint::solid({0.12f, 0.12f, 0.14f, 1.0f}), flex::Paint::none(), 0);
}

void CADTopBar::render_row2(flex::Renderer& r) {
    float ry = y_ + row1_h_ + 6;
    
    for (const auto& p : properties_) {
        r.draw_text(p.label.c_str(), x_ + p.x, ry + 4, "Arial", 10, false, {0.6f, 0.6f, 0.6f, 1.0f});
        
        float box_x = x_ + p.x + (p.label.size() * 6) + 4;
        if (p.label == "Layer:") box_x = x_ + p.x + 40; // Adjust for "Layer:"
        
        float box_w = p.w;
        
        // Dropdown box
        r.draw_rect(box_x, ry, box_w, 20, 2, flex::Paint::solid({0.12f, 0.12f, 0.14f, 1.0f}), flex::Paint::solid({0.25f, 0.25f, 0.27f, 1.0f}), 1.0f);
        
        // Text in box
        r.draw_text(p.value.c_str(), box_x + 6, ry + 4, "Arial", 10, false, {0.8f, 0.8f, 0.8f, 1.0f});
        
        // Down arrow
        float ax = box_x + box_w - 12;
        float ay = ry + 8;
        r.stroke_path("M " + std::to_string(ax) + " " + std::to_string(ay) + " L " + std::to_string(ax + 4) + " " + std::to_string(ay + 4) + " L " + std::to_string(ax + 8) + " " + std::to_string(ay), flex::Paint::solid({0.5f, 0.5f, 0.5f, 1.0f}), 1.5f);
    }
}

bool CADTopBar::handle_click(float mx, float my) {
    if (!contains(mx, my)) return false;
    return true; // Consume
}

} // namespace meta_editor
