/*
 * Meta Editor - Layers Panel Implementation
 */

#include "meta_editor/view/layers_panel.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"

namespace meta_editor {

LayersPanel::LayersPanel(Canvas* canvas, SelectionManager* selection)
    : canvas_(canvas), selection_(selection) {
    x_ = 16;
    y_ = 16;
    width_ = 180;
}

float LayersPanel::content_height() const {
    auto layers = canvas_->get_all_layers();
    return header_height_ + layers.size() * row_height_ + 8;
}

void LayersPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    float y = y_ + 4;

    // Header
    renderer.draw_text("Layers", x_ + 8, y + 6, "Arial", 12, true,
                      flex::Color{0.7f, 0.7f, 0.7f, 1.0f});
    y += header_height_;

    // Layer rows
    auto layers = canvas_->get_all_layers();
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        render_layer_row(renderer, layers[i], i, y);
        y += row_height_;
    }
}

void LayersPanel::render_layer_row(flex::Renderer& renderer, flex::Group* layer, int index, float y) {
    bool is_selected = (index == selected_layer_);
    bool is_visible = layer->visible();

    // Row background
    flex::Color row_bg = is_selected
        ? flex::Color{0.3f, 0.3f, 0.5f, 1.0f}
        : flex::Color{0.2f, 0.2f, 0.22f, 1.0f};
    renderer.draw_rect(x_ + 4, y, width_ - 8, row_height_ - 2, 4,
                      flex::Paint::solid(row_bg), flex::Paint::none(), 0);

    // Visibility icon (eye)
    float icon_x = x_ + 12;
    float icon_y = y + (row_height_ - icon_size_) / 2;

    flex::Color eye_color = is_visible
        ? flex::Color{0.8f, 0.8f, 0.8f, 1.0f}
        : flex::Color{0.4f, 0.4f, 0.4f, 1.0f};

    // Draw eye icon
    float cx = icon_x + icon_size_ / 2;
    float cy = icon_y + icon_size_ / 2;
    renderer.draw_ellipse(cx, cy, 6, 4, flex::Paint::none(), flex::Paint::solid(eye_color), 1.5f);
    renderer.draw_circle(cx, cy, 2, flex::Paint::solid(eye_color), flex::Paint::none(), 0);

    // Layer name (use id or generate)
    std::string name = layer->id().empty() ? "Layer " + std::to_string(index + 1) : layer->id();
    renderer.draw_text(name, x_ + 36, y + row_height_ / 2 - 6, "Arial", 12, false,
                      flex::Color{0.85f, 0.85f, 0.85f, 1.0f});

    // Shape count
    int count = static_cast<int>(layer->children().size());
    std::string count_str = std::to_string(count);
    renderer.draw_text(count_str, x_ + width_ - 24, y + row_height_ / 2 - 6, "Arial", 11, false,
                      flex::Color{0.5f, 0.5f, 0.5f, 1.0f});
}

bool LayersPanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    float local_x = screen_x - x_;
    float local_y = screen_y - y_ - 4 - header_height_;

    if (local_y < 0) return true;  // Clicked on header

    int layer_idx = hit_test_layer(local_y);
    if (layer_idx < 0) return true;

    auto layers = canvas_->get_all_layers();
    // Reverse index since we render bottom-to-top
    int actual_idx = static_cast<int>(layers.size()) - 1 - layer_idx;
    if (actual_idx < 0 || actual_idx >= static_cast<int>(layers.size())) return true;

    // Check if clicked on visibility icon
    if (hit_test_visibility(local_x)) {
        flex::Group* layer = layers[actual_idx];
        layer->set_visible(!layer->visible());
    } else {
        // Select this layer
        selected_layer_ = actual_idx;
    }

    return true;
}

int LayersPanel::hit_test_layer(float local_y) const {
    if (local_y < 0) return -1;
    return static_cast<int>(local_y / row_height_);
}

bool LayersPanel::hit_test_visibility(float local_x) const {
    return local_x >= 8 && local_x <= 8 + icon_size_ + 8;
}

} // namespace meta_editor
