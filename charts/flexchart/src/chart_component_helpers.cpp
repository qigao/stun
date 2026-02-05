#include "chart_component_internal.h"
#include <cstdio>

namespace flex {
namespace chart {

std::string get_string_val(const AstValue& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<double>(v)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", std::get<double>(v));
        return buf;
    }
    return "";
}

double get_double_val(const AstValue& v) {
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<std::string>(v)) {
        try { return std::stod(std::get<std::string>(v)); } catch(...) {}
    }
    return 0.0;
}

std::shared_ptr<AstData> find_dataset(const std::shared_ptr<AstChart>& chart, const std::string& name) {
    for (const auto& ds : chart->datasets) {
        if (ds->name == name) return ds;
    }
    return nullptr;
}

std::vector<Record> get_records(const std::shared_ptr<AstData>& data) {
    std::vector<Record> records;
    if (!data) return records;
    // Treat all transforms/values as records
    for (const auto& t : data->transforms) {
        records.push_back({t->properties});
    }
    return records;
}

std::unique_ptr<MarkRenderer> MarkRendererFactory::create(const std::string& type) {
    if (type == "bar") return std::make_unique<BarMarkRenderer>();
    if (type == "line") return std::make_unique<LineMarkRenderer>();
    if (type == "pie") return std::make_unique<PieMarkRenderer>();
    if (type == "arc") return std::make_unique<ArcMarkRenderer>();
    if (type == "area") return std::make_unique<AreaMarkRenderer>();
    if (type == "point") return std::make_unique<PointMarkRenderer>();
    if (type == "rect") return std::make_unique<RectMarkRenderer>();
    if (type == "boxplot") return std::make_unique<BoxplotMarkRenderer>();
    if (type == "radar") return std::make_unique<RadarMarkRenderer>();
    if (type == "text") return std::make_unique<TextMarkRenderer>();
    if (type == "rule") return std::make_unique<RuleMarkRenderer>();
    if (type == "tick") return std::make_unique<TickMarkRenderer>();
    if (type == "errorbar") return std::make_unique<ErrorbarMarkRenderer>();
    return nullptr;
}

} // namespace chart
} // namespace flex
