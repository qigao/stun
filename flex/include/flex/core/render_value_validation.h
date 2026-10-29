/* Shared value-domain checks for renderer effects; these do not imply support. */
#pragma once

#include "flex/core/types.h"

#include <cmath>

namespace flex::render_values {

inline bool valid_shadow(const Shadow& shadow) noexcept {
    return std::isfinite(shadow.offset_x) &&
           std::isfinite(shadow.offset_y) &&
           std::isfinite(shadow.blur) &&
           std::isfinite(shadow.spread) &&
           std::isfinite(shadow.color.r) &&
           std::isfinite(shadow.color.g) &&
           std::isfinite(shadow.color.b) &&
           std::isfinite(shadow.color.a) && shadow.blur >= 0.0f;
}

inline bool valid_blur(const BlurFilter& blur) noexcept {
    return std::isfinite(blur.radius) && blur.radius >= 0.0f;
}

} // namespace flex::render_values
