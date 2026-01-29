/*
 * Meta Editor - Line Style Panel Implementation
 */

#include "meta_editor/view/line_style_panel.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/tools/line_tool.h"
#include <stb_sprintf.h>

namespace meta_editor {

static const flex::Color PALETTE[6] = {
    {0.0f, 0.0f, 0.0f, 1.0f},       // Black
    {0.9f, 0.2f, 0.2f, 1.0f},       // Red
    {0.2f, 0.6f, 0.9f, 1.0f},       // Blue
    {0.2f, 0.8f, 0.4f, 1.0f},       // Green
    {0.9f, 0.7f, 0.1f, 1.0f},       // Yellow
    {0.6f, 0.3f, 0.8f, 1.0f},       // Purple
};

static const float WIDTHS[4] = {1.0f, 2.0f, 4.0f, 8.0f};

LineStylePanel::LineStylePanel(SelectionManager* selection, LineTool* line_tool)
    : selection_(selection), line_tool_(line_tool) {
    x_ = 16;
    y_ = 300;
    width_ = 180;
}

float LineStylePanel::content_height() const {
    return PADDING * 2 + ROW_HEIGHT * 4 + 8;
}

void LineStylePanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    float y = y_ + PADDING;
    flex::Paint text_paint = flex::Paint::solid(flex::Color(0.85f, 0.85f, 0.85f, 1.0f));

    renderer.draw_text("Stroke Color", x_ + PADDING, y, "Arial", 11.0f, false,
                       flex::Color(0.7f, 0.7f, 0.7f, 1.0f));
    y += 16;

    float swatch_size = 20.0f;
    float swatch_gap = 4.0f;
    for (int i = 0; i < COLOR_COUNT; ++i) {
        float sx = x_ + PADDING + i * (swatch_size + swatch_gap);
        flex::Paint fill = flex::Paint::solid(PALETTE[i]);
        flex::Paint border = (i == color_index_)
            ? flex::Paint::solid(flex::Color(1.0f, 1.0f, 1.0f, 1.0f))
            : flex::Paint::solid(flex::Color(0.4f, 0.4f, 0.4f, 1.0f));
        renderer.draw_rect(sx, y, swatch_size, swatch_size, 4, fill, border, 2.0f);
    }
    y += swatch_size + 12;

    renderer.draw_text("Stroke Width", x_ + PADDING, y, "Arial", 11.0f, false,
                       flex::Color(0.7f, 0.7f, 0.7f, 1.0f));
    y += 16;

    for (int i = 0; i < 4; ++i) {
        float bx = x_ + PADDING + i * 38;
        bool selected = (std::abs(stroke_width_ - WIDTHS[i]) < 0.1f);
        flex::Color bg = selected ? flex::Color(0.4f, 0.4f, 0.85f, 1.0f)
                                  : flex::Color(0.25f, 0.25f, 0.27f, 1.0f);
        renderer.draw_rect(bx, y, 34, 22, 4, flex::Paint::solid(bg), flex::Paint::none(), 0);

        char label[8];
        stbsp_snprintf(label, sizeof(label), "%.0f", WIDTHS[i]);
        renderer.draw_text(label, bx + 12, y + 4, "Arial", 11.0f, false,
                           flex::Color(0.9f, 0.9f, 0.9f, 1.0f));
    }
}

bool LineStylePanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    float local_y = screen_y - y_ - PADDING;

    if (local_y >= 16 && local_y < 16 + 20) {
        float local_x = screen_x - x_ - PADDING;
        int idx = (int)(local_x / 24);
        if (idx >= 0 && idx < COLOR_COUNT) {
            color_index_ = idx;
            line_tool_->set_stroke_color(PALETTE[color_index_]);
            apply_stroke_to_selection();
            return true;
        }
    }

    if (local_y >= 16 + 20 + 12 + 16 && local_y < 16 + 20 + 12 + 16 + 22) {
        float local_x = screen_x - x_ - PADDING;
        int idx = (int)(local_x / 38);
        if (idx >= 0 && idx < 4) {
            stroke_width_ = WIDTHS[idx];
            line_tool_->set_stroke_width(stroke_width_);
            apply_stroke_to_selection();
            return true;
        }
    }

    return false;
}

void LineStylePanel::apply_stroke_to_selection() {
    if (!selection_->has_selection()) return;

    flex::Paint stroke = flex::Paint::solid(PALETTE[color_index_]);
    selection_->set_stroke(stroke, stroke_width_);

    if (on_change_) on_change_();
}

} // namespace meta_editor
