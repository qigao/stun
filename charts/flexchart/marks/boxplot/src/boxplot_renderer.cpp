#include "flexchart/boxplot/boxplot_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <cmath>

extern "C" void flexchart_boxplot_force_link(void) {}

namespace flex {
namespace chart {

class BoxplotMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) override;
};

void BoxplotMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, min_f, q1_f, med_f, q3_f, max_f;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") med_f = enc->field;
        if (enc->channel == "min") min_f = enc->field;
        if (enc->channel == "q1") q1_f = enc->field;
        if (enc->channel == "median") med_f = enc->field;
        if (enc->channel == "q3") q3_f = enc->field;
        if (enc->channel == "max") max_f = enc->field;
    }

    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float px = get_x_pos(x_val, ctx);

        float y_min = ctx.estimated_plot_h - (float)(get_double_val(rec.get(min_f)) * ctx.y_scale);
        float y_q1 = ctx.estimated_plot_h - (float)(get_double_val(rec.get(q1_f)) * ctx.y_scale);
        float y_med = ctx.estimated_plot_h - (float)(get_double_val(rec.get(med_f)) * ctx.y_scale);
        float y_q3 = ctx.estimated_plot_h - (float)(get_double_val(rec.get(q3_f)) * ctx.y_scale);
        float y_max = ctx.estimated_plot_h - (float)(get_double_val(rec.get(max_f)) * ctx.y_scale);

        float box_w = 30.0f;

        auto whisker = ctx.arena.create<Shape>();
        char path[128];
        stbsp_snprintf(path, sizeof(path), "M %.4f %.4f L %.4f %.4f", px, y_min, px, y_max);
        whisker->set_path(path);
        whisker->set_stroke(Color(0.44f, 0.5f, 0.56f), 1.5f);
        whisker->set_position_absolute(true);
        whisker->set_position(0, 0);
        ctx.overlay->add_child(whisker);

        auto box = ctx.arena.create<Shape>();
        box->set_rect(box_w, std::abs(y_q1 - y_q3));
        box->set_fill(Color(0.24f, 0.51f, 1.0f, 0.2f));
        box->set_stroke(Color(0.24f, 0.51f, 1.0f), 1.5f);
        box->set_position_absolute(true);
        box->set_anchor(Anchor::Center);
        box->set_position(px, (y_q1 + y_q3) / 2.0f);
        ctx.overlay->add_child(box);

        auto med_line = ctx.arena.create<Shape>();
        stbsp_snprintf(path, sizeof(path), "M %.4f %.4f L %.4f %.4f", px - box_w/2, y_med, px + box_w/2, y_med);
        med_line->set_path(path);
        med_line->set_stroke(Color(0.24f, 0.51f, 1.0f), 2.5f);
        med_line->set_position_absolute(true);
        med_line->set_position(0, 0);
        ctx.overlay->add_child(med_line);
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("boxplot", flex::chart::BoxplotMarkRenderer)
