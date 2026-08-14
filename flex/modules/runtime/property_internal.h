#pragma once

#include "flex/core/types.h"

#include <cstdint>

namespace flex::detail {

enum class AnimatedPropertyValueKind : uint8_t {
    Scalar,
    String,
    Color,
    Vec2,
};

enum class PropertyTargetKind : uint8_t {
    Group = 1u << 0,
    Shape = 1u << 1,
    Text = 1u << 2,
    Image = 1u << 3,
    Svg = 1u << 4,
    Instance = 1u << 5,
    Solo = 1u << 6,
};

struct AnimatedPropertyDescriptor {
    PropertyID id = PropertyID::Unknown;
    AnimatedPropertyValueKind value_kind = AnimatedPropertyValueKind::Scalar;
    uint8_t target_mask = 0;
};

const AnimatedPropertyDescriptor* animated_property_descriptor(PropertyID id) noexcept;
bool animated_property_supports(PropertyID id, PropertyTargetKind target) noexcept;

} // namespace flex::detail
