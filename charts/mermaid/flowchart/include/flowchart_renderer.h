#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

// 引入 AST 定义
#include "flowchart/flowchart_ast.h"

namespace mermaid {
namespace flowchart {

/**
 * 渲染结果的中间表示
 */
struct Point {
    double x, y;
};

struct RenderedNode {
    std::string id;
    std::string text;
    double x, y, width, height;
    FlowchartNodeShape shape; // 增加形状信息
};

struct RenderedEdge {
    std::string source_id;
    std::string target_id;
    std::string label;
    std::vector<Point> points; // 布局引擎生成的路径点
    // Owned presentation values resolved by AST declaration ordinal, never by
    // source/target pair or a possibly reordered JSON projection.
    std::size_t declaration_index = 0;
    std::string stroke; // Empty uses RenderOptions::line_color.
    double stroke_width = -1.0; // Negative uses RenderOptions::line_width.
};

struct RenderOptions {
    std::string primary_color = "#e8e8ff";
    std::string background_color = "#f9f9f9";
    std::string line_color = "#333333";
    std::string text_color = "#000000";
    double line_width = 2.0;
    std::string font_family = "Inter, -apple-system, sans-serif";
    double font_size = 14.0;
};

struct LayoutSnapshot {
    std::vector<RenderedNode> nodes;
    std::vector<RenderedEdge> edges;
    double total_width = 0;
    double total_height = 0;
    RenderOptions options; // 视觉配置
};

/**
 * 流程图渲染引擎
 */
class FlowchartRenderer {
public:
    FlowchartRenderer();
    ~FlowchartRenderer();

    // 禁止拷贝，这是好习惯
    FlowchartRenderer(const FlowchartRenderer&) = delete;
    FlowchartRenderer& operator=(const FlowchartRenderer&) = delete;

    /**
     * 执行布局运算
     * @param diagram 已经解析好的图表数据
     * @throws std::invalid_argument for unsupported linkStyle properties or values.
     * @return 包含坐标和路径的快照
     */
    LayoutSnapshot layout(const FlowchartDiagram* diagram);

    /**
     * 将布局快照转换为 SVG 字符串
     * @param snapshot 布局结果
     * @return SVG 源代码
     */
    static std::string to_svg(const LayoutSnapshot& snapshot);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl; // PIMPL 模式：隐藏引擎实现细节
};

} // namespace flowchart
} // namespace mermaid
