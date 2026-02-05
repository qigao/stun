#include "chart_component_internal.h"
#include <algorithm>

namespace flex {
namespace chart {

void RectMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field, color_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
        if (enc->channel == "color") color_field = enc->field;
    }

    // For heatmap, we usually have many points.
    // We'll calculate a cell size.
    float cell_w = ctx.available_plot_w / (std::max)(1.0f, (float)ctx.x_labels.size());
    float cell_h = 30.0f; // Simplified fixed height for now

    for (size_t i = 0; i < records.size(); ++i) {
        std::string x_val = get_string_val(records[i].get(x_field));
        float px = get_x_pos(x_val, ctx);
        
        // For Heatmap Y, we might need categorical Y too. 
        // Currently our ctx only supports quantitative Y scale.
        // We'll fallback to quantitative for now.
        float val_y = (float)(get_double_val(records[i].get(y_field)));
        float py = ctx.estimated_plot_h - (val_y * ctx.y_scale);
        
        double val_color = get_double_val(records[i].get(color_field));
        
        auto rect_shape = ctx.arena.create<Shape>();
        rect_shape->set_rect(cell_w * 0.95f, cell_h * 0.95f);
        // Heatmap ramp: Blue for high values, light blue for low
        float alpha = (float)(val_color / 100.0);
        rect_shape->set_fill(Color(0.24f * alpha + 0.3f, 0.51f * alpha + 0.3f, 1.0f, alpha + 0.1f));
        rect_shape->set_position_absolute(true);
        rect_shape->set_anchor(Anchor::Center);
        rect_shape->set_position(px, py);
        ctx.overlay->add_child(rect_shape);
    }
}

} // namespace chart
} // namespace flex
