/*
 * Meta Editor - Properties Panel
 *
 * Edit fill/stroke/width for selected shapes.
 * Shows color swatches and stroke width control.
 */

#pragma once

#include "panel.h"
#include <vector>

namespace meta_editor {

class Canvas;
class SelectionManager;

class PropertiesPanel : public Panel {
public:
    PropertiesPanel(Canvas* canvas, SelectionManager* selection);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y) override;

protected:
    float content_height() const override;

private:
    void render_section_header(flex::Renderer& renderer, const char* title, float y);
    void render_color_swatches(flex::Renderer& renderer, float y, bool is_fill);
    void render_stroke_width(flex::Renderer& renderer, float y);

    int hit_test_color(float local_x, float local_y, float section_y) const;
    int hit_test_stroke_width(float local_x, float local_y, float section_y) const;

    void apply_fill_color(int color_idx);
    void apply_stroke_color(int color_idx);
    void apply_stroke_width(float width);

    Canvas* canvas_;
    SelectionManager* selection_;

    float header_height_ = 28;
    float swatch_size_ = 24;
    float swatch_gap_ = 4;
    int swatches_per_row_ = 6;

    // Predefined color palette
    static const std::vector<flex::Color>& color_palette();

    // Stroke width presets
    static constexpr float STROKE_WIDTHS[] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
    static constexpr int NUM_STROKE_WIDTHS = 6;
};

} // namespace meta_editor
