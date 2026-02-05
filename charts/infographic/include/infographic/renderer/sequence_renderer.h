#pragma once

#include <renderer/template_renderer.h>

namespace flex::modules::infographic {

/**
 * 序列类渲染器基类
 * 
 * 支持的布局变体：
 * - Timeline: 水平时间线
 * - Funnel: 漏斗图
 * - Circular: 圆形流程
 * - VerticalRoadmap: 垂直路线图
 */
class SequenceRendererBase : public TemplateRenderer {
protected:
    // 序列通用的节点位置计算
    struct NodePosition {
        int x, y;
        double angle;  // 用于圆形布局
    };
    
    std::vector<NodePosition> calculate_positions(
        const UnifiedInfographic& infographic, 
        int center_x, int center_y, int radius) const;
};

class TimelineRenderer : public SequenceRendererBase {
public:
    std::string name() const override { return "TimelineRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class FunnelRenderer : public SequenceRendererBase {
public:
    std::string name() const override { return "FunnelRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
    void calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class CircularRenderer : public SequenceRendererBase {
public:
    std::string name() const override { return "CircularRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class VerticalRoadmapRenderer : public SequenceRendererBase {
public:
    std::string name() const override { return "VerticalRoadmapRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
    void calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

} // namespace flex::modules::infographic
