/*
 * Meta Editor - Properties Panel Implementation
 */

#include "meta_editor/view/properties_panel.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

// Color palette - 12 colors (2 rows of 6)
const std::vector<flex::Color>& PropertiesPanel::color_palette() {
    static std::vector<flex::Color> palette = {
        {0.0f, 0.0f, 0.0f, 1.0f},       // Black
        {0.4f, 0.4f, 0.4f, 1.0f},       // Dark gray
        {0.7f, 0.7f, 0.7f, 1.0f},       // Light gray
        {1.0f, 1.0f, 1.0f, 1.0f},       // White
        {0.9f, 0.3f, 0.3f, 1.0f},       // Red
        {0.3f, 0.7f, 0.3f, 1.0f},       // Green
        {0.3f, 0.5f, 0.9f, 1.0f},       // Blue
        {0.9f, 0.7f, 0.2f, 1.0f},       // Yellow
        {0.9f, 0.5f, 0.2f, 1.0f},       // Orange
        {0.7f, 0.3f, 0.8f, 1.0f},       // Purple
        {0.3f, 0.8f, 0.8f, 1.0f},       // Cyan
        {0.9f, 0.5f, 0.7f, 1.0f},       // Pink
    };
    return palette;
}

PropertiesPanel::PropertiesPanel(Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {
    x_ = 16;
    y_ = 16;
    width_ = 180;
}

float PropertiesPanel::content_height() const {
    // Fill header + 2 rows of swatches + Stroke header + 2 rows + Width header + 1 row
    float fill_section = header_height_ + 2 * (swatch_size_ + swatch_gap_);
    float stroke_section = header_height_ + 2 * (swatch_size_ + swatch_gap_);
    float width_section = header_height_ + swatch_size_ + swatch_gap_;
    return fill_section + stroke_section + width_section + 8;  // 8px padding
}

void PropertiesPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    // Only show if there's a selection
    if (!selection_->has_selection()) return;

    render_background(renderer);

    float y = y_ + 4;

    // Fill section
    render_section_header(renderer, "Fill", y);
    y += header_height_;
    render_color_swatches(renderer, y, true);
    y += 2 * (swatch_size_ + swatch_gap_);

    // Stroke section
    render_section_header(renderer, "Stroke", y);
    y += header_height_;
    render_color_swatches(renderer, y, false);
    y += 2 * (swatch_size_ + swatch_gap_);

    // Stroke width section
    render_section_header(renderer, "Width", y);
    y += header_height_;
    render_stroke_width(renderer, y);
}

void PropertiesPanel::render_section_header(flex::Renderer& renderer, const char* title, float y) {
    renderer.draw_text(title, x_ + 8, y + 6, "Arial", 12, true,
                      flex::Color{0.7f, 0.7f, 0.7f, 1.0f});
}

void PropertiesPanel::render_color_swatches(flex::Renderer& renderer, float y, bool is_fill) {
    const auto& palette = color_palette();
    float start_x = x_ + 8;

    // Get current color from first selected shape
    flex::Color current_color = {0.5f, 0.5f, 0.5f, 1.0f};
    if (!selection_->selection().empty()) {
        auto* node = selection_->selection()[0];
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            if (is_fill && shape->has_fill()) {
                current_color = shape->fill().color;
            } else if (!is_fill && shape->has_stroke()) {
                current_color = shape->stroke().color;
            }
        }
    }

    for (size_t i = 0; i < palette.size(); ++i) {
        int col = i % swatches_per_row_;
        int row = i / swatches_per_row_;
        float sx = start_x + col * (swatch_size_ + swatch_gap_);
        float sy = y + row * (swatch_size_ + swatch_gap_);

        // Check if this is the current color
        bool is_current = (std::abs(palette[i].r - current_color.r) < 0.1f &&
                          std::abs(palette[i].g - current_color.g) < 0.1f &&
                          std::abs(palette[i].b - current_color.b) < 0.1f);

        // Swatch background
        flex::Paint fill = flex::Paint::solid(palette[i]);
        flex::Paint stroke = is_current
            ? flex::Paint::solid(flex::Color{1.0f, 1.0f, 1.0f, 1.0f})
            : flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.3f, 1.0f});
        renderer.draw_rect(sx, sy, swatch_size_, swatch_size_, 4, fill, stroke, is_current ? 2.0f : 1.0f);

        // "No color" indicator for first swatch position could be added here
    }

    // Add "none" swatch (transparent)
    float none_x = start_x + (palette.size() % swatches_per_row_) * (swatch_size_ + swatch_gap_);
    float none_y = y + (palette.size() / swatches_per_row_) * (swatch_size_ + swatch_gap_);
    if (palette.size() % swatches_per_row_ == 0) {
        none_x = start_x;
        none_y = y + 2 * (swatch_size_ + swatch_gap_);  // Next row
    }

    // Draw "none" swatch with diagonal line
    bool is_none = (current_color.a < 0.1f);
    flex::Paint none_stroke = is_none
        ? flex::Paint::solid(flex::Color{1.0f, 1.0f, 1.0f, 1.0f})
        : flex::Paint::solid(flex::Color{0.4f, 0.4f, 0.4f, 1.0f});
    renderer.draw_rect(none_x, none_y, swatch_size_, swatch_size_, 4,
                      flex::Paint::solid(flex::Color{0.2f, 0.2f, 0.2f, 1.0f}),
                      none_stroke, is_none ? 2.0f : 1.0f);
    // Diagonal line for "none"
    std::string diag = "M " + std::to_string(none_x + 4) + " " + std::to_string(none_y + swatch_size_ - 4) +
                       " L " + std::to_string(none_x + swatch_size_ - 4) + " " + std::to_string(none_y + 4);
    renderer.stroke_path(diag, flex::Paint::solid(flex::Color{0.6f, 0.3f, 0.3f, 1.0f}), 2.0f);
}

