#include "chart_component_internal.h"
#include <cmath>
#include <algorithm>

namespace flex {
namespace chart {

void PieMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "theta") y_field = enc->field; // Pie uses theta for values
        else if (enc->channel == "color") x_field = enc->field; // Pie uses color for categories
    }
    if (y_field.empty() && !mark->encodings.empty()) {
        // Fallback to channel y if theta not specified
        for (const auto& enc : mark->encodings) { if (enc->channel == "y") y_field = enc->field; }
    }

    double total = 0;
    for (const auto& rec : records) total += get_double_val(rec.get(y_field));
    if (total <= 0) return;

    float cx = ctx.available_plot_w / 2.0f;
    float cy = ctx.estimated_plot_h / 2.0f;
    float r = (std::min)(ctx.available_plot_w, ctx.estimated_plot_h) * 0.4f;

    float start_angle = -FLEX_PI / 2.0f; // Start at top
    
    // Palette for pie slices
    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };

    for (size_t i = 0; i < records.size(); ++i) {
        double val = get_double_val(records[i].get(y_field));
        float slice_angle = static_cast<float>((val / total) * 2.0 * FLEX_PI);
        float end_angle = start_angle + slice_angle;

        float x1 = cx + r * std::cos(start_angle);
        float y1 = cy + r * std::sin(start_angle);
        float x2 = cx + r * std::cos(end_angle);
        float y2 = cy + r * std::sin(end_angle);

        int large_arc = slice_angle > FLEX_PI ? 1 : 0;
        
        char buf[256];
        stbsp_snprintf(buf, sizeof(buf), "M %.4f %.4f L %.4f %.4f A %.4f %.4f 0 %d 1 %.4f %.4f Z",
                       cx, cy, x1, y1, r, r, large_arc, x2, y2);

        auto slice_shape = ctx.arena.create<Shape>();
        slice_shape->set_path(buf);
        slice_shape->set_fill(palette[i % palette.size()]);
        slice_shape->set_position_absolute(true);
        slice_shape->set_position(0, 0); // Path is already absolute (cx, cy)
        ctx.overlay->add_child(slice_shape);

        // --- Entrance Animation: Scale In ---
        if (ctx.instance) {
            std::string anim_id = "pie_entry_" + std::to_string(i);
            auto timeline = Timeline::create(anim_id.c_str(), *ctx.instance->object_allocator());
            auto track = timeline->add_track("scale");
            float delay = i * 0.1f;
            track->add_keyframe(delay, 0.0f);
            track->add_keyframe(delay + 0.5f, 1.0f, Easing::ease_out());
            ctx.instance->add_timeline(timeline);
            ctx.instance->play(anim_id.c_str(), slice_shape);
            slice_shape->set_scale(0.0f);
        }

        // --- Hover Interactions ---
        if (ctx.instance) {
            slice_shape->on_hover_enter([inst=ctx.instance, shape=slice_shape, i](PointerEvent&) {
                std::string anim_id = "pie_hover_in_" + std::to_string(i);
                auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                auto track = timeline->add_track("scale");
                track->add_keyframe(0.0f, 1.0f);
                track->add_keyframe(0.15f, 1.05f, Easing::ease_out());
                inst->add_timeline(timeline);
                inst->play(anim_id.c_str(), shape);
            });

            slice_shape->on_hover_leave([inst=ctx.instance, shape=slice_shape, i](PointerEvent&) {
                std::string anim_id = "pie_hover_out_" + std::to_string(i);
                auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                auto track = timeline->add_track("scale");
                track->add_keyframe(0.0f, 1.05f);
                track->add_keyframe(0.15f, 1.0f, Easing::ease_out());
                inst->add_timeline(timeline);
                inst->play(anim_id.c_str(), shape);
            });
        }

        // Labels
        auto label = ctx.arena.create<Text>();
        std::string label_text = get_string_val(records[i].get(x_field)) + ": " + std::to_string((int)val);
        label->set_content(label_text);
        label->set_font_size(14.0f);
        label->set_color(Color(0.25f, 0.35f, 0.45f));
        label->set_position_absolute(true); // CRITICAL: Fix stacking
        
        float label_r = r * 1.25f;
        float label_angle = start_angle + slice_angle / 2.0f;
        float lx = cx + cosf(label_angle) * label_r;
        float ly = cy + sinf(label_angle) * label_r;
        
        label->set_anchor(Anchor::Center);
        label->set_position(lx, ly);
        ctx.overlay->add_child(label);

        start_angle = end_angle;
    }
}

} // namespace chart
} // namespace flex
