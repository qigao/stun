#pragma once

#include <renderer/template_renderer.h>

namespace flex::modules::infographic {

class ListRendererBase : public TemplateRenderer {
protected:
    struct GridLayout {
        int cols, rows, start_x, start_y, cell_width, cell_height;
    };
    GridLayout calculate_grid(const UnifiedInfographic& infographic, int max_cols = 3) const;
};

class ListGridRenderer : public ListRendererBase {
public:
    std::string name() const override { return "ListGridRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class ListRowsRenderer : public ListRendererBase {
public:
    std::string name() const override { return "ListRowsRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class ListColumnRenderer : public ListRendererBase {
public:
    std::string name() const override { return "ListColumnRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

class ListCandyGridRenderer : public ListRendererBase {
public:
    std::string name() const override { return "ListCandyGridRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
    LayoutParams get_layout_params() const override;
};

} // namespace flex::modules::infographic
