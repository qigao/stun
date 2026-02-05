#include "chart_component_internal.h"

namespace flex {
namespace chart {

void TextMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records,
                              MarkRenderContext& ctx) {
    std::string x_field, y_field, text_field, color_field;
    float font_size = 12.0f;
    
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "text") text_field = enc->field;
        else if (enc->channel == "color") color_field = enc->field;
        else if (enc->channel == "size") {
            auto it = enc->config.find("value");
            if (it != enc->config.end()) font_size = (float)get_double_val(it->second);
        }
    }
    
    if (text_field.empty()) text_field = y_field;
    
    Color default_color(0.2f, 0.2f, 0.2f);
    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f)
    };
    
    int idx = 0;
    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float y_val = (float)get_double_val(rec.get(y_field));
        std::string text_val = get_string_val(rec.get(text_field));
        
        if (text_val.empty()) {
            text_val = std::to_string((int)get_double_val(rec.get(text_field)));
        }
        
        float px = get_x_pos(x_val, ctx);
        float py = ctx.estimated_plot_h - (y_val * ctx.y_scale);
        
        auto label = ctx.arena.create<Text>();
        label->set_content(text_val);
        label->set_font_size(font_size);
        
        Color c = default_color;
        if (!color_field.empty()) {
            std::string cat = get_string_val(rec.get(color_field));
            size_t hash = std::hash<std::string>{}(cat);
            c = palette[hash % palette.size()];
        }
        label->set_color(c);
        
        label->set_position_absolute(true);
        label->set_anchor(Anchor::Bottom);
        label->set_position(px, py - 5.0f);
        
        ctx.overlay->add_child(label);
        
        if (ctx.instance) {
            std::string anim_id = "txt_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.8f + idx * 0.05f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.3f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), label);
            label->set_opacity(0.0f);
        }
        
        idx++;
    }
}

} // namespace chart
} // namespace flex
