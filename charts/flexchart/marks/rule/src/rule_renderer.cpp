#include "flexchart/rule/rule_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_rule_force_link(void) {}

namespace flex {
namespace chart {

class RuleMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) override;
};

void RuleMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string y_field, x_field, x2_field, y2_field, color_field;
    float stroke_width = 2.0f;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "x2") x2_field = enc->field;
        else if (enc->channel == "y2") y2_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
        else if (enc->channel == "strokeWidth") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) stroke_width = (float)get_double_val(it->second);
        }
    }
    Color default_color(0.6f, 0.6f, 0.6f);
    int idx = 0;
    for (const auto& rec : records) {
        float x1 = 0, y1 = 0, x2 = ctx.available_plot_w, y2 = 0;
        if (!y_field.empty() && x_field.empty()) {
            float y_val = (float)get_double_val(rec.get(y_field));
            y1 = y2 = ctx.estimated_plot_h - (y_val * ctx.y_scale);
            x1 = 0; x2 = ctx.available_plot_w;
        } else if (!x_field.empty() && y_field.empty()) {
            std::string x_val = get_string_val(rec.get(x_field));
            x1 = x2 = get_x_pos(x_val, ctx);
            y1 = 0; y2 = ctx.estimated_plot_h;
        } else {
            if (!x_field.empty()) { std::string xv = get_string_val(rec.get(x_field)); x1 = get_x_pos(xv, ctx); }
            if (!x2_field.empty()) { std::string xv = get_string_val(rec.get(x2_field)); x2 = get_x_pos(xv, ctx); } else { x2 = x1; }
            if (!y_field.empty()) { float yv = (float)get_double_val(rec.get(y_field)); y1 = ctx.estimated_plot_h - (yv * ctx.y_scale); }
            if (!y2_field.empty()) { float yv = (float)get_double_val(rec.get(y2_field)); y2 = ctx.estimated_plot_h - (yv * ctx.y_scale); } else { y2 = y1; }
        }
        Color c = default_color;
        if (!color_field.empty()) {
            std::string cv = get_string_val(rec.get(color_field));
            if (cv.length() > 0 && cv[0] == '#') {
                unsigned int hex = std::stoul(cv.substr(1), nullptr, 16);
                c = Color(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f);
            }
        }
        auto rule = ctx.arena.create<Shape>();
        char path_buf[128];
        stbsp_snprintf(path_buf, sizeof(path_buf), "M %.2f %.2f L %.2f %.2f", x1, y1, x2, y2);
        rule->set_path(path_buf);
        rule->set_stroke(c, stroke_width);
        rule->set_fill(Color(0, 0, 0, 0));
        rule->set_position_absolute(true);
        rule->set_position(0, 0);
        ctx.overlay->add_child(rule);
        if (ctx.instance) {
            std::string anim_id = "rule_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.5f + idx * 0.1f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), rule);
            rule->set_opacity(0.0f);
        }
        idx++;
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("rule", flex::chart::RuleMarkRenderer)
