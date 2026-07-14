#include "flexchart/tick/tick_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_tick_force_link(void) {}

namespace flex {
namespace chart {

class TickMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void TickMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field, color_field;
    float tick_size = 10.0f;
    std::string orient;

    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
        else if (enc->channel == "size") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) tick_size = (float)get_double_val(it->second);
        }
    }

    auto it_orient = mark->styles.find("orient");
    if (it_orient != mark->styles.end()) orient = get_string_val(it_orient->second);

    bool vertical = (orient == "vertical");

    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };

    int idx = 0;
    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float y_val = (float)get_double_val(rec.get(y_field));

        float px = get_x_pos(x_val, ctx);
        float py = ctx.estimated_plot_h - (y_val * ctx.y_scale);

        Color c = palette[0];
        if (!color_field.empty()) {
            std::string cv = get_string_val(rec.get(color_field));
            unsigned int h = 0;
            for (size_t i = 0; i < cv.size(); i++) h = h * 31 + (unsigned char)cv[i];
            c = palette[h % palette.size()];
        }

        auto tick_shape = ctx.arena.create<Shape>();
        char path_buf[128];
        if (vertical) {
            stbsp_snprintf(path_buf, sizeof(path_buf), "M %.2f %.2f L %.2f %.2f",
                           px, py - tick_size / 2.0f, px, py + tick_size / 2.0f);
        } else {
            stbsp_snprintf(path_buf, sizeof(path_buf), "M %.2f %.2f L %.2f %.2f",
                           px - tick_size / 2.0f, py, px + tick_size / 2.0f, py);
        }
        tick_shape->set_path(path_buf);
        tick_shape->set_stroke(c, 2.0f);
        tick_shape->set_fill(Color(0, 0, 0, 0));
        tick_shape->set_position_absolute(true);
        tick_shape->set_position(0, 0);
        ctx.overlay->add_child(tick_shape);

        if (ctx.instance) {
            std::string anim_id = "tick_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.5f + idx * 0.1f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), tick_shape);
            tick_shape->set_opacity(0.0f);
        }
        idx++;
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("tick", flex::chart::TickMarkRenderer)
