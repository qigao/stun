#include "flexchart/flexchart.h"
#include "flexchart/chart_ast.h"
#include <cctype>
#include <string>

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

namespace flex {
namespace chart {

std::string detect_type(std::string_view src) {
    auto p = src.begin();
    while (p != src.end() && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    auto start = p;
    while (p != src.end() && std::isalpha(static_cast<unsigned char>(*p))) ++p;
    return std::string(start, p);
}

bool parse_chart(const char* source, AstProgram* program, std::string& error) {
    if (!source || !program) {
        error = "null source or program";
        return false;
    }

    std::string type = detect_type(source);
    std::shared_ptr<AstChart> chart;

    if      (type == "bar")       chart = bar_chart_parse_ast(source, error);
    else if (type == "line")      chart = line_chart_parse_ast(source, error);
    else if (type == "pie")       chart = pie_chart_parse_ast(source, error);
    else if (type == "arc")       chart = arc_chart_parse_ast(source, error);
    else if (type == "area")      chart = area_chart_parse_ast(source, error);
    else if (type == "point")     chart = point_chart_parse_ast(source, error);
    else if (type == "rect")      chart = rect_chart_parse_ast(source, error);
    else if (type == "rule")      chart = rule_chart_parse_ast(source, error);
    else if (type == "tick")      chart = tick_chart_parse_ast(source, error);
    else if (type == "text")      chart = text_chart_parse_ast(source, error);
    else if (type == "radar")     chart = radar_chart_parse_ast(source, error);
    else if (type == "boxplot")   chart = boxplot_chart_parse_ast(source, error);
    else if (type == "errorbar")  chart = errorbar_chart_parse_ast(source, error);
    else if (type == "trail")     chart = trail_chart_parse_ast(source, error);
    else if (type == "errorband") chart = errorband_chart_parse_ast(source, error);
    else if (type == "geoshape")  chart = geoshape_chart_parse_ast(source, error);
    else if (type == "image")     chart = image_chart_parse_ast(source, error);
    else {
        error = "Unknown chart type: " + type;
        return false;
    }

    if (!chart) return false;

    program->views.push_back(chart);
    return true;
}

} // namespace chart
} // namespace flex
