#pragma once

#include <renderer/template_renderer.h>

namespace flex::modules::infographic {

class HierarchyTreeRenderer : public TemplateRenderer {
public:
    std::string name() const override { return "HierarchyTreeRenderer"; }
protected:
    void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) override;
};

} // namespace flex::modules::infographic
