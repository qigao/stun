#pragma once

#include <cstdint>

namespace flexUI::detail {

// Shared semantic IDs. Keep the namespace, underlying type, and existing
// values stable when adding properties; the registry owns their behavior.
enum class StylePropertyId : std::uint16_t {
    Unknown = 0,
    Opacity,
    BackgroundColor,
    TextColor,
    BorderColor,
    FontSize,
    TransformX,
    TransformY,
    TransformScale,
    TransformScaleX,
    TransformScaleY,
    TransformRotate,
    OutlineWidth,
    OutlineOffset,
    OutlineColor,
    RingWidth,
    RingOffset,
    RingColor,
    RingOffsetColor,
    BoxShadowOffsetX,
    BoxShadowOffsetY,
    BoxShadowBlur,
    BoxShadowSpread,
    BoxShadowColor,
    Visibility,
};

} // namespace flexUI::detail
