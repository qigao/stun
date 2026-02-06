#pragma once

#include "chart_renderer.h"

namespace flex::modules::flexmaid {

class BlockRenderer : public ChartRenderer {
protected:
    LayoutData do_layout(const UnifiedDiagram& diagram, const Theme& theme) override;
};

} // namespace flex::modules::flexmaid
