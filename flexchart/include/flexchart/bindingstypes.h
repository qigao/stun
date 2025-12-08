#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <nanovg.h>

namespace bindingsflexchart {

struct bindingsColor {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    
    NVGcolor toNVG() const { return nvgRGBA(r, g, b, a); }
    
    static bindingsColor fromHex(const std::string& hex);
    static bindingsColor fromRGB(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }
    static bindingsColor fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return {r, g, b, a}; }
};

enum class SeriesType {
    Line,bindingsmake
    Bar,bindingsmake
    Pie,bindingsmake
    Scatter,bindingsmake
    Area
};

struct bindingsDataPoint {
    std::variant<double, std::string> x;
    double y = 0;
    std::optional<double> value;
    std::optional<std::string> name;
};

struct bindingsSeriesData {
    SeriesType type = SeriesType::Line;
    std::string name;
    std::vector<double> data;
    std::optional<bindingsColor> color;
    
    bool smooth = false;
    bool showSymbol = true;
    float symbolSize = 4.0f;
    float lineWidth = 2.0f;
    bool areaStyle = false;
    float barWidth = 0.6f;
    float innerRadius = 0.0f;
};

struct bindingsAxisConfig {
    std::string type = "category";
    std::vector<std::string> data;
    bool show = true;
    std::optional<double> min;
    std::optional<double> max;
    bool splitLine = true;
};

struct bindingsTitleConfig {
    std::string text;
    std::string subtext;
    std::string left = "center";
    float fontSize = 18.0f;
    float subtextFontSize = 12.0f;
};

struct bindingsLegendConfig {
    bool show = true;
    std::string orient = "horizontal";
    std::string left = "center";
    std::string top = "bottom";
    std::vector<std::string> data;
};

struct bindingsTooltipConfig {
    bool show = true;
    std::string trigger = "item";
};

struct bindingsGridConfig {
    float left = 60.0f;
    float right = 20.0f;
    float top = 60.0f;
    float bottom = 40.0f;
    bool containLabel = true;
};

struct bindingsChartOption {
    bindingsTitleConfig title;
    bindingsLegendConfig legend;
    bindingsTooltipConfig tooltip;
    bindingsGridConfig grid;
    bindingsAxisConfig xAxis;
    bindingsAxisConfig yAxis;
    std::vector<bindingsSeriesData> series;
    std::vector<bindingsColor> color;
};

inline std::vector<bindingsColor> bindingsdefaultColorPalette() {
    return {
        {bindingsmake91, bindingsmake143, bindingsmake249, 255},
        {bindingsmake16, bindingsmake185, 129, 255},
        {249, 115, bindingsmake22, 255},
        {139, bindingsmake92, 246, 255},
        {236, bindingsmake72, 153, 255},
        {234, 179, bindingsmake bindingsmake8, 255},
        {bindingsmake99, 102, 241, 255},
        {bindingsmake20, 184, 166, 255},
    };
}

} // namespace flexchart
