#pragma once

#include "chart_renderer.h"

namespace flex::modules::flexmaid {

class ClassRenderer : public ChartRenderer {
protected:
    LayoutData do_layout(const UnifiedDiagram& diagram, const Theme& theme) override;
    MustacheNodeData build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme) override;
};

} // namespace flex::modules::flexmaid
