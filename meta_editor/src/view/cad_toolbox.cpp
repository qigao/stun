#include "meta_editor/view/cad_toolbox.h"
#include "meta_editor/view/icon_system.h"
#include <cstdio>

namespace meta_editor {

CADToolbox::CADToolbox() {
    set_draggable(true);
    width_ = 200;
    height_ = 450;
    x_ = 10;
    y_ = 80;

    // Default "Place" tools
    add_tool("Place", {"Line", "polyline-dash", "L"});
    add_tool("Place", {"Polyline", "polyline", "PL"});
    add_tool("Place", {"Arc", "direction", "A"}); // "direction" looks like an arc arrow
    add_tool("Place", {"Circle", "circle", "C"});
    add_tool("Place", {"Rectangle", "bbox", "R"});
    add_tool("Place", {"Ellipse", "circle-o", "EL"});
    add_tool("Place", {"Polygon", "polygon", "POL"});
    add_tool("Place", {"Text", "text-fields", "T"}); // Material fallback
    add_tool("Place", {"Dim", "measure", "D"});

    add_tool("Modify", {"Move", "move", "M"});
    add_tool("Modify", {"Copy", "copy-poly", "CO"});
    add_tool("Modify", {"Rotate", "history", "RO"}); // Placeholder
    add_tool("Modify", {"Scale", "expand", "SC"});
    add_tool("Modify", {"Mirror", "flip", "MI"});
    add_tool("Modify", {"Trim", "cut", "TR"});
    add_tool("Modify", {"Extend", "arrow", "EX"});
}

void CADToolbox::add_tool(const std::string& tab, const Tool& tool) {
    tool_map_[tab].push_back(tool);
}

void CADToolbox::render(flex::Renderer& r) {
    if (!visible_) return;

    render_background(r);

    // Title Bar
    r.draw_rect(x_, y_, width_, 24, 0, flex::Paint::solid({0.12f, 0.12f, 0.14f, 1.0f}), flex::Paint::none(), 0);
    r.draw_text("Toolbox", x_ + 8, y_ + 5, "Arial", 11, false, {0.7f, 0.7f, 0.7f, 1.0f});

    render_tab_bar(r);

    float content_y = y_ + 24 + tab_height_;
    
    // Grid background
    r.draw_rect(x_ + 4, content_y, width_ - 8, 140, 4, flex::Paint::solid({0.14f, 0.14f, 0.16f, 1.0f}), flex::Paint::none(), 0);
    
    render_grid(r);
    
    render_selection_mode(r);
}

void CADToolbox::render_tab_bar(flex::Renderer& r) {
    float tx = x_ + 4;
    float ty = y_ + 24 + 4;
    
    for (const auto& tab : tabs_) {
        bool active = (tab == active_tab_);
        float tw = (width_ - 12) / 3;
        
        flex::Color bg = active ? flex::Color{0.18f, 0.25f, 0.18f, 1.0f} : flex::Color{0.14f, 0.14f, 0.16f, 1.0f};
        r.draw_rect(tx, ty, tw, tab_height_ - 4, 2, flex::Paint::solid(bg), flex::Paint::none(), 0);
        
        r.draw_text(tab.c_str(), tx + 4, ty + 6, "Arial", 10, active, {0.8f, 0.8f, 0.8f, 1.0f});
        tx += tw + 2;
    }
}

void CADToolbox::render_grid(flex::Renderer& r) {
    auto it = tool_map_.find(active_tab_);
    if (it == tool_map_.end()) return;

    float gx = x_ + padding_;
    float gy = y_ + 24 + tab_height_ + padding_;
    float cell_w = (width_ - padding_ * 2) / columns_;
    
    for (size_t i = 0; i < it->second.size(); ++i) {
        int col = i % columns_;
        int row = i / columns_;
        
        float cx = gx + col * cell_w;
        float cy = gy + row * row_height_;
        
        // Button bg
        r.draw_rect(cx + 1, cy + 1, cell_w - 2, row_height_ - 2, 4, flex::Paint::solid({0.22f, 0.22f, 0.24f, 1.0f}), flex::Paint::none(), 0);
        
        // Icon
        if (icons_) {
            icons_->render_icon(r, it->second[i].icon, cx + 6, cy + 6, cell_w - 12, {0.6f, 1.0f, 0.6f, 1.0f});
        } else {
            // Icon sketch fallback
            r.draw_rect(cx + 6, cy + 6, cell_w - 12, row_height_ - 12, 1, flex::Paint::none(), flex::Paint::solid({0.6f, 1.0f, 0.6f, 1.0f}), 1.5f);
        }
    }
}

void CADToolbox::render_selection_mode(flex::Renderer& r) {
    float sy = y_ + 24 + tab_height_ + 150;
    r.draw_text("Select objects", x_ + 8, sy, "Arial", 11, false, {0.6f, 0.6f, 0.6f, 1.0f});
    
    sy += 18;
    r.draw_text("Mode:", x_ + 8, sy, "Arial", 10, false, {0.5f, 0.5f, 0.5f, 1.0f});
    
    sy += 14;
    // Toggle button
    r.draw_rect(x_ + 8, sy, 50, 20, 4, flex::Paint::solid({0.18f, 0.35f, 0.18f, 1.0f}), flex::Paint::none(), 0);
    r.draw_text("Toggle", x_ + 14, sy + 4, "Arial", 10, false, {1,1,1,1});
}

bool CADToolbox::handle_click(float mx, float my) {
    if (!contains(mx, my)) return false;
    
    float local_y = my - (y_ + 24 + 4);
    if (local_y >= 0 && local_y < tab_height_) {
        float local_x = mx - (x_ + 4);
        float tw = (width_ - 12) / 3;
        int idx = (int)(local_x / (tw + 2));
        if (idx >= 0 && idx < (int)tabs_.size()) {
            active_tab_ = tabs_[idx];
            return true;
        }
    }
    
    return true;
}

} // namespace meta_editor
