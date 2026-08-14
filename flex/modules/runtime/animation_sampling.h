#pragma once

#include "flex/animation/keyframe.h"

#include <algorithm>
#include <cstddef>

namespace flex {
namespace animation {
class NumericExpression;
}

namespace animation_sampling {

inline Color lerp_color(const Color& from, const Color& to, float progress) {
    const float clamped =
        (std::max)(0.0f, (std::min)(1.0f, progress));
    return Color(from.r + (to.r - from.r) * clamped,
                 from.g + (to.g - from.g) * clamped,
                 from.b + (to.b - from.b) * clamped,
                 from.a + (to.a - from.a) * clamped);
}

inline Vec2 lerp_vec2(const Vec2& from, const Vec2& to, float progress) {
    const float clamped =
        (std::max)(0.0f, (std::min)(1.0f, progress));
    return {from.x + (to.x - from.x) * clamped,
            from.y + (to.y - from.y) * clamped};
}

inline Vec2 catmull_rom(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                        const Vec2& p3, float progress) {
    const float squared = progress * progress;
    const float cubed = squared * progress;
    return {
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * progress +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * squared +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * cubed),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * progress +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * squared +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * cubed),
    };
}

} // namespace animation_sampling

// Samples a contiguous immutable keyframe segment. The caller owns the
// segment and expression for the full duration of this call.
AnimValue sample_keyframes(const Keyframe* keyframes, size_t keyframe_count,
                           animation::NumericExpression* numeric_expression,
                           SpatialInterpolation spatial_interpolation,
                           float time);

} // namespace flex
