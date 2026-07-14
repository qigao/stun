#include "flexchart/errorband/errorband_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <cstdio>

extern "C" void flexchart_errorband_force_link(void) {}

namespace flex {
namespace chart {

class ErrorbandMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override {
        if (!mark || records.empty() || !ctx.overlay) return;

        std::string x_field, y_field, error_field;
        for (const auto& enc : mark->encodings) {
            if (enc->channel == "x") x_field = enc->field;
            else if (enc->channel == "y") y_field = enc->field;
            else if (enc->channel == "y-error") error_field = enc->field;
        }
        if (x_field.empty() || y_field.empty() || error_field.empty()) return;

        std::string path;
        char command[96];
        for (size_t i = 0; i < records.size(); ++i) {
            const float x = get_x_pos(get_string_val(records[i].get(x_field)), ctx);
            const float y = static_cast<float>(get_double_val(records[i].get(y_field)));
            const float error = static_cast<float>(get_double_val(records[i].get(error_field)));
            const float upper = ctx.estimated_plot_h - (y + error) * ctx.y_scale;
            std::snprintf(command, sizeof(command), i == 0 ? "M %.4f %.4f" : " L %.4f %.4f", x, upper);
            path += command;
        }
        for (size_t i = records.size(); i-- > 0;) {
            const float x = get_x_pos(get_string_val(records[i].get(x_field)), ctx);
            const float y = static_cast<float>(get_double_val(records[i].get(y_field)));
            const float error = static_cast<float>(get_double_val(records[i].get(error_field)));
            const float lower = ctx.estimated_plot_h - (y - error) * ctx.y_scale;
            std::snprintf(command, sizeof(command), " L %.4f %.4f", x, lower);
            path += command;
        }
        path += " Z";

        auto band = ctx.arena.create<Shape>();
        if (!band) return;
        band->set_id("errorband-mark-shape");
        band->set_path(path);
        band->set_fill(Color(0.24f, 0.51f, 1.0f, 0.2f));
        band->set_stroke(Color(0.24f, 0.51f, 1.0f, 0.7f), 1.0f);
        band->set_position_absolute(true);
        band->set_anchor(Anchor::TopLeft);
        band->set_position(0, 0);
        ctx.overlay->add_child(band);
    }
};

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("errorband", flex::chart::ErrorbandMarkRenderer)
