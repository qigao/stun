#include "meta_editor/view/cad_bottom_bar.h"
#include "meta_editor/view/icon_system.h"
#include <cstdio>
#include <string>

namespace meta_editor {

CADBottomBar::CADBottomBar() {
    set_draggable(false);
    
    // Snapping icons (verified placeholders)
    // magnet -> nest-cam-magnet-mount (Material) or similar
    // ortho -> snap-ortho (GIS)
    // direction -> direction (GIS)
    // points -> point (GIS)
    // otrack -> eye-tracking-rounded (Material) or track-changes
    // ducs -> grid-on (Material)
    // dyn -> spatial-tracking (Material) or dynamic-feed
    
    // Using verified keys:
    std::vector<std::string> icons = {
        "nest-cam-magnet-mount", // Magnet
        "snap-ortho",            // Ortho
        "direction",             // Polar
        "point",                 // Osnap points
        "track-changes",         // Otrack
        "grid-on",               // DUCS (Grid)
        "spatial-tracking",      // Dyn input
        "polyline-pt",           // LWT
        "search-poi"             // QP
    };
    for (const auto& icon : icons) {
        snap_tools_.push_back({icon, false, 0});
    }
}

void CADBottomBar::set_layout(float x, float y, float w) {
    set_position(x, y);
    set_size(w, content_height());
    
    // Layout snap tools in center
    float total_w = snap_tools_.size() * 28;
    float start_x = x + (w - total_w) / 2;
    for (size_t i = 0; i < snap_tools_.size(); ++i) {
        snap_tools_[i].x = start_x + i * 28;
    }
}

void CADBottomBar::render(flex::Renderer& r) {
    if (!visible_) return;

    r.draw_rect(x_, y_, width_, height_, 0, flex::Paint::solid({0.15f, 0.15f, 0.17f, 1.0f}), flex::Paint::none(), 0);
    r.draw_rect(x_, y_, width_, 1, 0, flex::Paint::solid({0.1f, 0.1f, 0.12f, 1.0f}), flex::Paint::none(), 0);

    render_coords(r);
    render_snap_tools(r);
    render_pagination(r);
}

void CADBottomBar::render_coords(flex::Renderer& r) {
    float tx = x_ + 10;
    float ty = y_ + 10;
    
    r.draw_text("X: 0.000000", tx, ty, "Arial", 10, false, {0.6f, 0.6f, 0.6f, 1.0f});
    r.draw_text("Y: 0.000000", tx + 100, ty, "Arial", 10, false, {0.6f, 0.6f, 0.6f, 1.0f});
    
    // Mode buttons
    float bx = tx + 200;
    r.draw_rect(bx, y_ + 6, 80, 24, 4, flex::Paint::solid({0.2f, 0.22f, 0.2f, 1.0f}), flex::Paint::solid({0.3f, 0.35f, 0.3f, 1.0f}), 1.0f);
    r.draw_text("Rectangular", bx + 8, y_ + 10, "Arial", 10, false, {0.7f, 0.7f, 0.7f, 1.0f});
    
    bx += 90;
    r.draw_rect(bx, y_ + 6, 60, 24, 4, flex::Paint::solid({0.18f, 0.25f, 0.18f, 1.0f}), flex::Paint::none(), 0);
    r.draw_text("Relative", bx + 10, y_ + 10, "Arial", 10, false, {1,1,1,1});
}

void CADBottomBar::render_snap_tools(flex::Renderer& r) {
    // Snap toolbar container
    float total_w = snap_tools_.size() * 28;
    float start_x = snap_tools_[0].x - 4;
    r.draw_rect(start_x, y_ + 4, total_w + 8, 28, 4, flex::Paint::solid({0.12f, 0.12f, 0.14f, 1.0f}), flex::Paint::none(), 0);

    for (const auto& tool : snap_tools_) {
        bool active = tool.active;
        flex::Color bg = active ? flex::Color{0.18f, 0.25f, 0.18f, 1.0f} : flex::Color{0.22f, 0.22f, 0.24f, 1.0f};
        
        r.draw_rect(tool.x, y_ + 8, 20, 20, 4, flex::Paint::solid(bg), flex::Paint::none(), 0);
        
        // Icon
        if (icons_) {
            icons_->render_icon(r, tool.icon, tool.x + 2, y_ + 10, 16, {0.6f, 0.7f, 0.6f, 1.0f});
        } else {
            // Icon sketch fallback
            r.draw_rect(tool.x + 5, y_ + 13, 10, 10, 0, flex::Paint::none(), flex::Paint::solid({0.6f, 0.7f, 0.6f, 1.0f}), 1.0f);
        }
    }
}

void CADBottomBar::render_pagination(flex::Renderer& r) {
    float rx = x_ + width_ - 100;
    r.draw_rect(rx, y_ + 8, 20, 20, 4, flex::Paint::solid({0.22f, 0.22f, 0.24f, 1.0f}), flex::Paint::none(), 0);
    r.draw_text("<", rx + 7, y_ + 10, "Arial", 11, true, {0.8,0.8,0.8,1});
    
    rx += 25;
    r.draw_text("1 of 1", rx, y_ + 10, "Arial", 10, false, {0.6,0.6,0.6,1});
    
    rx += 40;
    r.draw_rect(rx, y_ + 8, 20, 20, 4, flex::Paint::solid({0.22f, 0.22f, 0.24f, 1.0f}), flex::Paint::none(), 0);
    r.draw_text(">", rx + 7, y_ + 10, "Arial", 11, true, {0.8,0.8,0.8,1});
}

bool CADBottomBar::handle_click(float mx, float my) {
    if (!contains(mx, my)) return false;
    
    for (auto& tool : snap_tools_) {
        if (mx >= tool.x && mx <= tool.x + 20 && my >= y_ + 8 && my <= y_ + 28) {
            tool.active = !tool.active;
            return true;
        }
    }
    
    return true;
}

} // namespace meta_editor
