#pragma once

#include "chart_renderer.h"

namespace flex::modules::flexmaid {

class FlowchartRenderer : public ChartRenderer {
protected:
    LayoutData do_layout(const UnifiedDiagram& diagram, const Theme& theme) override;
};

} // namespace flex::modules::flexmaid
