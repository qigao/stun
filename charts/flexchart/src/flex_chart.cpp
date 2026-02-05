#include "flexchart/flex_chart.h"
#include "flexchart/flexchart.h"
#include "flexchart/chart_component.h"
#include <flex.h>

namespace flex::modules::chart {

FlexChart::FlexChart() : theme_(Theme::light()) {}
FlexChart::~FlexChart() = default;

ParseResult FlexChart::parse(std::string_view source) {
    ParseResult result;
    
    flex::chart::AstProgram program;
    std::string error;
    
    if (!flex::chart::parse_chart(std::string(source).c_str(), &program, error)) {
        result.success = false;
        result.error = error;
        return result;
    }
    
    if (program.views.empty()) {
        result.success = false;
        result.error = "No chart view found";
        return result;
    }
    
    result.chart = std::dynamic_pointer_cast<flex::chart::AstChart>(program.views[0]);
    if (!result.chart) {
        result.success = false;
        result.error = "First view is not a chart";
        return result;
    }
    
    result.success = true;
    return result;
}

std::string FlexChart::to_svg(const flex::chart::AstChart& chart) {
    // TODO: 实现 SVG 渲染
    // 目前 flexchart 只支持 flex 运行时渲染
    return "<svg><text>SVG rendering not yet implemented for charts</text></svg>";
}

std::string FlexChart::to_svg(std::string_view source) {
    auto result = parse(source);
    if (!result.success) {
        return "<svg><text>Parse error: " + result.error + "</text></svg>";
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

// ChartComponent 委托到原有实现
void ChartComponent::register_component() {
    flex::chart::ChartComponent::register_component();
}

flex::Group* ChartComponent::build(const flex::chart::AstChart& chart, flex::Instance& instance) {
    return flex::chart::ChartComponent::build(
        std::make_shared<flex::chart::AstChart>(chart), instance);
}

} // namespace flex::modules::chart
