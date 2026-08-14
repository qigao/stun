#include "animation_sampling.h"

#include "flex/animation/numeric_expression.h"
#include "flex/core/cubic_bezier.h"

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace flex {
namespace {

// Upper-bound lookup: O(log keyframe_count) time and O(1) space. Times before
// or after the segment clamp to its first or last keyframe, respectively.
void find_keyframes(const Keyframe* keyframes, size_t keyframe_count,
                    float time, size_t* previous, size_t* next) {
    size_t left = 0;
    size_t right = keyframe_count;
    while (left < right) {
        const size_t middle = left + (right - left) / 2;
        if (keyframes[middle].time <= time) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    if (left == 0) {
        *previous = *next = 0;
    } else if (left >= keyframe_count) {
        *previous = *next = keyframe_count - 1;
    } else {
        *previous = left - 1;
        *next = left;
    }
}

} // namespace

AnimValue sample_keyframes(const Keyframe* keyframes, size_t keyframe_count,
                           animation::NumericExpression* numeric_expression,
                           SpatialInterpolation spatial_interpolation,
                           float time) {
    if (keyframe_count == 0) {
        return AnimValue(0.0f);
    }
    if (!keyframes) {
        throw std::invalid_argument(
            "animation keyframe segment is null with a non-zero size");
    }

    size_t previous_index = 0;
    size_t next_index = 0;
    find_keyframes(keyframes, keyframe_count, time, &previous_index,
                   &next_index);

    const Keyframe& previous = keyframes[previous_index];
    const Keyframe& next = keyframes[next_index];
    if (previous_index == next_index) {
        return previous.value;
    }

    const float duration = next.time - previous.time;
    float progress = duration > 0 ? (time - previous.time) / duration : 0;
    progress = previous.easing.evaluate(progress);

    if (std::holds_alternative<float>(previous.value) &&
        std::holds_alternative<float>(next.value)) {
        const float from = std::get<float>(previous.value);
        const float to = std::get<float>(next.value);
        if (numeric_expression) {
            return AnimValue(numeric_expression->sample(
                {time, progress, from, to}));
        }
        return AnimValue(from + (to - from) * progress);
    }

    if (std::holds_alternative<Vec2>(previous.value) &&
        std::holds_alternative<Vec2>(next.value)) {
        const Vec2& p1 = std::get<Vec2>(previous.value);
        const Vec2& p2 = std::get<Vec2>(next.value);
        if (spatial_interpolation == SpatialInterpolation::CatmullRom) {
            const Vec2& p0 =
                previous_index > 0 &&
                        std::holds_alternative<Vec2>(
                            keyframes[previous_index - 1].value)
                    ? std::get<Vec2>(keyframes[previous_index - 1].value)
                    : p1;
            const Vec2& p3 =
                next_index + 1 < keyframe_count &&
                        std::holds_alternative<Vec2>(
                            keyframes[next_index + 1].value)
                    ? std::get<Vec2>(keyframes[next_index + 1].value)
                    : p2;
            return AnimValue(animation_sampling::catmull_rom(
                p0, p1, p2, p3, progress));
        }
        if (spatial_interpolation == SpatialInterpolation::CubicBezier) {
            if (!previous.spatial_tangents || !next.spatial_tangents) {
                throw std::logic_error(
                    "cubicBezier tracks require explicit tangents on every keyframe");
            }
            const Vec2 control1 = p1 + previous.spatial_tangents->out;
            const Vec2 control2 = p2 + next.spatial_tangents->in;
            return AnimValue(
                CubicBezier2D(p1, control1, control2, p2).position(progress));
        }
        return AnimValue(animation_sampling::lerp_vec2(p1, p2, progress));
    }

    if (std::holds_alternative<Color>(previous.value) &&
        std::holds_alternative<Color>(next.value)) {
        return AnimValue(animation_sampling::lerp_color(
            std::get<Color>(previous.value), std::get<Color>(next.value),
            progress));
    }

    return next.value;
}

} // namespace flex
