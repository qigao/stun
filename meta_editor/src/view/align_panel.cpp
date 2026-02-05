/*
 * Meta Editor - Align Panel Implementation
 */
 
#include "meta_editor/view/align_panel.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include <algorithm>

namespace meta_editor {

AlignPanel::AlignPanel(Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {
    x_ = 16;
    y_ = 16;
    width_ = 4 * button_size_ + 3 * gap_ + 2 * padding_;
}

float AlignPanel::content_height() const {
    return 3 * button_size_ + 2 * gap_ + 2 * padding_ + 24;  // 3 rows + header
}

void AlignPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;
    if (!selection_->has_selection() || selection_->selection().size() < 2) return;

    render_background(renderer);

    // Header
    renderer.draw_text("Align", x_ + 8, y_ + 6, "Arial", 11, true,
                      flex::Color{0.6f, 0.6f, 0.6f, 1.0f});

    float start_y = y_ + 24;

    // Row 1: Align horizontal
    render_button(renderer, x_ + padding_, start_y, AlignType::Left);
    render_button(renderer, x_ + padding_ + button_size_ + gap_, start_y, AlignType::CenterH);
    render_button(renderer, x_ + padding_ + 2 * (button_size_ + gap_), start_y, AlignType::Right);
    render_button(renderer, x_ + padding_ + 3 * (button_size_ + gap_), start_y, AlignType::DistributeH);

    // Row 2: Align vertical
    float row2_y = start_y + button_size_ + gap_;
    render_button(renderer, x_ + padding_, row2_y, AlignType::Top);
    render_button(renderer, x_ + padding_ + button_size_ + gap_, row2_y, AlignType::CenterV);
    render_button(renderer, x_ + padding_ + 2 * (button_size_ + gap_), row2_y, AlignType::Bottom);
    render_button(renderer, x_ + padding_ + 3 * (button_size_ + gap_), row2_y, AlignType::DistributeV);

    // Row 3: Group/Ungroup
    float row3_y = row2_y + button_size_ + gap_;
    render_button(renderer, x_ + padding_, row3_y, AlignType::Group);
    render_button(renderer, x_ + padding_ + button_size_ + gap_, row3_y, AlignType::Ungroup);
}

void AlignPanel::render_button(flex::Renderer& renderer, float x, float y, AlignType type) {
    // Button background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.27f, 1.0f});
    renderer.draw_rect(x, y, button_size_, button_size_, 4, bg, flex::Paint::none(), 0);

    flex::Color icon_color{0.7f, 0.7f, 0.7f, 1.0f};
    float cx = x + button_size_ / 2;
    float cy = y + button_size_ / 2;
    float s = 8;

    // Draw icon based on type
    std::string path;
    switch (type) {
        case AlignType::Left:
            // Vertical line on left + shapes
            path = "M " + std::to_string(cx - s) + " " + std::to_string(cy - s) +
                   " L " + std::to_string(cx - s) + " " + std::to_string(cy + s);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 2);
            renderer.draw_rect(cx - s + 2, cy - 4, 10, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - s + 2, cy + 2, 6, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::CenterH:
            path = "M " + std::to_string(cx) + " " + std::to_string(cy - s) +
                   " L " + std::to_string(cx) + " " + std::to_string(cy + s);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 1);
            renderer.draw_rect(cx - 5, cy - 5, 10, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 3, cy + 2, 6, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::Right:
            path = "M " + std::to_string(cx + s) + " " + std::to_string(cy - s) +
                   " L " + std::to_string(cx + s) + " " + std::to_string(cy + s);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 2);
            renderer.draw_rect(cx + s - 12, cy - 4, 10, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + s - 8, cy + 2, 6, 4, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::Top:
            path = "M " + std::to_string(cx - s) + " " + std::to_string(cy - s) +
                   " L " + std::to_string(cx + s) + " " + std::to_string(cy - s);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 2);
            renderer.draw_rect(cx - 5, cy - s + 2, 4, 8, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy - s + 2, 4, 5, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::CenterV:
            path = "M " + std::to_string(cx - s) + " " + std::to_string(cy) +
                   " L " + std::to_string(cx + s) + " " + std::to_string(cy);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 1);
            renderer.draw_rect(cx - 5, cy - 5, 4, 10, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy - 3, 4, 6, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::Bottom:
            path = "M " + std::to_string(cx - s) + " " + std::to_string(cy + s) +
                   " L " + std::to_string(cx + s) + " " + std::to_string(cy + s);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 2);
            renderer.draw_rect(cx - 5, cy + s - 10, 4, 8, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy + s - 7, 4, 5, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::DistributeH:
            renderer.draw_rect(cx - 7, cy - 4, 3, 8, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 1, cy - 4, 3, 8, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 5, cy - 4, 3, 8, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::DistributeV:
            renderer.draw_rect(cx - 4, cy - 7, 8, 3, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 4, cy - 1, 8, 3, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 4, cy + 5, 8, 3, 1, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::Group:
            // Draw bracket icon [ ] enclosing shapes
            renderer.draw_rect(cx - 6, cy - 5, 2, 10, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 6, cy - 5, 4, 2, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx - 6, cy + 3, 4, 2, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 4, cy - 5, 2, 10, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy - 5, 4, 2, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy + 3, 4, 2, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            // Small shapes inside
            renderer.draw_rect(cx - 3, cy - 2, 3, 3, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 1, cy, 3, 3, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            break;
        case AlignType::Ungroup:
            // Draw separated shapes
            renderer.draw_rect(cx - 7, cy - 4, 5, 5, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            renderer.draw_rect(cx + 2, cy - 1, 5, 5, 0, flex::Paint::solid(icon_color), flex::Paint::none(), 0);
            // Arrows pointing outward
            path = "M " + std::to_string(cx - 1) + " " + std::to_string(cy - 2) +
                   " L " + std::to_string(cx + 1) + " " + std::to_string(cy);
            renderer.stroke_path(path, flex::Paint::solid(icon_color), 1);
            break;
    }
}

bool AlignPanel::handle_click(float screen_x, float screen_y) {
    if (!visible_ || !selection_->has_selection() || selection_->selection().size() < 2) return false;
    if (!contains(screen_x, screen_y)) return false;

    float local_x = screen_x - x_ - padding_;
    float local_y = screen_y - y_ - 24;

    if (local_x < 0 || local_y < 0) return true;

    int col = static_cast<int>(local_x / (button_size_ + gap_));
    int row = static_cast<int>(local_y / (button_size_ + gap_));

    if (col < 0 || col > 3 || row < 0 || row > 2) return true;

    // Row 0-1: Alignment
    if (row < 2) {
        AlignType types[2][4] = {
            {AlignType::Left, AlignType::CenterH, AlignType::Right, AlignType::DistributeH},
            {AlignType::Top, AlignType::CenterV, AlignType::Bottom, AlignType::DistributeV}
        };
        do_align(types[row][col]);
    }
    // Row 2: Group/Ungroup
    else if (row == 2) {
        if (col == 0 && on_group_) {
            on_group_();
        } else if (col == 1 && on_ungroup_) {
            on_ungroup_();
        }
    }

    return true;
}

void AlignPanel::do_align(AlignType type) {
    auto& sel = selection_->selection();
    if (sel.size() < 2) return;

    // Calculate bounds
    float min_x = 1e9f, max_x = -1e9f;
    float min_y = 1e9f, max_y = -1e9f;

    struct ShapeInfo {
        flex::Node* node;
        float x, y, w, h;
    };
    std::vector<ShapeInfo> shapes;

    for (auto* node : sel) {
        auto b = node->bounds();
        float x = node->x();
        float y = node->y();
        shapes.push_back({node, x, y, b.width, b.height});
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x + b.width);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y + b.height);
    }

    float center_x = (min_x + max_x) / 2;
    float center_y = (min_y + max_y) / 2;

    switch (type) {
        case AlignType::Left:
            for (auto& s : shapes) s.node->set_position(min_x, s.y);
            break;
        case AlignType::CenterH:
            for (auto& s : shapes) s.node->set_position(center_x - s.w / 2, s.y);
            break;
        case AlignType::Right:
            for (auto& s : shapes) s.node->set_position(max_x - s.w, s.y);
            break;
        case AlignType::Top:
            for (auto& s : shapes) s.node->set_position(s.x, min_y);
            break;
        case AlignType::CenterV:
            for (auto& s : shapes) s.node->set_position(s.x, center_y - s.h / 2);
            break;
        case AlignType::Bottom:
            for (auto& s : shapes) s.node->set_position(s.x, max_y - s.h);
            break;
        case AlignType::DistributeH:
            if (shapes.size() >= 2) {
                std::sort(shapes.begin(), shapes.end(), [](const auto& a, const auto& b) { return a.x < b.x; });
                float total_width = 0;
                for (auto& s : shapes) total_width += s.w;
                float spacing = (max_x - min_x - total_width) / (shapes.size() - 1);
                float curr_x = min_x;
                for (auto& s : shapes) {
                    s.node->set_position(curr_x, s.y);
                    curr_x += s.w + spacing;
                }
            }
            break;
        case AlignType::DistributeV:
            if (shapes.size() >= 2) {
                std::sort(shapes.begin(), shapes.end(), [](const auto& a, const auto& b) { return a.y < b.y; });
                float total_height = 0;
                for (auto& s : shapes) total_height += s.h;
                float spacing = (max_y - min_y - total_height) / (shapes.size() - 1);
                float curr_y = min_y;
                for (auto& s : shapes) {
                    s.node->set_position(s.x, curr_y);
                    curr_y += s.h + spacing;
                }
            }
            break;
    }
}

} // namespace meta_editor
