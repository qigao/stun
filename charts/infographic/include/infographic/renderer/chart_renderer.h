#pragma once

#include <renderer/template_renderer.h>

namespace flex::modules::infographic {

/**
 * 图表类渲染器
 * 
 * 支持的布局变体：
 * - Pie: 饼图
 * - Donut: 环形图
 * - Bar: 条形图
 */
class ChartRendererBase : public TemplateRenderer {
protected:
    // 图表通用的数值计算
    double calculate_total(const UnifiedInfographic& infographic) const;
    
    // 图例渲染
    void render_legend(SvgContext& ctx, const UnifiedInfographic& infographic, 
                      int x, int y);
};

class PieChartRenderer : public ChartRendererBase {
public:
    std::string name() const override { return "PieChartRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class DonutChartRenderer : public ChartRendererBase {
public:
    std::string name() const override { return "DonutChartRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class BarChartRenderer : public ChartRendererBase {
public:
    std::string name() const override { return "BarChartRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

} // namespace flex::modules::infographic
