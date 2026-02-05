#include "chart_component_internal.h"

namespace flex {
namespace chart {

void PointMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field, size_field, color_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
        if (enc->channel == "size") size_field = enc->field;
        if (enc->channel == "color") color_field = enc->field;
    }

    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };
    std::map<std::string, Color> color_map;

    for (size_t i = 0; i < records.size(); ++i) {
        std::string x_val = get_string_val(records[i].get(x_field));
        float px = get_x_pos(x_val, ctx);
        float val_y = (float)(get_double_val(records[i].get(y_field)));
        float py = ctx.estimated_plot_h - (val_y * ctx.y_scale);
        
        float radius = 4.5f;
        if (!size_field.empty()) {
            float s_val = (float)get_double_val(records[i].get(size_field));
            radius = (s_val / 100.0f) * 20.0f + 2.0f; // Scale to 2-22 range
        }

        auto point_shape = ctx.arena.create<Shape>();
        point_shape->set_circle(radius);
        
        // Bubble Color based on category
        Color base_color(0.24f, 0.51f, 1.0f); // Default blue
        if (!color_field.empty()) {
            std::string cat = get_string_val(records[i].get(color_field));
            if (color_map.find(cat) == color_map.end()) {
                color_map[cat] = palette[color_map.size() % palette.size()];
            }
            base_color = color_map[cat];
        }

        // 3D Bubble Radial Gradient
        RadialGradient grad(0.5f, 0.5f, 0.5f);
        grad.fx = 0.35f; grad.fy = 0.35f;
        grad.add_stop(0.0f, Color(base_color.r + 0.3f, base_color.g + 0.3f, base_color.b + 0.3f, 0.9f));
        grad.add_stop(1.0f, Color(base_color.r, base_color.g, base_color.b, 0.7f));
        point_shape->set_fill(grad);

        point_shape->set_stroke(Color(1.0f, 1.0f, 1.0f), 1.0f);
        point_shape->set_position_absolute(true);
        point_shape->set_anchor(Anchor::Center);
        point_shape->set_position(px, py);
        ctx.overlay->add_child(point_shape);

        // Entry Animation: Pop In
        if (ctx.instance) {
            std::string anim_id = "point_pop_" + std::to_string(ctx.mark_index) + "_" + std::to_string(i);
            auto timeline = Timeline::create(anim_id.c_str(), ctx.arena);
            auto track = timeline->add_track("scale");
            float delay = i * 0.02f;
            track->add_keyframe(delay, 0.0f);
            track->add_keyframe(delay + 0.5f, 1.0f, Easing::bounce_out());
            ctx.instance->add_timeline(timeline);
            ctx.instance->play(anim_id.c_str(), point_shape);
            point_shape->set_scale(0.0f);
            
            // --- Hover Interactions ---
            point_shape->on_hover_enter([inst=ctx.instance, shape=point_shape, mark_idx=ctx.mark_index, i](PointerEvent&) {
                std::string anim_id = "point_hover_in_" + std::to_string(mark_idx) + "_" + std::to_string(i);
                auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                auto track = timeline->add_track("scale");
                track->add_keyframe(0.0f, 1.0f);
                track->add_keyframe(0.15f, 1.3f, Easing::ease_out());
                inst->add_timeline(timeline);
                inst->play(anim_id.c_str(), shape);
            });

            point_shape->on_hover_leave([inst=ctx.instance, shape=point_shape, mark_idx=ctx.mark_index, i](PointerEvent&) {
                std::string anim_id = "point_hover_out_" + std::to_string(mark_idx) + "_" + std::to_string(i);
                auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                auto track = timeline->add_track("scale");
                track->add_keyframe(0.0f, 1.3f);
                track->add_keyframe(0.15f, 1.0f, Easing::ease_out());
                inst->add_timeline(timeline);
                inst->play(anim_id.c_str(), shape);
            });
        }
        
        // Value Label
        auto label = ctx.arena.create<Text>();
        label->set_content(std::to_string((int)val_y));
        label->set_font_size(11.0f);
        label->set_color(Color(0.44f, 0.5f, 0.56f));
        label->set_position_absolute(true);
        label->set_position(px, py - 12.0f);
        label->set_anchor(Anchor::Center);
        ctx.overlay->add_child(label);
    }
}

} // namespace chart
} // namespace flex
