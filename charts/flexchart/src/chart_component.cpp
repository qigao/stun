#include "flexchart/chart_component.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"
#include "flex/runtime/group.h"
#include "flex/runtime/text.h"
#include "flex/runtime/shape.h"
#include "flex/runtime/component.h"
#include "flex.h"
#include "flex/runtime/timeline.h"
#include "flexchart/flexchart.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <tlog.h>

namespace flex {
namespace chart {

void ChartComponent::register_component() {
    ComponentRegistry::instance().register_component("Chart", [](const Props& props) -> std::shared_ptr<Node> {
        std::string source = get_prop_string(props, "source", "");
        if (source.empty()) return nullptr;

        std::string content;
        std::ifstream file(source);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            content = buffer.str();
        } else {
            content = source;
        }

        AstProgram program;
        std::string error;
        if (!parse_chart(content.c_str(), &program, error)) {
             TLOG_ERROR("Chart parse error: {}", error);
             return nullptr;
        }

        if (program.views.empty()) return nullptr;
        auto chart_ast = std::dynamic_pointer_cast<AstChart>(program.views[0]);
        if (!chart_ast) return nullptr;

        // Build the chart into a flex node tree using a component-local Instance.
        // The instance lifetime is tied to the returned Group via shared_ptr.
        Instance::SharedPtr inst = Instance::create(800.0f, 600.0f);
        Group* root = ChartComponent::build(chart_ast, *inst);
        if (!root) return nullptr;

        // Wrap the raw Group pointer in a shared_ptr that keeps `inst` alive.
        return std::shared_ptr<Node>(root, [inst](Node*) mutable { inst.reset(); });
    });
}

Group* ChartComponent::build(const std::shared_ptr<AstChart>& chart,
                             Instance& instance) {
    return build(chart, instance, {});
}

Group* ChartComponent::build(const std::shared_ptr<AstChart>& chart,
                             Instance& instance,
                             const ChartComponentBuildOptions& options) {
    ensure_builtin_mark_renderers_linked();
    ArenaAllocator& arena = *instance.object_allocator();
    auto root = Group::create(arena);
    root->set_id("chart-root");
    
    // Safe dimension parser: handles "", "px" suffix, "%" suffix, and stof exceptions.
    auto parse_size = [](const std::string& s, float default_px, float percent_base) -> float {
        if (s.empty()) return default_px;
        try {
            if (s.back() == '%') {
                float pct = std::stof(s.substr(0, s.size() - 1));
                return percent_base * (pct / 100.0f);
            }
            // Strip a trailing "px" suffix if present
            size_t end = s.size();
            if (end >= 2 && s[end-1] == 'x' && s[end-2] == 'p') end -= 2;
            return std::stof(s.substr(0, end));
        } catch (...) {
            TLOG_WARN("Chart: could not parse size '{}', using default {:.0f}", s, default_px);
            return default_px;
        }
    };

    float root_w = parse_size(chart->width,  800.0f, 800.0f);
    float root_h = parse_size(chart->height, 600.0f, 600.0f);

    root->set_layout_size(root_w, root_h);
    root->set_layout(LayoutMode::Flex);
    root->set_flex_direction(FlexDirection::Column);
    root->set_align_items(AlignItems::Stretch);
    root->set_padding(40.0f);
    
    // 1. Title
    if (options.show_title && !chart->title.empty()) {
        auto title = arena.create<Text>();
        title->set_content(chart->title);
        title->set_font_size(28.0f);
        title->set_color(Color(0.12f, 0.14f, 0.17f));
        title->set_font_weight(FontWeight::Bold);
        title->set_margin(0, 0, 20.0f, 0); 
        title->set_align_self(AlignSelf::Center);
        root->add_child(title);

        // Entry Animation
        auto tl = Timeline::create("title_fade", arena);
        auto trk = tl->add_track("opacity");
        trk->add_keyframe(0.0f, 0.0f);
        trk->add_keyframe(0.8f, 1.0f);
        instance.add_timeline(tl);
        instance.play("title_fade", title);
        title->set_opacity(0.0f);
    }
    
    // 2. Scan Type & Data
    bool has_pie = false;
    for (const auto& m : chart->marks) if (m->type == "pie") has_pie = true;

    std::vector<Record> all_records;
    float data_y_max = 0.0f;
    float data_x_min = 0.0f, data_x_max = 0.0f;
    bool has_numeric_x = false;
    std::vector<std::string> x_labels;
    for (const auto& mark : chart->marks) {
        auto ds = find_dataset(chart, mark->data_ref);
        std::vector<Record> records;
        if (!mark->expr.empty() && !mark->ranges.empty()) {
            records = generate_expr_records(mark->expr, mark->ranges);
            has_numeric_x = true;
        } else {
            records = get_records(ds);
        }
        apply_computed_fields(records, mark->encodings);
        // Resolve field names after computed-field expansion (expression
        // encodings produce synthetic fields stored in the record itself).
        std::string x_field, y_field;
        if (!records.empty()) {
            const Record& first_rec = records.front();
            for (const auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = resolved_field_name(*enc, first_rec);
                if (enc->channel == "y") y_field = resolved_field_name(*enc, first_rec);
            }
            if (x_field.empty() || y_field.empty()) {
                for (const auto& enc : mark->encodings) {
                    if (x_field.empty() && enc->channel == "color") x_field = resolved_field_name(*enc, first_rec);
                    if (y_field.empty() && enc->channel == "theta") y_field = resolved_field_name(*enc, first_rec);
                }
            }
        } else {
            for (const auto& enc : mark->encodings) {
                if (enc->channel == "x") x_field = enc->field;
                if (enc->channel == "y") y_field = enc->field;
            }
        }
        // Detect if x data is numeric (from expression ranges)
        if (ds && !ds->expr.empty()) has_numeric_x = true;
        bool first_x = true;
        for (const auto& rec : records) {
            float val = (float)get_double_val(rec.get(y_field));
            if (val > data_y_max) data_y_max = val;
            if (has_numeric_x) {
                float xv = (float)get_double_val(rec.get(x_field));
                if (first_x) { data_x_min = data_x_max = xv; first_x = false; }
                else { if (xv < data_x_min) data_x_min = xv; if (xv > data_x_max) data_x_max = xv; }
            } else {
                std::string x_val = get_string_val(rec.get(x_field));
                if (!x_val.empty()) {
                    bool found = false;
                    for (const auto& l : x_labels) if (l == x_val) found = true;
                    if (!found) x_labels.push_back(x_val);
                }
            }
        }
    }

    // Lookup axis definitions for title/ticks
    auto x_axis_def = find_axis(chart, "x");
    auto y_axis_def = find_axis(chart, "y");
    std::string x_axis_title, y_axis_title;
    int y_ticks = 4, x_ticks = 5;
    if (x_axis_def) {
        auto it = x_axis_def->properties.find("title");
        if (it != x_axis_def->properties.end()) x_axis_title = get_string_val(it->second);
        auto tt = x_axis_def->properties.find("ticks");
        if (tt != x_axis_def->properties.end()) x_ticks = (int)get_double_val(tt->second);
    }
    if (y_axis_def) {
        auto it = y_axis_def->properties.find("title");
        if (it != y_axis_def->properties.end()) y_axis_title = get_string_val(it->second);
        auto tt = y_axis_def->properties.find("ticks");
        if (tt != y_axis_def->properties.end()) y_ticks = (int)get_double_val(tt->second);
    }

    // 3. Legend (Inserted before Plot)
    std::vector<std::pair<std::string, Color>> legend_items;
    std::vector<Color> palette = {
        Color(0.24f, 0.51f, 1.0f), Color(0.12f, 0.73f, 0.52f),
        Color(1.0f, 0.75f, 0.04f), Color(1.0f, 0.34f, 0.2f),
        Color(0.6f, 0.4f, 0.8f),   Color(0.0f, 0.8f, 0.8f)
    };
    for (const auto& mark : chart->marks) {
        std::string color_field;
        for (const auto& enc : mark->encodings) if (enc->channel == "color") color_field = enc->field;
        if (!color_field.empty()) {
            auto ds = find_dataset(chart, mark->data_ref);
            auto records = get_records(ds);
            for (const auto& rec : records) {
                std::string cat = get_string_val(rec.get(color_field));
                if (!cat.empty()) {
                    bool found = false;
                    for (const auto& item : legend_items) if (item.first == cat) found = true;
                    if (!found) legend_items.push_back({cat, palette[legend_items.size() % palette.size()]});
                }
            }
        }
    }

    if (options.show_legend && !legend_items.empty()) {
        auto legend_root = arena.create<Group>();
        legend_root->set_layout(LayoutMode::Flex);
        legend_root->set_flex_direction(FlexDirection::Row);
        legend_root->set_justify_content(JustifyContent::Center);
        legend_root->set_margin(0, 0, 20.0f, 0);
        root->add_child(legend_root);
        for (size_t i = 0; i < legend_items.size(); ++i) {
            auto item = arena.create<Group>();
            item->set_layout(LayoutMode::Flex);
            item->set_flex_direction(FlexDirection::Row);
            item->set_align_items(AlignItems::Center);
            item->set_margin(0, 20.0f, 0, 20.0f); // More spacing
            legend_root->add_child(item);
            auto dot = arena.create<Shape>();
            dot->set_circle(5.0f);
            dot->set_fill(legend_items[i].second);
            dot->set_margin(0, 5.0f, 0, 0);
            item->add_child(dot);
            auto text = arena.create<Text>();
            text->set_content(legend_items[i].first);  // legend label text
            text->set_font_size(12.0f);
            text->set_color(Color(0.44f, 0.5f, 0.56f));
            item->add_child(text);

            // Entry Animation
            std::string id = "leg_" + std::to_string(i);
            auto tl = Timeline::create(id.c_str(), arena);
            auto trk = tl->add_track("opacity");
            float d = 0.4f + i * 0.1f;
            trk->add_keyframe(d, 0.0f);
            trk->add_keyframe(d + 0.3f, 1.0f);
            instance.add_timeline(tl);
            instance.play(id.c_str(), item);
            item->set_opacity(0.0f);
        }
    }

    // 4. Content (Plot Area)
    auto content = arena.create<Group>();
    content->set_id("chart-content");
    content->set_layout(LayoutMode::Flex);
    content->set_flex_direction(FlexDirection::Row);
    content->set_align_items(AlignItems::Stretch); // Allow children (Y-Axis, Plot) to fill height
    content->set_flex_grow(1.0f);
    root->add_child(content);

    float padding = 40.0f;
    float yaxis_w = 60.0f;
    float yaxis_margin = 15.0f;
    float available_plot_w = root_w - (padding * 2);
    if (!has_pie) available_plot_w -= (yaxis_w + yaxis_margin);
    
    float occupied_h = 80.0f; // 2 * padding (top + bottom padding = 2 * 40.0)
    if (!chart->title.empty()) occupied_h += 48.0f;
    if (!legend_items.empty()) occupied_h += 32.0f;
    float full_plot_h = (std::max)(100.0f, root_h - occupied_h);
    
    float baseline_offset = has_pie ? 0.0f : 40.0f;
    float effective_plot_h = full_plot_h - baseline_offset;
    float y_max = (std::max)(100.0f, data_y_max * 1.1f);
    float y_scale = effective_plot_h / y_max;

    if (!has_pie) {
        auto yaxis = arena.create<Group>();
        yaxis->set_layout_width(yaxis_w);
        yaxis->set_layout_height(full_plot_h);
        yaxis->set_layout(LayoutMode::None); // Absolute positioning for precise alignment
        yaxis->set_margin(0, yaxis_margin, 0, 0);
        content->add_child(yaxis);

        float by = full_plot_h - baseline_offset; // Baseline Y position

        for (int i = 0; i <= y_ticks; ++i) {
            float val = i * (y_max / (float)y_ticks);
            float py = by - (val * y_scale);

            auto label = arena.create<Text>();
            // Format: use decimal for small ranges, integer for large
            if (y_max < 10.0f) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f", val);
                label->set_content(buf);
            } else {
                label->set_content(std::to_string((int)val));
            }
            label->set_font_size(12.0f);
            label->set_color(Color(0.44f, 0.5f, 0.56f));

            label->set_position_absolute(true);
            label->set_anchor(Anchor::Right);
            label->set_position(yaxis_w - 5.0f, py); // Right align, 5px gap from axis

            yaxis->add_child(label);

            // Entry Animation
            std::string id = "ylbl_" + std::to_string(i);
            auto tl = Timeline::create(id.c_str(), arena);
            auto trk = tl->add_track("opacity");
            float d = 0.6f + (y_ticks - i) * 0.1f;
            trk->add_keyframe(d, 0.0f);
            trk->add_keyframe(d + 0.4f, 1.0f);
            instance.add_timeline(tl);
            instance.play(id.c_str(), label);
            label->set_opacity(0.0f);
        }

        // Y-axis title
        if (!y_axis_title.empty()) {
            auto ytitle = arena.create<Text>();
            ytitle->set_content(y_axis_title);
            ytitle->set_font_size(13.0f);
            ytitle->set_color(Color(0.3f, 0.34f, 0.38f));
            ytitle->set_position_absolute(true);
            ytitle->set_anchor(Anchor::Right);
            ytitle->set_position(yaxis_w - 5.0f, by + 15.0f);
            yaxis->add_child(ytitle);
        }
    }
    
