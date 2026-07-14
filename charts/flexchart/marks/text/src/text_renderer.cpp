#include "flexchart/text/text_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_text_force_link(void) {}

namespace flex {
namespace chart {

class TextMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void TextMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
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

    // Fall back text_field to y_field
    if (text_field.empty()) text_field = y_field;

    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };

    int idx = 0;
    for (const auto& rec : records) {
        std::string x_val = get_string_val(rec.get(x_field));
        float y_val = (float)get_double_val(rec.get(y_field));
        std::string display_text = get_string_val(rec.get(text_field));

        float px = get_x_pos(x_val, ctx);
        float py = ctx.estimated_plot_h - (y_val * ctx.y_scale);

        Color c = palette[0];
        if (!color_field.empty()) {
            std::string cv = get_string_val(rec.get(color_field));
            unsigned int h = 0;
            for (size_t i = 0; i < cv.size(); i++) h = h * 31 + (unsigned char)cv[i];
            c = palette[h % palette.size()];
        }

        auto text_node = ctx.arena.create<Text>();
        text_node->set_content(display_text);
        text_node->set_font_size(font_size);
        text_node->set_color(c);
        text_node->set_position_absolute(true);
        text_node->set_position(px, py);
        ctx.overlay->add_child(text_node);

        if (ctx.instance) {
            std::string anim_id = "text_" + std::to_string(ctx.mark_index) + "_" + std::to_string(idx);
            auto tl = Timeline::create(anim_id.c_str(), ctx.arena);
            auto trk = tl->add_track("opacity");
            float delay = 0.5f + idx * 0.1f;
            trk->add_keyframe(delay, 0.0f);
            trk->add_keyframe(delay + 0.4f, 1.0f);
            ctx.instance->add_timeline(tl);
            ctx.instance->play(anim_id.c_str(), text_node);
            text_node->set_opacity(0.0f);
        }
        idx++;
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("text", flex::chart::TextMarkRenderer)
