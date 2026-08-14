/*
 * Flex Animation - Keyframe sampling data shared by editable and compiled
 * animation representations.
 */
#pragma once

#include "flex/core/types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace flex {

enum class SpatialInterpolation : uint8_t {
    Linear,
    CatmullRom,
    CubicBezier,
};

struct Keyframe {
    float time = 0;
    AnimValue value;
    Easing easing = Easing::linear();

    struct SpatialTangents {
        Vec2 in;
        Vec2 out;
    };
    std::optional<SpatialTangents> spatial_tangents;

    Keyframe() = default;
    Keyframe(float t, float v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const std::string& v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const Color& v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const Vec2& v, Easing e = Easing::linear())
        : time(t), value(v), easing(e) {}
    Keyframe(float t, const Vec2& v, const Vec2& in_tangent,
             const Vec2& out_tangent, Easing e = Easing::linear())
        : time(t), value(v), easing(e),
          spatial_tangents(SpatialTangents{in_tangent, out_tangent}) {}
};

} // namespace flex
