#pragma once

#include "flexchart/chart_ast.h"
#include <string>
#include <string_view>
#include <memory>

namespace flex {
class Instance;
class Group;
}

namespace flex::modules::chart {

// 解析结果 - 与 flexmaid/infographic 统一
struct ParseResult {
    bool success = false;
    std::string error;
    int error_line = 0;
    int error_column = 0;
    std::shared_ptr<flex::chart::AstChart> chart;
    
    bool has_error() const { return !success; }
    std::string get_error() const { return error; }
};

// 主题 - 与其他模块统一
struct Theme {
    std::string background_color = "#ffffff";
    std::string primary_color = "#3498db";
    std::string secondary_color = "#2ecc71";
    std::string text_color = "#333333";
    std::string grid_color = "#e0e0e0";
    std::string font_family = "Arial, sans-serif";
    int font_size = 14;
    float line_width = 2.0f;
    std::vector<std::string> palette = {
        "#3498db", "#2ecc71", "#e74c3c", "#f39c12", "#9b59b6", "#1abc9c"
    };
    
    static Theme light();
    static Theme dark();
    static Theme modern();
};

// 统一的 FlexChart API
class FlexChart {
public:
    FlexChart();
    ~FlexChart();
    
    // 解析
    ParseResult parse(std::string_view source);
    
    // SVG 渲染
    std::string to_svg(const flex::chart::AstChart& chart);
    std::string to_svg(std::string_view source);
    
    // Flex 运行时渲染
    flex::Group* to_flex(const flex::chart::AstChart& chart, flex::Instance& instance);
    flex::Group* to_flex(std::string_view source, flex::Instance& instance);
    
    // 主题
    void set_theme(const Theme& theme);
    const Theme& get_theme() const;

private:
    Theme theme_;
};

// 组件注册 - 与其他模块统一
class ChartComponent {
public:
    static void register_component();
    [[deprecated("use create_flexui_chart for interactive UI")]]
    static flex::Group* build(const flex::chart::AstChart& chart, flex::Instance& instance);
};

} // namespace flex::modules::chart
