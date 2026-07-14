#include "flexchart/line/line_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <cstdio>

extern "C" void flexchart_line_force_link(void) {}

namespace flex {
namespace chart {

class LineMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void LineMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
    }

    auto line_shape = ctx.arena.create<Shape>();
    line_shape->set_id("line-mark-shape");
    line_shape->set_position_absolute(true);
    line_shape->set_anchor(Anchor::TopLeft);
    line_shape->set_position(0, 0);

    std::string path_data = "";
    bool first = true;
    for (size_t i = 0; i < records.size(); ++i) {
        std::string x_val = get_string_val(records[i].get(x_field));
        float px = get_x_pos(x_val, ctx);

        float val = (float)(get_double_val(records[i].get(y_field)));
        float py = ctx.estimated_plot_h - (val * ctx.y_scale);

        char buf[64];
        if (first) {
            snprintf(buf, sizeof(buf), "M %g %g", px, py);
            first = false;
        } else {
            snprintf(buf, sizeof(buf), " L %g %g", px, py);
        }
        path_data += buf;

        auto label = ctx.arena.create<Text>();
        label->set_content(std::to_string((int)val));
        label->set_font_size(11.0f);
        label->set_color(Color(0.44f, 0.5f, 0.56f));
        label->set_position_absolute(true);
        label->set_position(px, py - 12.0f);
        label->set_anchor(Anchor::Center);
        ctx.overlay->add_child(label);
    }
    if (!path_data.empty()) {
        line_shape->set_path(path_data);
        line_shape->set_stroke(Color(1.0f, 0.42f, 0.42f), 2.5f);
        line_shape->clear_fill();
        ctx.overlay->add_child(line_shape);

        if (ctx.instance) {
            std::string anim_id = "line_reveal_" + std::to_string(ctx.mark_index);
            auto timeline = Timeline::create(anim_id.c_str(), ctx.arena);
            ctx.instance->add_timeline(timeline);

            line_shape->set_opacity(0.0f);
            auto op_track = timeline->add_track("opacity");
            op_track->add_keyframe(0.2f, 0.0f);
            op_track->add_keyframe(1.2f, 1.0f, Easing::ease_out());

            ctx.instance->play(anim_id.c_str(), line_shape);
        }
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("line", flex::chart::LineMarkRenderer)
