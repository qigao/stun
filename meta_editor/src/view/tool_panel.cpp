#include "meta_editor/view/tool_panel.h"
#include "meta_editor/tool_manager.h"
#include <flex.h>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace meta_editor {

ToolPanel::ToolPanel(ToolManager* tools) : tools_(tools) {
    width_ = 52;
    
    tool_defs_ = {
        {"Select", "V", "M 12.586 12.586 L 19 19 M 3.688 3.037 a 0.497 0.497 0 0 0 -0.651 0.651 l 6.5 15.999 a 0.501 0.501 0 0 0 0.947 -0.062 l 1.569 -6.083 a 2 2 0 0 1 1.448 -1.479 l 6.124 -1.579 a 0.5 0.5 0 0 0 0.063 -0.947 z"}, 
        {"Pen", "P", "M 21.174 6.812 a 1 1 0 0 0 -3.986 -3.987 L 3.842 16.174 a 2 2 0 0 0 -0.5 0.83 l -1.321 4.352 a 0.5 0.5 0 0 0 0.623 0.622 l 4.353 -1.32 a 2 2 0 0 0 0.83 -0.497 z"},
        {"Rectangle", "R", "geometry:rect"},
        {"Circle", "O", "geometry:circle"},
        {"Ellipse", "E", "geometry:ellipse"},
        {"Star", "S", "geometry:star"},
        {"Polygon", "G", "geometry:polygon"},
        {"Triangle", "W", "geometry:triangle"},
    };

    // Create standalone Flex instance for this panel's UI
    ui_instance_ = flex::Instance::create(200, 600); // Fixed capacity
    rebuild_layout();
}

void ToolPanel::rebuild_layout() {
    auto* scene = ui_instance_->scene();
    root_group_ = scene->root()->add<flex::Group>();
    root_group_->set_layout(flex::LayoutMode::Flex);
    root_group_->set_flex_direction(flex::FlexDirection::Column);
    root_group_->set_padding(padding_);
    root_group_->set_gap(gap_);
    root_group_->set_layout_width(width_); // Fixed width
    root_group_->set_layout_height(800);   // Sufficient height for tools
    
    tool_buttons_.clear();

    for (const auto& def : tool_defs_) {
        // Button container
        auto* btn = root_group_->add<flex::Group>();
        btn->set_layout_size(button_size_, button_size_);
        btn->set_flex_shrink(0);
        btn->set_id(def.name);
        
        // Background (initially transparent/default)
        auto* bg = btn->add<flex::Shape>();
        bg->set_rect(button_size_, button_size_, 6.0f); // Rounded corners
        bg->set_fill(flex::Color(0.24f, 0.24f, 0.26f, 1.0f));
        bg->set_id("bg"); // Use ID for finding

        // Icon
        auto* icon = btn->add<flex::Shape>();
        
        if (def.icon_path.rfind("geometry:", 0) == 0) {
            // Primitive shapes for icons (most are centered at 0,0 by default)
            icon->set_position(button_size_/2, button_size_/2); 

            if (def.name == "Rectangle") {
                icon->set_rect(20, 14, 2);
                icon->set_position((button_size_ - 20) / 2, (button_size_ - 14) / 2); // Rect is top-left aligned
            }
            else if (def.name == "Circle") icon->set_circle(9);
            else if (def.name == "Ellipse") icon->set_ellipse(10, 7);
            else if (def.name == "Star") icon->set_star(5, 10, 4);
            else if (def.name == "Polygon") icon->set_polygon(5, 9);
            else if (def.name == "Triangle") icon->set_triangle(18, 16, flex::Direction::Up);
            
            icon->set_stroke(flex::Color(0.75f, 0.75f, 0.75f, 1.0f), 1.5f);
            icon->clear_fill();
        } else {
             // Use path from lucide (assumed 24x24)
             icon->set_path(def.icon_path);
             float icon_scale = 0.8f;
             icon->set_scale(icon_scale);
             
             // Center 24x24 icon with scale
             float scaled_size = 24.0f * icon_scale;
             float offset = (button_size_ - scaled_size) / 2;
             icon->set_position(offset, offset);
             
             icon->set_stroke(flex::Color(0.75f, 0.75f, 0.75f, 1.0f), 1.5f);
             icon->clear_fill();
        }
        icon->set_id("icon");
        
        tool_buttons_[def.name] = btn;
    }
    update_layout();
    
    // Ensure parent panel height matches content
    height_ = content_height(); 
}

float ToolPanel::content_height() const {
    // Flex auto-calculates height, but we can query the root group's layout height
    // after a layout pass. For now, manual estimate is fine for panel sizing.
    if (!root_group_) return 100;
    return root_group_->layout_height();
}

void ToolPanel::update_layout() {
    if (root_group_) {
        root_group_->perform_layout();
    }
}

void ToolPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);
    update_button_states();
    update_layout();

    // Sync Root position with Panel position
    if (root_group_) {
        root_group_->set_position(x_, y_);
    }

    // Render the isolated UI instance
    ui_instance_->render(renderer);
}

void ToolPanel::update_button_states() {
    std::string active_tool = tools_->active_tool() ? tools_->active_tool()->name() : "";
    
    for (auto& [name, group] : tool_buttons_) {
        bool is_active = (name == active_tool);
        
        // Update styling based on state
        auto* bg = dynamic_cast<flex::Shape*>(group->find("bg")); 
        
        if (bg) {
             flex::Color bg_color = is_active 
                ? flex::Color{0.41f, 0.40f, 0.86f, 1.0f} 
                : flex::Color{0.24f, 0.24f, 0.26f, 1.0f};
             bg->set_fill(bg_color);
             
             // Update icon color too
             auto* icon = dynamic_cast<flex::Shape*>(group->find("icon"));
             if (icon) {
                 flex::Color icon_color = is_active
                    ? flex::Color{1.0f, 1.0f, 1.0f, 1.0f}
                    : flex::Color{0.75f, 0.75f, 0.75f, 1.0f};
                 if (icon->has_fill()) icon->set_fill(icon_color);
                 else icon->set_stroke(icon_color, 1.5f);
             }
        }
    }
}

bool ToolPanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    printf("[ToolPanel] Clicked at (%.2f, %.2f) | Panel Pos: (%.2f, %.2f) Width: %.2f Height: %.2f\n", 
           screen_x, screen_y, x_, y_, width_, height_);

    if (root_group_) {
        root_group_->set_position(x_, y_);
        update_layout();
    }

    // Hit test against actual Flex layout positions
    for (size_t i = 0; i < tool_defs_.size(); ++i) {
        auto it = tool_buttons_.find(tool_defs_[i].name);
        if (it == tool_buttons_.end()) continue;

        auto wb = it->second->world_bounds();
        printf("  Tool '%s' world_bounds: (%.2f, %.2f, %.2f, %.2f)\n", 
               tool_defs_[i].name.c_str(), wb.x, wb.y, wb.width, wb.height);
        if (screen_x >= wb.x && screen_x <= wb.x + wb.width &&
            screen_y >= wb.y && screen_y <= wb.y + wb.height) {
            tools_->set_active_tool(tool_defs_[i].name);
            return true;
        }
    }

    return false;
}

} // namespace meta_editor
