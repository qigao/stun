#include "flexchart/mark_renderer_registry.h"
#include "chart_component_internal.h"

extern "C" {
void flexchart_bar_force_link(void);
void flexchart_line_force_link(void);
void flexchart_arc_force_link(void);
void flexchart_area_force_link(void);
void flexchart_point_force_link(void);
void flexchart_rect_force_link(void);
void flexchart_rule_force_link(void);
void flexchart_tick_force_link(void);
void flexchart_text_force_link(void);
void flexchart_pie_force_link(void);
void flexchart_radar_force_link(void);
void flexchart_boxplot_force_link(void);
void flexchart_errorbar_force_link(void);
void flexchart_trail_force_link(void);
void flexchart_errorband_force_link(void);
void flexchart_geoshape_force_link(void);
void flexchart_image_force_link(void);
}

namespace flex {
namespace chart {

MarkRendererRegistry& MarkRendererRegistry::instance() {
    static MarkRendererRegistry r;
    return r;
}

void MarkRendererRegistry::register_renderer(const std::string& type, Creator c) {
    registry_[type] = std::move(c);
}

std::unique_ptr<MarkRenderer> MarkRendererRegistry::create(const std::string& type) const {
    auto it = registry_.find(type);
    return it != registry_.end() ? it->second() : nullptr;
}

void ensure_builtin_mark_renderers_linked() {
    // These references are intentionally explicit: they make each renderer
    // object reachable when flexchart is consumed as a static library.
    flexchart_bar_force_link();
    flexchart_line_force_link();
    flexchart_arc_force_link();
    flexchart_area_force_link();
    flexchart_point_force_link();
    flexchart_rect_force_link();
    flexchart_rule_force_link();
    flexchart_tick_force_link();
    flexchart_text_force_link();
    flexchart_pie_force_link();
    flexchart_radar_force_link();
    flexchart_boxplot_force_link();
    flexchart_errorbar_force_link();
    flexchart_trail_force_link();
    flexchart_errorband_force_link();
    flexchart_geoshape_force_link();
    flexchart_image_force_link();
}

} // namespace chart
} // namespace flex
