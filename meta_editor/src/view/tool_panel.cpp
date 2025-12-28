/*
 * Meta Editor - Vertical Tool Panel Implementation
 */

#include "meta_editor/view/tool_panel.h"
#include "meta_editor/tool_manager.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace meta_editor {

ToolPanel::ToolPanel(ToolManager* tools) : tools_(tools) {
    // Set default position and size
    x_ = 16;
    y_ = 16;
    width_ = 44;

    // Define tools with shortcuts
    tool_defs_ = {
        {"Select", "", "V"},
        {"Pen", "", "P"},
        {"Rectangle", "", "R"},
        {"Circle", "", "O"},
        {"Ellipse", "", "E"},
        {"Star", "", "S"},
        {"Polygon", "", "G"},
    };
}

float ToolPanel::content_height() const {
    int count = static_cast<int>(tool_defs_.size());
    return padding_ * 2 + count * button_size_ + (count - 1) * gap_;
}

void ToolPanel::draw_tool_icon(flex::Renderer& r, const std::string& name,
                                float cx, float cy, float size, const flex::Color& color) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (name == "Select") {
        // Arrow cursor
        float s = size * 0.4f;
        ss << "M " << (cx - s * 0.5f) << " " << (cy - s)
           << " L " << (cx - s * 0.5f) << " " << (cy + s * 0.6f)
           << " L " << (cx - s * 0.1f) << " " << (cy + s * 0.2f)
           << " L " << (cx + s * 0.4f) << " " << (cy + s * 0.7f)
           << " L " << (cx + s * 0.6f) << " " << (cy + s * 0.5f)
           << " L " << (cx + s * 0.1f) << " " << (cy)
           << " L " << (cx + s * 0.5f) << " " << (cy - s * 0.4f)
           << " Z";
        r.fill_path(ss.str(), flex::Paint::solid(color));
    }
    else if (name == "Pen") {
        // Pen shape
        float s = size * 0.35f;
        ss << "M " << (cx - s) << " " << (cy + s)
           << " L " << (cx - s * 0.6f) << " " << (cy + s * 0.4f)
           << " L " << (cx + s * 0.6f) << " " << (cy - s * 0.8f)
           << " L " << (cx + s) << " " << (cy - s * 0.4f)
           << " L " << (cx) << " " << (cy + s * 0.6f)
           << " Z";
        r.fill_path(ss.str(), flex::Paint::solid(color));
    }
    else if (name == "Rectangle") {
        float s = size * 0.35f;
        r.draw_rect(cx - s, cy - s * 0.7f, s * 2, s * 1.4f, 2,
                   flex::Paint::none(), flex::Paint::solid(color), 1.5f);
    }
    else if (name == "Circle") {
        float radius = size * 0.35f;
        r.draw_circle(cx, cy, radius, flex::Paint::none(), flex::Paint::solid(color), 1.5f);
    }
    else if (name == "Ellipse") {
        float rx = size * 0.4f;
        float ry = size * 0.25f;
        r.draw_ellipse(cx, cy, rx, ry, flex::Paint::none(), flex::Paint::solid(color), 1.5f);
    }
    else if (name == "Star") {
        float outer = size * 0.38f;
        float inner = outer * 0.4f;
        int points = 5;
        for (int i = 0; i < points * 2; ++i) {
            float angle = (float)i * 3.14159f / points - 3.14159f / 2;
            float radius = (i % 2 == 0) ? outer : inner;
            float px = cx + std::cos(angle) * radius;
            float py = cy + std::sin(angle) * radius;
            ss << (i == 0 ? "M " : " L ") << px << " " << py;
        }
        ss << " Z";
        r.stroke_path(ss.str(), flex::Paint::solid(color), 1.5f);
    }
    else if (name == "Polygon") {
        float radius = size * 0.35f;
        int sides = 6;
        for (int i = 0; i < sides; ++i) {
            float angle = (float)i * 2 * 3.14159f / sides - 3.14159f / 2;
            float px = cx + std::cos(angle) * radius;
            float py = cy + std::sin(angle) * radius;
            ss << (i == 0 ? "M " : " L ") << px << " " << py;
        }
        ss << " Z";
        r.stroke_path(ss.str(), flex::Paint::solid(color), 1.5f);
    }
}

void ToolPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    int count = static_cast<int>(tool_defs_.size());

    // Draw panel background using base class
    render_background(renderer);

    std::string active_tool = tools_->active_tool() ? tools_->active_tool()->name() : "";

    for (int i = 0; i < count; ++i) {
        float btn_x = x_ + padding_;
        float btn_y = y_ + padding_ + i * (button_size_ + gap_);

        bool is_active = (tool_defs_[i].name == active_tool);

        // Button background
        flex::Color btn_bg = is_active
            ? flex::Color{0.41f, 0.40f, 0.86f, 1.0f}  // Active: purple
            : flex::Color{0.24f, 0.24f, 0.26f, 1.0f}; // Normal: dark gray

        renderer.draw_rect(btn_x, btn_y, button_size_, button_size_, 6,
                          flex::Paint::solid(btn_bg), flex::Paint::none(), 0);

        // Icon color
        flex::Color icon_color = is_active
            ? flex::Color{1.0f, 1.0f, 1.0f, 1.0f}
            : flex::Color{0.75f, 0.75f, 0.75f, 1.0f};

        // Draw geometric icon centered in button
        float icon_cx = btn_x + button_size_ / 2;
        float icon_cy = btn_y + button_size_ / 2;

        draw_tool_icon(renderer, tool_defs_[i].name, icon_cx, icon_cy, button_size_, icon_color);
    }
}

bool ToolPanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    int count = static_cast<int>(tool_defs_.size());

    // Find which button was clicked
    float local_y = screen_y - y_ - padding_;
    int idx = static_cast<int>(local_y / (button_size_ + gap_));

    if (idx >= 0 && idx < count) {
        // Check if actually within button bounds (not in gap)
        float btn_top = idx * (button_size_ + gap_);
        float btn_bottom = btn_top + button_size_;
        if (local_y >= btn_top && local_y < btn_bottom) {
            tools_->set_active_tool(tool_defs_[idx].name);
            return true;
        }
    }

    return false;
}

} // namespace meta_editor
