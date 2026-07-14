#include "flexchart/errorbar/errorbar_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_errorbar_force_link(void) {}

namespace flex {
namespace chart {

class ErrorbarMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void ErrorbarMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field, y_error_field, y_error_min_field, y_error_max_field, color_field;
    float cap_size = 8.0f;

    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "yError") y_error_field = enc->field;
        else if (enc->channel == "yErrorMin") y_error_min_field = enc->field;
        else if (enc->channel == "yErrorMax") y_error_max_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
    }

    auto it_cap = mark->styles.find("cap-size");
    if (it_cap != mark->styles.end()) cap_size = (float)get_double_val(it_cap->second);

    Color default_color(0.24f, 0.51f, 1.0f);

    int idx = 0;
    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float y_val = (float)get_double_val(rec.get(y_field));

        float y_min, y_max;
        if (!y_error_min_field.empty() && !y_error_max_field.empty()) {
            y_min = (float)get_double_val(rec.get(y_error_min_field));
            y_max = (float)get_double_val(rec.get(y_error_max_field));
        } else if (!y_error_field.empty()) {
            float err = (float)get_double_val(rec.get(y_error_field));
            y_min = y_val - err;
            y_max = y_val + err;
        } else {
            y_min = y_val;
            y_max = y_val;
        }

        float px = get_x_pos(x_val, ctx);
        float py_center = ctx.estimated_plot_h - (y_val * ctx.y_scale);
        float py_min = ctx.estimated_plot_h - (y_min * ctx.y_scale);
        float py_max = ctx.estimated_plot_h - (y_max * ctx.y_scale);

        Color c = default_color;
        if (!color_field.empty()) {
            std::string cv = get_string_val(rec.get(color_field));
            if (cv.length() > 0 && cv[0] == '#') {
                unsigned int hex = std::stoul(cv.substr(1), nullptr, 16);
                c = Color(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f);
            }
        }

        // Draw error bar path: vertical line + top cap + bottom cap
        auto err_shape = ctx.arena.create<Shape>();
        char path_buf[256];
        float half_cap = cap_size / 2.0f;
        stbsp_snprintf(path_buf, sizeof(path_buf),
            "M %.2f %.2f L %.2f %.2f M %.2f %.2f L %.2f %.2f M %.2f %.2f L %.2f %.2f",
            px, py_max, px, py_min,
            px - half_cap, py_max, px + half_cap, py_max,
            px - half_cap, py_min, px + half_cap, py_min);
        err_shape->set_path(path_buf);
        err_shape->set_stroke(c, 2.0f);
        err_shape->set_fill(Color(0, 0, 0, 0));
        err_shape->set_position_absolute(true);
        err_shape->set_position(0, 0);
        ctx.overlay->add_child(err_shape);

        // Draw center point
        auto center_dot = ctx.arena.create<Shape>();
        char dot_buf[128];
        stbsp_snprintf(dot_buf, sizeof(dot_buf),
            "M %.2f %.2f m -3 0 a 3 3 0 1 0 6 0 a 3 3 0 1 0 -6 0",
            px, py_center);
        center_dot->set_path(dot_buf);
        center_dot->set_fill(c);
        center_dot->set_position_absolute(true);
        center_dot->set_position(0, 0);
        ctx.overlay->add_child(center_dot);

        if (ctx.instance) {
            std::string anim_id = "errorbar_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.5f + idx * 0.1f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), err_shape);
            err_shape->set_opacity(0.0f);

            std::string anim_id2 = "errorbar_dot_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl2 = Timeline::create(anim_id2.c_str(), ctx.arena);
            auto trk2 = tl2->add_track("opacity");
            trk2->add_keyframe(delay, 0.0f);
            trk2->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl2);
            ctx.instance->play(anim_id2.c_str(), center_dot);
            center_dot->set_opacity(0.0f);
        }
        idx++;
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("errorbar", flex::chart::ErrorbarMarkRenderer)
