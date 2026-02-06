#include "layout/edge_router.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace flex::modules::flexmaid {

std::vector<AnchorPoint> EdgeRouter::get_anchors(const LayoutData::Bounds& b, ShapeType shape) {
    std::vector<AnchorPoint> anchors;
    float cx = b.x, cy = b.y, hw = b.width / 2, hh = b.height / 2;
    
    switch (shape) {
        case ShapeType::Rect:
            // 4 edge midpoints
            anchors.push_back({cx, cy - hh, 0, -1});  // top
            anchors.push_back({cx, cy + hh, 0, 1});   // bottom
            anchors.push_back({cx - hw, cy, -1, 0}); // left
            anchors.push_back({cx + hw, cy, 1, 0});  // right
            break;
            
        case ShapeType::Circle: {
            // 8 points around circle
            float r = std::min(hw, hh);
            for (int i = 0; i < 8; ++i) {
                float angle = i * 3.14159f / 4;
                float nx = std::cos(angle), ny = std::sin(angle);
                anchors.push_back({cx + r * nx, cy + r * ny, nx, ny});
            }
            break;
        }
        
        case ShapeType::Diamond:
            // 4 vertices
            anchors.push_back({cx, cy - hh, 0, -1});  // top
            anchors.push_back({cx, cy + hh, 0, 1});   // bottom
            anchors.push_back({cx - hw, cy, -1, 0}); // left
            anchors.push_back({cx + hw, cy, 1, 0});  // right
            break;
            
        case ShapeType::Cylinder:
            // top/bottom ellipse centers + left/right edges
            anchors.push_back({cx, cy - hh, 0, -1});  // top
            anchors.push_back({cx, cy + hh, 0, 1});   // bottom
            anchors.push_back({cx - hw, cy, -1, 0}); // left
            anchors.push_back({cx + hw, cy, 1, 0});  // right
            break;
    }
    return anchors;
}

AnchorPoint EdgeRouter::best_anchor(const std::vector<AnchorPoint>& anchors, float tx, float ty) {
    AnchorPoint best = anchors[0];
    float best_score = std::numeric_limits<float>::max();
    
    for (const auto& a : anchors) {
        float dx = tx - a.x, dy = ty - a.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        // Prefer anchors whose normal points toward target
        float dot = (dist > 0.1f) ? (dx * a.nx + dy * a.ny) / dist : 0;
        float score = dist - dot * 50;  // bonus for aligned normal
        if (score < best_score) {
            best_score = score;
            best = a;
        }
    }
    return best;
}

std::vector<std::pair<float, float>> EdgeRouter::route(
    const LayoutData::Bounds& from,
    const LayoutData::Bounds& to,
    ShapeType from_shape,
    ShapeType to_shape)
{
    auto from_anchors = get_anchors(from, from_shape);
    auto to_anchors = get_anchors(to, to_shape);
    
    // Find best anchor pair
    auto a1 = best_anchor(from_anchors, to.x, to.y);
    auto a2 = best_anchor(to_anchors, from.x, from.y);
    
    // Start/end at anchor points (marker extends outward from line endpoint)
    float x1 = a1.x;
    float y1 = a1.y;
    float x2 = a2.x;
    float y2 = a2.y;
    
    std::vector<std::pair<float, float>> points;
    points.push_back({x1, y1});
    
    // Orthogonal routing
    bool horizontal_first = std::abs(a1.nx) > std::abs(a1.ny);
    if (horizontal_first) {
        float mid_x = (x1 + x2) / 2;
        if (std::abs(y1 - y2) > 1.0f) {
            points.push_back({mid_x, y1});
            points.push_back({mid_x, y2});
        }
    } else {
        float mid_y = (y1 + y2) / 2;
        if (std::abs(x1 - x2) > 1.0f) {
            points.push_back({x1, mid_y});
            points.push_back({x2, mid_y});
        }
    }
    
    points.push_back({x2, y2});
    return points;
}

} // namespace flex::modules::flexmaid