void PropertiesPanel::render_stroke_width(flex::Renderer& renderer, float y) {
    float start_x = x_ + 8;

    // Get current stroke width
    float current_width = 2.0f;
    if (!selection_->selection().empty()) {
        auto* node = selection_->selection()[0];
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            if (shape->has_stroke()) {
                current_width = shape->stroke().width;
            }
        }
    }

    for (int i = 0; i < NUM_STROKE_WIDTHS; ++i) {
        float sx = start_x + i * (swatch_size_ + swatch_gap_);
        float sw = STROKE_WIDTHS[i];

        bool is_current = (std::abs(sw - current_width) < 0.5f);

        // Button background
        flex::Paint bg = is_current
            ? flex::Paint::solid(flex::Color{0.4f, 0.4f, 0.8f, 1.0f})
            : flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.27f, 1.0f});
        renderer.draw_rect(sx, y, swatch_size_, swatch_size_, 4, bg, flex::Paint::none(), 0);

        // Draw line with that width
        float line_y = y + swatch_size_ / 2;
        std::string line_path = "M " + std::to_string(sx + 4) + " " + std::to_string(line_y) +
                               " L " + std::to_string(sx + swatch_size_ - 4) + " " + std::to_string(line_y);
        renderer.stroke_path(line_path, flex::Paint::solid(flex::Color{0.9f, 0.9f, 0.9f, 1.0f}), sw);
    }
}

bool PropertiesPanel::handle_click(float screen_x, float screen_y) {
    if (!visible_ || !selection_->has_selection()) return false;
    if (!contains(screen_x, screen_y)) return false;

    float local_x = screen_x - x_;
    float local_y = screen_y - y_ - 4;

    float y_offset = 0;

    // Fill section
    y_offset += header_height_;
    float fill_section_height = 2 * (swatch_size_ + swatch_gap_);
    if (local_y >= y_offset && local_y < y_offset + fill_section_height) {
        int idx = hit_test_color(local_x, local_y - y_offset, 0);
        if (idx >= 0) {
            apply_fill_color(idx);
            return true;
        }
    }
    y_offset += fill_section_height;

    // Stroke section
    y_offset += header_height_;
    float stroke_section_height = 2 * (swatch_size_ + swatch_gap_);
    if (local_y >= y_offset && local_y < y_offset + stroke_section_height) {
        int idx = hit_test_color(local_x, local_y - y_offset, 0);
        if (idx >= 0) {
            apply_stroke_color(idx);
            return true;
        }
    }
    y_offset += stroke_section_height;

    // Width section
    y_offset += header_height_;
    if (local_y >= y_offset) {
        int idx = hit_test_stroke_width(local_x, local_y - y_offset, 0);
        if (idx >= 0 && idx < NUM_STROKE_WIDTHS) {
            apply_stroke_width(STROKE_WIDTHS[idx]);
            return true;
        }
    }

    return true;  // Consumed click even if nothing hit
}

int PropertiesPanel::hit_test_color(float local_x, float local_y, float) const {
    float start_x = 8;
    if (local_x < start_x) return -1;

    int col = static_cast<int>((local_x - start_x) / (swatch_size_ + swatch_gap_));
    int row = static_cast<int>(local_y / (swatch_size_ + swatch_gap_));

    if (col < 0 || col >= swatches_per_row_) return -1;
    if (row < 0 || row >= 2) return -1;

    int idx = row * swatches_per_row_ + col;
    const auto& palette = color_palette();

    // Check if it's the "none" swatch (index 12)
    if (idx >= static_cast<int>(palette.size())) {
        return static_cast<int>(palette.size());  // "none" index
    }

    return idx;
}

int PropertiesPanel::hit_test_stroke_width(float local_x, float local_y, float) const {
    float start_x = 8;
    if (local_x < start_x || local_y < 0 || local_y > swatch_size_) return -1;

    int idx = static_cast<int>((local_x - start_x) / (swatch_size_ + swatch_gap_));
    if (idx < 0 || idx >= NUM_STROKE_WIDTHS) return -1;

    return idx;
}

void PropertiesPanel::apply_fill_color(int color_idx) {
    const auto& palette = color_palette();
    flex::Color color;

    if (color_idx >= static_cast<int>(palette.size())) {
        // "None" - transparent
        color = {0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        color = palette[color_idx];
    }

    for (auto* node : selection_->selection()) {
        if (node->type() == flex::NodeType::Shape) {
            static_cast<flex::Shape*>(node)->set_fill(color);
        }
    }
}

void PropertiesPanel::apply_stroke_color(int color_idx) {
    const auto& palette = color_palette();
    flex::Color color;

    if (color_idx >= static_cast<int>(palette.size())) {
        color = {0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        color = palette[color_idx];
    }

    for (auto* node : selection_->selection()) {
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            float width = shape->has_stroke() ? shape->stroke().width : 2.0f;
            shape->set_stroke(color, width);
        }
    }
}

void PropertiesPanel::apply_stroke_width(float width) {
    for (auto* node : selection_->selection()) {
        if (node->type() == flex::NodeType::Shape) {
            auto* shape = static_cast<flex::Shape*>(node);
            flex::Color color = shape->has_stroke() ? shape->stroke().color : flex::Color{0, 0, 0, 1};
            shape->set_stroke(color, width);
        }
    }
}

constexpr float PropertiesPanel::STROKE_WIDTHS[];

} // namespace meta_editor
