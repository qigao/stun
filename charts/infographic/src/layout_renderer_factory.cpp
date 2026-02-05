#include <layout_renderer.h>

namespace flex::modules::infographic {

std::unique_ptr<LayoutRenderer> LayoutRendererFactory::create(LayoutType type) {
    switch (type) {
        case LayoutType::Grid:     return std::make_unique<GridLayoutRenderer>();
        case LayoutType::Timeline: return std::make_unique<TimelineLayoutRenderer>();
        case LayoutType::Funnel:   return std::make_unique<FunnelLayoutRenderer>();
        case LayoutType::Pie:      return std::make_unique<PieLayoutRenderer>();
        case LayoutType::Bar:      return std::make_unique<BarLayoutRenderer>();
        case LayoutType::Quadrant: return std::make_unique<SwotLayoutRenderer>();
        case LayoutType::Tree:     return std::make_unique<TreeLayoutRenderer>();
        case LayoutType::Zigzag:   return std::make_unique<ZigzagLayoutRenderer>();
        case LayoutType::Circular: return std::make_unique<CircularLayoutRenderer>();
        case LayoutType::Row:      return std::make_unique<RoadmapLayoutRenderer>();
        case LayoutType::Column:   return std::make_unique<RoadmapLayoutRenderer>();
        default:                   return std::make_unique<GridLayoutRenderer>();
    }
}

} // namespace flex::modules::infographic
