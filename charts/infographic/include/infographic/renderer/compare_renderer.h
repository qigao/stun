#pragma once

#include <renderer/template_renderer.h>

namespace flex::modules::infographic {

/**
 * 比较类渲染器
 * 
 * 支持的布局变体：
 * - VsLayout: A vs B 对比
 * - SwotQuadrant: SWOT 四象限分析
 */
class CompareRendererBase : public TemplateRenderer {
protected:
    // 比较布局的通用参数
    static constexpr int SECTION_WIDTH = 350;
    static constexpr int SECTION_HEIGHT = 400;
};

class VsLayoutRenderer : public CompareRendererBase {
public:
    std::string name() const override { return "VsLayoutRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class SwotRenderer : public CompareRendererBase {
public:
    std::string name() const override { return "SwotRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

} // namespace flex::modules::infographic
