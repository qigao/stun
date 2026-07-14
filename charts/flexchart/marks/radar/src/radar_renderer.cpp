#include "flexchart/radar/radar_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <cmath>
#include <algorithm>

extern "C" void flexchart_radar_force_link(void) {}

namespace flex {
namespace chart {

class RadarMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) override;
};

void RadarMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    if (records.empty()) return;

    std::string x_field, y_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
    }

    float cx = ctx.available_plot_w / 2.0f;
    float cy = ctx.estimated_plot_h / 2.0f;
    float r_max = (std::min)(ctx.available_plot_w, ctx.estimated_plot_h) * 0.45f;
    size_t count = records.size();

    for (float step : {0.25f, 0.50f, 0.75f, 1.0f}) {
        auto grid = ctx.arena.create<Shape>();
        std::string grid_path = "";
        for (size_t i = 0; i <= count; ++i) {
            float angle = (float)(i * 2.0 * FLEX_PI / count) - FLEX_PI / 2.0f;
            float px = cx + r_max * step * std::cos(angle);
            float py = cy + r_max * step * std::sin(angle);
            char buf[64];
            stbsp_snprintf(buf, sizeof(buf), (i == 0) ? "M %.2f %.2f" : " L %.2f %.2f", px, py);
            grid_path += buf;
        }
        grid->set_path(grid_path + " Z");
        grid->set_stroke(Color(0.85f, 0.88f, 0.91f), 1.0f);
        grid->clear_fill();
        grid->set_position_absolute(true);
        grid->set_position(0, 0);
        ctx.overlay->add_child(grid);
    }

    auto data_poly = ctx.arena.create<Shape>();
    std::string data_path = "";
    for (size_t i = 0; i < count; ++i) {
        float angle = (float)(i * 2.0 * FLEX_PI / count) - FLEX_PI / 2.0f;
        double val = get_double_val(records[i].get(y_field));
        float dist = (float)((val / 100.0) * r_max);
        float px = cx + dist * std::cos(angle);
        float py = cy + dist * std::sin(angle);

        char buf[64];
        stbsp_snprintf(buf, sizeof(buf), (i == 0) ? "M %.2f %.2f" : " L %.2f %.2f", px, py);
        data_path += buf;

        auto label = ctx.arena.create<Text>();
        label->set_content(get_string_val(records[i].get(x_field)));
        label->set_font_size(12.0f);
        label->set_color(Color(0.24f, 0.28f, 0.33f));
        float lx = cx + (r_max + 20.0f) * std::cos(angle);
        float ly = cy + (r_max + 15.0f) * std::sin(angle);
        label->set_position(lx, ly);
        label->set_anchor(Anchor::Center);
        ctx.overlay->add_child(label);
    }
    data_poly->set_path(data_path + " Z");
    data_poly->set_fill(Color(0.24f, 0.51f, 1.0f, 0.3f));
    data_poly->set_stroke(Color(0.24f, 0.51f, 1.0f), 2.0f);
    data_poly->set_position_absolute(true);
    data_poly->set_position(0, 0);
    ctx.overlay->add_child(data_poly);
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("radar", flex::chart::RadarMarkRenderer)
