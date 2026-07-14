#include "flexchart/trail/trail_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

extern "C" void flexchart_trail_force_link(void) {}

namespace flex {
namespace chart {

class TrailMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override {
        if (!mark || records.size() < 2 || !ctx.overlay) return;

        std::string x_field, y_field, size_field;
        for (const auto& enc : mark->encodings) {
            if (enc->channel == "x") x_field = enc->field;
            else if (enc->channel == "y") y_field = enc->field;
            else if (enc->channel == "size") size_field = enc->field;
        }
        if (x_field.empty() || y_field.empty()) return;

        auto point = [&](size_t index) {
            const auto& rec = records[index];
            return std::pair<float, float>{
                get_x_pos(get_string_val(rec.get(x_field)), ctx),
                ctx.estimated_plot_h -
                    static_cast<float>(get_double_val(rec.get(y_field))) * ctx.y_scale};
        };
        auto width = [&](size_t index) {
            if (size_field.empty()) return 4.0f;
            return std::clamp(
                static_cast<float>(get_double_val(records[index].get(size_field))),
                1.0f, 40.0f);
        };

        for (size_t i = 1; i < records.size(); ++i) {
            const auto [x0, y0] = point(i - 1);
            const auto [x1, y1] = point(i);
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 0.0001f) continue;

            const float half_width = 0.25f * (width(i - 1) + width(i));
            const float nx = -dy * half_width / length;
            const float ny = dx * half_width / length;
            char path[256];
            std::snprintf(path, sizeof(path),
                          "M %.4f %.4f L %.4f %.4f L %.4f %.4f L %.4f %.4f Z",
                          x0 + nx, y0 + ny, x1 + nx, y1 + ny,
                          x1 - nx, y1 - ny, x0 - nx, y0 - ny);

            auto segment = ctx.arena.create<Shape>();
            if (!segment) return;
            segment->set_id("trail-mark-segment-" + std::to_string(i - 1));
            segment->set_path(path);
            segment->set_fill(Color(0.24f, 0.51f, 1.0f, 0.75f));
            segment->clear_stroke();
            segment->set_position_absolute(true);
            segment->set_anchor(Anchor::TopLeft);
            segment->set_position(0, 0);
            ctx.overlay->add_child(segment);
        }
    }
};

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("trail", flex::chart::TrailMarkRenderer)
