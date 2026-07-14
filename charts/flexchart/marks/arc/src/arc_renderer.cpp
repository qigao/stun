#include "flexchart/arc/arc_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" void flexchart_arc_force_link(void) {}

namespace flex {
namespace chart {

class ArcMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void ArcMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records,
                             MarkRenderContext& ctx) {
    std::string theta_field, color_field;
    float inner_radius = 0.0f;
    float outer_radius = 120.0f;
    float pad_angle = 0.02f;

    for (const auto& enc : mark->encodings) {
        if (enc->channel == "theta") theta_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
        else if (enc->channel == "innerRadius") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) inner_radius = (float)get_double_val(it->second);
        }
        else if (enc->channel == "outerRadius") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) outer_radius = (float)get_double_val(it->second);
        }
    }

    auto it = mark->styles.find("innerRadius");
    if (it != mark->styles.end()) inner_radius = (float)get_double_val(it->second);
    it = mark->styles.find("outerRadius");
    if (it != mark->styles.end()) outer_radius = (float)get_double_val(it->second);

    double total = 0;
    for (const auto& rec : records) {
        total += get_double_val(rec.get(theta_field));
    }
    if (total <= 0) return;

    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };

    float cx = ctx.available_plot_w / 2.0f;
    float cy = ctx.estimated_plot_h / 2.0f;

    double start_angle = -M_PI / 2;
    int idx = 0;

    for (const auto& rec : records) {
        double value = get_double_val(rec.get(theta_field));
        double sweep = (value / total) * 2 * M_PI - pad_angle;
        if (sweep <= 0) { idx++; continue; }

        double end_angle = start_angle + sweep;

        float x1_outer = cx + outer_radius * (float)cos(start_angle);
        float y1_outer = cy + outer_radius * (float)sin(start_angle);
        float x2_outer = cx + outer_radius * (float)cos(end_angle);
        float y2_outer = cy + outer_radius * (float)sin(end_angle);

        int large_arc = sweep > M_PI ? 1 : 0;

        char path_buf[512];
        if (inner_radius > 0) {
            float x1_inner = cx + inner_radius * (float)cos(end_angle);
            float y1_inner = cy + inner_radius * (float)sin(end_angle);
            float x2_inner = cx + inner_radius * (float)cos(start_angle);
            float y2_inner = cy + inner_radius * (float)sin(start_angle);

            stbsp_snprintf(path_buf, sizeof(path_buf),
                "M %.2f %.2f A %.2f %.2f 0 %d 1 %.2f %.2f L %.2f %.2f A %.2f %.2f 0 %d 0 %.2f %.2f Z",
                x1_outer, y1_outer,
                outer_radius, outer_radius, large_arc, x2_outer, y2_outer,
                x1_inner, y1_inner,
                inner_radius, inner_radius, large_arc, x2_inner, y2_inner);
        } else {
            stbsp_snprintf(path_buf, sizeof(path_buf),
                "M %.2f %.2f L %.2f %.2f A %.2f %.2f 0 %d 1 %.2f %.2f Z",
                cx, cy,
                x1_outer, y1_outer,
                outer_radius, outer_radius, large_arc, x2_outer, y2_outer);
        }

        Color c = palette[idx % palette.size()];

        auto arc = ctx.arena.create<Shape>();
        arc->set_path(path_buf);
        arc->set_fill(c);
        arc->set_stroke(Color(1, 1, 1), 2.0f);
        arc->set_position_absolute(true);
        arc->set_position(0, 0);

        ctx.overlay->add_child(arc);

        if (ctx.instance) {
            std::string anim_id = "arc_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.3f + idx * 0.1f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), arc);
            arc->set_opacity(0.0f);
        }

        start_angle = end_angle + pad_angle;
        idx++;
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("arc", flex::chart::ArcMarkRenderer)
