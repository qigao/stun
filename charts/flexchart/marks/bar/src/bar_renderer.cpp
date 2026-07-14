#include "flexchart/bar/bar_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include <tlog.h>
#include <algorithm>
#include <cstdio>

extern "C" void flexchart_bar_force_link(void) {}

namespace flex {
namespace chart {

class BarMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void BarMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    std::string x_field, y_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        if (enc->channel == "y") y_field = enc->field;
    }

    std::map<std::string, const Record*> record_map;
    for (const auto& rec : records) {
        record_map[get_string_val(rec.get(x_field))] = &rec;
    }

    float inner_w = ctx.available_plot_w - 20.0f;
    float group_w = inner_w / (std::max)(1ULL, (unsigned long long)ctx.x_labels.size());

    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };

    TLOG_INFO("Rendering Bars (Mark {}): group_w={:.2f}", ctx.mark_index, group_w);
    for (size_t i = 0; i < ctx.x_labels.size(); ++i) {
        const auto& label_text = ctx.x_labels[i];

        Group* group = nullptr;
        if (ctx.mark_index == 0) {
            group = ctx.arena.create<Group>();
            group->set_id("bar-group-" + std::to_string(i));
            group->set_layout(LayoutMode::Flex);
            group->set_flex_direction(FlexDirection::ColumnReverse);
            group->set_justify_content(JustifyContent::Start);
            group->set_align_items(AlignItems::Center);
            group->set_flex_basis(group_w);
            group->set_flex_grow(1.0f);
            ctx.plot->add_child(group);
        } else {
            group = dynamic_cast<Group*>(ctx.plot->find_child("bar-group-" + std::to_string(i)));
        }

        if (!group) continue;

        auto it = record_map.find(label_text);
        if (it != record_map.end()) {
            float val = (float)get_double_val(it->second->get(y_field));
            float bar_h = val * ctx.y_scale;

            float corner_radius = 4.0f;
            auto it_cr = mark->styles.find("corner-radius");
            if (it_cr != mark->styles.end()) corner_radius = (float)get_double_val(it_cr->second);

            auto bar_shape = ctx.arena.create<Shape>();

            float w = 32.0f;
            float h = bar_h;
            // Clamp r so the arcs never exceed either dimension
            float r = (std::min)(corner_radius, (std::min)(w / 2.0f, h / 2.0f));
            char path_buf[256];
            // Rounded-top rectangle (clockwise from bottom-left):
            // M 0 h  → bottom-left
            // L 0 r  → up the left edge to start of top-left arc
            // A r r 0 0 1  r 0  → top-left arc
            // L w-r 0  → top edge between arcs   ← was missing!
            // A r r 0 0 1  w r  → top-right arc
            // L w h  → down the right edge to bottom-right
            // Z      → close
            stbsp_snprintf(path_buf, sizeof(path_buf),
                "M 0 %.2f L 0 %.2f A %.2f %.2f 0 0 1 %.2f 0 L %.2f 0 A %.2f %.2f 0 0 1 %.2f %.2f L %.2f %.2f Z",
                h, r,   r, r, r,   w - r,   r, r, w, r,   w, h);

            bar_shape->set_path(path_buf);
            bar_shape->set_layout_size(w, h);

            LinearGradient grad(0, 0, 0, 1);
            grad.add_stop(0.0f, Color(0.35f, 0.65f, 1.0f));
            grad.add_stop(1.0f, Color(0.15f, 0.35f, 0.8f));
            bar_shape->set_fill(grad);

            bar_shape->set_anchor(Anchor::Bottom);

            if (ctx.instance) {
                std::string anim_id = "bar_grow_" + std::to_string(ctx.mark_index) + "_" + std::to_string(i);
                auto timeline = Timeline::create(anim_id.c_str(), ctx.arena);
                auto track = timeline->add_track("scaleY");
                float delay = (i * 0.05f) + (ctx.mark_index * 0.2f);
                track->add_keyframe(delay, 0.0f);
                track->add_keyframe(delay + 0.6f, 1.0f, Easing::ease_out_cubic());

                ctx.instance->add_timeline(timeline);
                ctx.instance->play(anim_id.c_str(), bar_shape);
                bar_shape->set_scale(1.0f, 0.0f);
            }

            group->add_child(bar_shape);

            if (ctx.instance) {
                bar_shape->on_hover_enter([inst=ctx.instance, shape=bar_shape, mark_idx=ctx.mark_index, i](PointerEvent&) {
                    std::string anim_id = "bar_hover_in_" + std::to_string(mark_idx) + "_" + std::to_string(i);
                    auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                    auto track = timeline->add_track("scale");
                    track->add_keyframe(0.0f, 1.0f);
                    track->add_keyframe(0.2f, 1.05f, Easing::ease_out());
                    inst->add_timeline(timeline);
                    inst->play(anim_id.c_str(), shape);
                });

                bar_shape->on_hover_leave([inst=ctx.instance, shape=bar_shape, mark_idx=ctx.mark_index, i](PointerEvent&) {
                    std::string anim_id = "bar_hover_out_" + std::to_string(mark_idx) + "_" + std::to_string(i);
                    auto timeline = Timeline::create(anim_id.c_str(), *inst->object_allocator());
                    auto track = timeline->add_track("scale");
                    track->add_keyframe(0.0f, 1.05f);
                    track->add_keyframe(0.2f, 1.0f, Easing::ease_out());
                    inst->add_timeline(timeline);
                    inst->play(anim_id.c_str(), shape);
                });
            }
        }
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("bar", flex::chart::BarMarkRenderer)
