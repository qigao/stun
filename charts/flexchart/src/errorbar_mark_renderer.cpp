#include "chart_component_internal.h"

namespace flex {
namespace chart {

void ErrorbarMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records,
                                  MarkRenderContext& ctx) {
    std::string x_field, y_field, y_error_field, y_error_min_field, y_error_max_field, color_field;
    float stroke_width = 2.0f;
    float cap_size = 8.0f;
    
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "yError") y_error_field = enc->field;
        else if (enc->channel == "yErrorMin" || enc->channel == "y2") y_error_min_field = enc->field;
        else if (enc->channel == "yErrorMax") y_error_max_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
    }
    
    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f)
    };
    Color default_color(0.4f, 0.4f, 0.4f);
    
    int idx = 0;
    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float y_val = (float)get_double_val(rec.get(y_field));
        
        float y_min = y_val, y_max = y_val;
        
        // Symmetric error
        if (!y_error_field.empty()) {
            float err = (float)get_double_val(rec.get(y_error_field));
            y_min = y_val - err;
            y_max = y_val + err;
        }
        // Asymmetric error
        if (!y_error_min_field.empty()) {
            y_min = (float)get_double_val(rec.get(y_error_min_field));
        }
        if (!y_error_max_field.empty()) {
            y_max = (float)get_double_val(rec.get(y_error_max_field));
        }
        
        float px = get_x_pos(x_val, ctx);
        float py_center = ctx.estimated_plot_h - (y_val * ctx.y_scale);
        float py_min = ctx.estimated_plot_h - (y_min * ctx.y_scale);
        float py_max = ctx.estimated_plot_h - (y_max * ctx.y_scale);
        
        Color c = default_color;
        if (!color_field.empty()) {
            std::string cat = get_string_val(rec.get(color_field));
            size_t hash = std::hash<std::string>{}(cat);
            c = palette[hash % palette.size()];
        }
        
        // Draw error bar: vertical line + top cap + bottom cap
        auto errorbar = ctx.arena.create<Shape>();
        char path_buf[256];
        stbsp_snprintf(path_buf, sizeof(path_buf),
            "M %.2f %.2f L %.2f %.2f "  // Vertical line
            "M %.2f %.2f L %.2f %.2f "  // Top cap
            "M %.2f %.2f L %.2f %.2f",  // Bottom cap
            px, py_min, px, py_max,
            px - cap_size/2, py_max, px + cap_size/2, py_max,
            px - cap_size/2, py_min, px + cap_size/2, py_min);
        
        errorbar->set_path(path_buf);
        errorbar->set_stroke(c, stroke_width);
        errorbar->set_fill(Color(0, 0, 0, 0));
        errorbar->set_position_absolute(true);
        errorbar->set_position(0, 0);
        
        ctx.overlay->add_child(errorbar);
        
        // Center point
        auto center = ctx.arena.create<Shape>();
        center->set_circle(3.0f);
        center->set_fill(c);
        center->set_position_absolute(true);
        center->set_position(px, py_center);
        ctx.overlay->add_child(center);
        
        if (ctx.instance) {
            std::string anim_id = "err_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.6f + idx * 0.05f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.3f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), errorbar);
            ctx.instance->play(anim_id.c_str(), center);
            errorbar->set_opacity(0.0f);
            center->set_opacity(0.0f);
        }
        
        idx++;
    }
}

} // namespace chart
} // namespace flex
