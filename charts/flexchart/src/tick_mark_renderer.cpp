#include "chart_component_internal.h"

namespace flex {
namespace chart {

void TickMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records,
                              MarkRenderContext& ctx) {
    std::string x_field, y_field, color_field;
    float tick_size = 10.0f;
    float stroke_width = 2.0f;
    bool is_vertical = true;  // Default: vertical ticks on x-axis
    
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
        else if (enc->channel == "size") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) tick_size = (float)get_double_val(it->second);
        }
    }
    
    // If only y is specified, draw horizontal ticks
    if (x_field.empty() && !y_field.empty()) is_vertical = false;
    
    auto it = mark->styles.find("orient");
    if (it != mark->styles.end()) {
        std::string orient = get_string_val(it->second);
        is_vertical = (orient == "vertical");
    }
    
    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f)
    };
    Color default_color(0.3f, 0.3f, 0.3f);
    
    int idx = 0;
    for (const auto& rec : records) {
        float px = 0, py = 0;
        
        if (!x_field.empty()) {
            std::string x_val = get_string_val(rec.get(x_field));
            px = get_x_pos(x_val, ctx);
        }
        if (!y_field.empty()) {
            float y_val = (float)get_double_val(rec.get(y_field));
            py = ctx.estimated_plot_h - (y_val * ctx.y_scale);
        } else {
            py = ctx.estimated_plot_h;  // On baseline
        }
        
        Color c = default_color;
        if (!color_field.empty()) {
            std::string cat = get_string_val(rec.get(color_field));
            size_t hash = std::hash<std::string>{}(cat);
            c = palette[hash % palette.size()];
        }
        
        auto tick = ctx.arena.create<Shape>();
        char path_buf[128];
        
        if (is_vertical) {
            // Vertical tick: centered on x, extends up/down from y
            stbsp_snprintf(path_buf, sizeof(path_buf), 
                "M %.2f %.2f L %.2f %.2f", px, py - tick_size/2, px, py + tick_size/2);
        } else {
            // Horizontal tick: centered on y, extends left/right from x
            stbsp_snprintf(path_buf, sizeof(path_buf), 
                "M %.2f %.2f L %.2f %.2f", px - tick_size/2, py, px + tick_size/2, py);
        }
        
        tick->set_path(path_buf);
        tick->set_stroke(c, stroke_width);
        tick->set_fill(Color(0, 0, 0, 0));
        tick->set_position_absolute(true);
        tick->set_position(0, 0);
        
        ctx.overlay->add_child(tick);
        
        if (ctx.instance) {
            std::string anim_id = "tick_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.5f + idx * 0.03f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.2f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), tick);
            tick->set_opacity(0.0f);
        }
        
        idx++;
    }
}

} // namespace chart
} // namespace flex
