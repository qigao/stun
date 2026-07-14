#pragma once

#include "dotgraph_renderer.h"

#include <string>
#include <string_view>

namespace flexUI {
class Box;
class Element;
}

namespace dotgraph {

/**
 * Options shared by DOT parsing/layout and the flexUI semantic view.
 *
 * Negative routing tuning values preserve libavoid's defaults.
 */
struct FlexUiDotGraphOptions {
    float width = 500.0f;
    float height = 500.0f;
    float font_size = 13.0f;
    std::string font_family = "Segoe UI";
    float node_pad_x = 24.0f;
    float node_pad_y = 14.0f;
    DotGraphRoutingMode routing_mode = DG_ROUTE_ORTHOGONAL;
    float routing_shape_buffer = -1.0f;
    float routing_nudging_distance = -1.0f;
    float routing_segment_penalty = -1.0f;
    float routing_angle_penalty = -1.0f;
    float routing_crossing_penalty = -1.0f;
    int routing_nudge_orthogonal_ends = -1;
    int routing_nudge_shared_paths = -1;
};

struct FlexUiDotGraphResult {
    flexUI::Element* root = nullptr;
    std::string error;

    explicit operator bool() const noexcept { return root != nullptr; }
};

/**
 * Build a Box-owned semantic Element tree from an existing layout snapshot.
 *
 * The returned pointer is owned by `box`. The caller may append it to another
 * Box-owned Element or install it with Box::set_root(). Invalid dimensions
 * throw std::invalid_argument before any elements are allocated.
 */
flexUI::Element* create_flexui_dotgraph(
    flexUI::Box& box,
    const LayoutSnapshot& snapshot,
    const FlexUiDotGraphOptions& options = {});

/**
 * Parse, measure, lay out, and build a Box-owned semantic DOT view.
 *
 * Expected input/layout failures are returned in `error`; no partial tree is
 * exposed. The input is limited by DOTGRAPH_MAX_INPUT_BYTES.
 */
FlexUiDotGraphResult create_flexui_dotgraph(
    flexUI::Box& box,
    std::string_view source,
    const FlexUiDotGraphOptions& options = {});

} // namespace dotgraph
