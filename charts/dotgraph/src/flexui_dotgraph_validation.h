#pragma once

#include "dotgraph/flexui_dotgraph.h"

#include <cmath>
#include <stdexcept>

namespace dotgraph::detail {

inline void validate_options(const FlexUiDotGraphOptions& options) {
    if (!std::isfinite(options.width) || !std::isfinite(options.height) ||
        !std::isfinite(options.font_size) ||
        !std::isfinite(options.node_pad_x) ||
        !std::isfinite(options.node_pad_y) || options.width <= 0.0f ||
        options.height <= 0.0f || options.font_size <= 0.0f ||
        options.node_pad_x < 0.0f || options.node_pad_y < 0.0f) {
        throw std::invalid_argument(
            "DOT flexUI view requires finite positive dimensions/font size "
            "and non-negative node padding");
    }
}

}  // namespace dotgraph::detail
