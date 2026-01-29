/*
 * Flex Engine - Layout Data (On-demand allocation)
 *
 * Separated from Node to reduce memory footprint.
 * Only nodes participating in flex layout need this data.
 */

#pragma once

#include "flex/runtime/types.h"
#include <cmath>

namespace flex {

struct LayoutData {
    // Size
    float width = 0;
    float height = 0;
    bool width_is_percent = false;
    bool height_is_percent = false;

    // Flex properties
    float flex_grow = 0;
    float flex_shrink = 1;
    float flex_basis = 0;
    AlignSelf align_self = AlignSelf::Auto;

    // Position
    PositionMode position_mode = PositionMode::Static;
    float position_top = NAN;
    float position_right = NAN;
    float position_bottom = NAN;
    float position_left = NAN;
    bool position_absolute = false;

    // Box model
    float margin[4] = {0, 0, 0, 0};
    float border_width[4] = {0, 0, 0, 0};
    BoxSizing box_sizing = BoxSizing::ContentBox;
    int z_index = 0;

    // Anchor
    Anchor anchor = Anchor::TopLeft;
};

} // namespace flex
