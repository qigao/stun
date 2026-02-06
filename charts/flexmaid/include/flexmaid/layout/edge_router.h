#pragma once

#include <vector>
#include <utility>
#include "render/render_types.h"

namespace flex::modules::flexmaid {

enum class ShapeType { Rect, Circle, Diamond, Cylinder };

struct AnchorPoint {
    float x, y;
    float nx, ny;  // normal direction (outward)
};

class EdgeRouter {
public:
    static std::vector<std::pair<float, float>> route(
        const LayoutData::Bounds& from,
        const LayoutData::Bounds& to,
        ShapeType from_shape = ShapeType::Rect,
        ShapeType to_shape = ShapeType::Rect
    );
    
private:
    static std::vector<AnchorPoint> get_anchors(const LayoutData::Bounds& b, ShapeType shape);
    static AnchorPoint best_anchor(const std::vector<AnchorPoint>& anchors, float target_x, float target_y);
};

} // namespace flex::modules::flexmaid
