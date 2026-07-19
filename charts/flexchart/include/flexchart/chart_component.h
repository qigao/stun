#pragma once

#include <flex/runtime/component.h>
#include <flex/runtime/group.h>
#include "flexchart/chart_ast.h"
#include <memory>
#include <string>

namespace flex {
    class Instance; // Forward declaration

namespace chart {

struct ChartComponentBuildOptions {
    bool show_title = true;
    bool show_legend = true;
};

/**
 * @brief Component that renders a chart based on the Chart DSL.
 */
class ChartComponent {
public:
    static void register_component();

    /**
     * @brief Builds a node tree from an AstChart.
     * 
     * @param chart The chart AST to render.
     * @param instance The flex instance (provides allocator and animation controller).
     * @return A Group node containing the chart representation.
     */
    static flex::Group* build(const std::shared_ptr<AstChart>& chart, flex::Instance& instance);
    static flex::Group* build(const std::shared_ptr<AstChart>& chart,
                              flex::Instance& instance,
                              const ChartComponentBuildOptions& options);
};

} // namespace chart
} // namespace flex
