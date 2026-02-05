#include "chart_component_internal.h"
#include <cmath>

namespace flex {
namespace chart {

void AreaMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
    }

    auto area_shape = ctx.arena.create<Shape>();
    area_shape->set_id("area-mark-shape");
    area_shape->set_position_absolute(true); 
    area_shape->set_anchor(Anchor::TopLeft);
    area_shape->set_position(0, 0); 
    
    std::string path_data = "";
    if (records.empty()) return;

    // Start at the baseline below the first point
    float x0 = get_x_pos(get_string_val(records[0].get(x_field)), ctx);
    float y_baseline = ctx.estimated_plot_h; 
    
    char buf[128];
    stbsp_snprintf(buf, sizeof(buf), "M %.4f %.4f", x0, y_baseline);
    path_data += buf;

    for (size_t i = 0; i < records.size(); ++i) {
        std::string x_val = get_string_val(records[i].get(x_field));
        float px = get_x_pos(x_val, ctx);
        float val = (float)(get_double_val(records[i].get(y_field)));
        float py = ctx.estimated_plot_h - (val * ctx.y_scale);
        
        stbsp_snprintf(buf, sizeof(buf), " L %.4f %.4f", px, py);
        path_data += buf;
    }

    // Close back to baseline at the last point
    float x_last = get_x_pos(get_string_val(records.back().get(x_field)), ctx);
    stbsp_snprintf(buf, sizeof(buf), " L %.4f %.4f Z", x_last, y_baseline);
    path_data += buf;

    if (!path_data.empty()) {
        area_shape->set_path(path_data);
        
        // Modern Semi-transparent Gradient
        LinearGradient grad(0, 0, 0, 1);
        grad.add_stop(0.0f, Color(0.24f, 0.51f, 1.0f, 0.4f)); 
        grad.add_stop(1.0f, Color(0.24f, 0.51f, 1.0f, 0.05f)); 
        area_shape->set_fill(grad);
        
        area_shape->set_stroke(Color(0.24f, 0.51f, 1.0f), 1.5f);
        ctx.overlay->add_child(area_shape);
    }
}

} // namespace chart
} // namespace flex