    // 5. Plot Area (The part after Y-Axis)
    auto plot = arena.create<Group>();
    plot->set_id("plot-container");
    plot->set_flex_grow(1.0f);
    plot->set_layout(LayoutMode::None); // Layer container
    content->add_child(plot);

    auto marks_layer = arena.create<Group>();
    marks_layer->set_id("marks-layer");
    marks_layer->set_position_absolute(true);
    marks_layer->set_position(0, 0);
    marks_layer->set_layout_size(available_plot_w, full_plot_h);
    if (!has_pie) {
        marks_layer->set_layout(LayoutMode::Flex);
        marks_layer->set_flex_direction(FlexDirection::Row);
        marks_layer->set_align_items(AlignItems::Stretch); // Stretch bar columns to full height
        marks_layer->set_padding(0, 10.0f, baseline_offset, 10.0f);
    }
    plot->add_child(marks_layer);

    auto overlay_layer = arena.create<Group>();
    overlay_layer->set_id("overlay-layer");
    overlay_layer->set_position_absolute(true);
    overlay_layer->set_position(0, 0);
    overlay_layer->set_layout_size(available_plot_w, full_plot_h);
    plot->add_child(overlay_layer);

    if (!has_pie) {
        float by = full_plot_h - baseline_offset;
        
        // Main Axes Lines (L-Shape)
        auto axes = arena.create<Shape>();
        axes->set_stroke(Color(0.44f, 0.5f, 0.56f), 2.0f);
        axes->set_fill(Color(0,0,0,0)); // Transparent fill
        axes->set_position_absolute(true);
        axes->set_position(0, 0);
        
        char path_buf[128];
        // Path: Move to Top(0,0) -> Line to Origin(0, by) -> Line to Right(w, by)
        stbsp_snprintf(path_buf, sizeof(path_buf), "M 0 0 L 0 %.2f L %.2f %.2f", by, available_plot_w, by);
        axes->set_path(path_buf);
        overlay_layer->add_child(axes);

        // Y-Axis Arrow (Top)
        auto y_arrow = arena.create<Shape>();
        y_arrow->set_path("M -4 10 L 0 0 L 4 10 Z"); // Upward triangle
        y_arrow->set_fill(Color(0.44f, 0.5f, 0.56f));
        y_arrow->set_position_absolute(true);
        y_arrow->set_position(0, 0);
        overlay_layer->add_child(y_arrow);

        // X-Axis Arrow (Right)
        auto x_arrow = arena.create<Shape>();
        x_arrow->set_path("M -10 -4 L 0 0 L -10 4 Z"); // Rightward triangle
        x_arrow->set_fill(Color(0.44f, 0.5f, 0.56f));
        x_arrow->set_position_absolute(true);
        x_arrow->set_position(available_plot_w, by);
        overlay_layer->add_child(x_arrow);

        float inner_w = available_plot_w - 20.0f;
        if (has_numeric_x) {
            // Numeric x-axis ticks
            for (int i = 0; i <= x_ticks; ++i) {
                float val = data_x_min + i * ((data_x_max - data_x_min) / (float)x_ticks);
                float px = 10.0f + (inner_w * i / (float)x_ticks);

                auto lbl = arena.create<Text>();
                if ((data_x_max - data_x_min) < 10.0f) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.1f", val);
                    lbl->set_content(buf);
                } else {
                    lbl->set_content(std::to_string((int)val));
                }
                lbl->set_font_size(12.0f);
                lbl->set_color(Color(0.44f, 0.5f, 0.56f));
                lbl->set_anchor(Anchor::Top);
                lbl->set_position(px, full_plot_h - baseline_offset + 10.0f);
                overlay_layer->add_child(lbl);

                std::string anim_id = "xlab_" + std::to_string(i);
                auto tl = Timeline::create(anim_id.c_str(), arena);
                auto trk = tl->add_track("opacity");
                float delay = 0.6f + i * 0.05f;
                trk->add_keyframe(delay, 0.0f);
                trk->add_keyframe(delay + 0.4f, 1.0f);
                instance.add_timeline(tl);
                instance.play(anim_id.c_str(), lbl);
                lbl->set_opacity(0.0f);
            }
        } else {
            // Categorical x-axis labels
            float band_w = x_labels.empty() ? 0 : inner_w / x_labels.size();
            for (size_t i = 0; i < x_labels.size(); ++i) {
                auto lbl = arena.create<Text>();
                lbl->set_content(x_labels[i]);
                lbl->set_font_size(12.0f);
                lbl->set_color(Color(0.44f, 0.5f, 0.56f));
                lbl->set_anchor(Anchor::Top);
                lbl->set_position(10.0f + (i + 0.5f) * band_w, full_plot_h - baseline_offset + 10.0f);
                overlay_layer->add_child(lbl);

                std::string anim_id = "xlab_" + std::to_string(i);
                auto tl = Timeline::create(anim_id.c_str(), arena);
                auto trk = tl->add_track("opacity");
                float delay = 0.6f + i * 0.05f;
                trk->add_keyframe(delay, 0.0f);
                trk->add_keyframe(delay + 0.4f, 1.0f);
                instance.add_timeline(tl);
                instance.play(anim_id.c_str(), lbl);
                lbl->set_opacity(0.0f);
            }
        }

