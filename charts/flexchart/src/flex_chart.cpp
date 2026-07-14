#include "flexchart/flex_chart.h"
#include "flexchart/chart_component.h"
#include "flexchart/flexchart.h"
#include <flex.h>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <tlog.h>

// Per-mark C++ parser headers
#include "flexchart/bar/bar_parser.h"
#include "flexchart/line/line_parser.h"
#include "flexchart/pie/pie_parser.h"
#include "flexchart/arc/arc_parser.h"
#include "flexchart/area/area_parser.h"
#include "flexchart/point/point_parser.h"
#include "flexchart/rect/rect_parser.h"
#include "flexchart/rule/rule_parser.h"
#include "flexchart/tick/tick_parser.h"
#include "flexchart/text/text_parser.h"
#include "flexchart/radar/radar_parser.h"
#include "flexchart/boxplot/boxplot_parser.h"
#include "flexchart/errorbar/errorbar_parser.h"
#include "flexchart/trail/trail_parser.h"
#include "flexchart/errorband/errorband_parser.h"
#include "flexchart/geoshape/geoshape_parser.h"
#include "flexchart/image/image_parser.h"

namespace flex::modules::chart {

using ParseFn = std::shared_ptr<flex::chart::AstChart>(*)(const char*, std::string&);

static const std::pair<std::string, ParseFn> parsers[] = {
    {"bar",       flex::chart::bar_chart_parse_ast},
    {"line",      flex::chart::line_chart_parse_ast},
    {"pie",       flex::chart::pie_chart_parse_ast},
    {"arc",       flex::chart::arc_chart_parse_ast},
    {"area",      flex::chart::area_chart_parse_ast},
    {"point",     flex::chart::point_chart_parse_ast},
    {"rect",      flex::chart::rect_chart_parse_ast},
    {"rule",      flex::chart::rule_chart_parse_ast},
    {"tick",      flex::chart::tick_chart_parse_ast},
    {"text",      flex::chart::text_chart_parse_ast},
    {"radar",     flex::chart::radar_chart_parse_ast},
    {"boxplot",   flex::chart::boxplot_chart_parse_ast},
    {"errorbar",  flex::chart::errorbar_chart_parse_ast},
    {"trail",     flex::chart::trail_chart_parse_ast},
    {"errorband", flex::chart::errorband_chart_parse_ast},
    {"geoshape",  flex::chart::geoshape_chart_parse_ast},
    {"image",     flex::chart::image_chart_parse_ast},
};

FlexChart::FlexChart() : theme_(Theme::light()) {}
FlexChart::~FlexChart() = default;

ParseResult FlexChart::parse(std::string_view source) {
    ParseResult result;
    auto type = flex::chart::detect_type(source);
    std::string input_str(source);

    for (auto& [name, fn] : parsers) {
        if (type == name) {
            std::string parse_error;
            auto chart = fn(input_str.c_str(), parse_error);
            if (chart) {
                result.chart = chart;
                result.success = true;
            } else {
                result.success = false;
                result.error = parse_error;
            }
            return result;
        }
    }

    result.success = false;
    result.error = "Unknown chart type: " + type;
    return result;
}

std::string FlexChart::to_svg(const flex::chart::AstChart& chart) {
    (void)chart;
    throw std::logic_error("FlexChart SVG rendering is not implemented; use to_flex() with a renderer backend");
}

std::string FlexChart::to_svg(std::string_view source) {
    auto result = parse(source);
    if (!result.success) {
        throw std::invalid_argument("FlexChart parse error: " + result.error);
    }
    return to_svg(*result.chart);
}

flex::Group* FlexChart::to_flex(const flex::chart::AstChart& chart, flex::Instance& instance) {
    return flex::chart::ChartComponent::build(
        std::make_shared<flex::chart::AstChart>(chart), instance);
}

flex::Group* FlexChart::to_flex(std::string_view source, flex::Instance& instance) {
    auto result = parse(source);
    if (!result.success) return nullptr;
    return flex::chart::ChartComponent::build(result.chart, instance);
}

void FlexChart::set_theme(const Theme& theme) { theme_ = theme; }
const Theme& FlexChart::get_theme() const { return theme_; }

Theme Theme::light() {
    return Theme{};
}

Theme Theme::dark() {
    Theme t;
    t.background_color = "#1e1e1e";
    t.text_color = "#ffffff";
    t.grid_color = "#444444";
    t.palette = {"#4fc3f7", "#81c784", "#ff8a65", "#ffd54f", "#ba68c8", "#4dd0e1"};
    return t;
}

Theme Theme::modern() {
    Theme t;
    t.background_color = "#f8f9fa";
    t.primary_color = "#007bff";
    t.text_color = "#212529";
    t.font_family = "Inter, system-ui, sans-serif";
    return t;
}

void ChartComponent::register_component() {
    flex::chart::ChartComponent::register_component();
}

flex::Group* ChartComponent::build(const flex::chart::AstChart& chart, flex::Instance& instance) {
    return flex::chart::ChartComponent::build(
        std::make_shared<flex::chart::AstChart>(chart), instance);
}

} // namespace flex::modules::chart
