#include "flexchart/rect/rect_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <algorithm>

extern "C" void flexchart_rect_force_link(void) {}

namespace flex {
namespace chart {

class RectMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) override;
};

void RectMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field, color_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
        if (enc->channel == "color") color_field = enc->field;
    }
    float cell_w = ctx.available_plot_w / (std::max)(1.0f, (float)ctx.x_labels.size());
    float cell_h = 30.0f;
    for (size_t i = 0; i < records.size(); ++i) {
        std::string x_val = get_string_val(records[i].get(x_field));
        float px = get_x_pos(x_val, ctx);
        float val_y = (float)(get_double_val(records[i].get(y_field)));
        float py = ctx.estimated_plot_h - (val_y * ctx.y_scale);
        double val_color = get_double_val(records[i].get(color_field));
        auto rect_shape = ctx.arena.create<Shape>();
        rect_shape->set_rect(cell_w * 0.95f, cell_h * 0.95f);
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
REGISTER_MARK_RENDERER("rect", flex::chart::RectMarkRenderer)