        // X-axis title
        if (!x_axis_title.empty()) {
            auto xtitle = arena.create<Text>();
            xtitle->set_content(x_axis_title);
            xtitle->set_font_size(13.0f);
            xtitle->set_color(Color(0.3f, 0.34f, 0.38f));
            xtitle->set_anchor(Anchor::Top);
            xtitle->set_position(available_plot_w / 2.0f, full_plot_h - baseline_offset + 26.0f);
            overlay_layer->add_child(xtitle);
        }
    }

    MarkRenderContext ctx{arena, marks_layer, overlay_layer, available_plot_w, effective_plot_h, y_scale, x_labels, &instance};
    for (const auto& mark : chart->marks) {
        std::vector<Record> records;
        if (!mark->expr.empty() && !mark->ranges.empty()) {
            records = generate_expr_records(mark->expr, mark->ranges);
        } else {
            auto ds = find_dataset(chart, mark->data_ref);
            records = get_records(ds);
        }
        apply_computed_fields(records, mark->encodings);
        auto renderer = MarkRendererRegistry::instance().create(mark->type);
        if (!renderer) {
            throw std::runtime_error("No renderer registered for chart mark type: " + mark->type);
        }
        renderer->render(mark, records, ctx);
        ctx.mark_index++;
    }

    return root;
}

} // namespace chart
} // namespace flex
