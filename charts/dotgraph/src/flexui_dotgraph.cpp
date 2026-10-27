#include "dotgraph/flexui_dotgraph.h"
#include "dotgraph/dotgraph_parser_wrapper.h"
#include "dotgraph_renderer.h"
#include "flexui_dotgraph_validation.h"
#include "flex/core/text.h"

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace dotgraph {

FlexUiDotGraphResult create_flexui_dotgraph(
    flexUI::Box& box, std::string_view source,
    const FlexUiDotGraphOptions& options) {
    try {
        detail::validate_options(options);
    } catch (const std::exception& error) {
        return {nullptr, error.what()};
    }
    if (source.size() > DOTGRAPH_MAX_INPUT_BYTES) {
        return {nullptr, "DOT input exceeds DOTGRAPH_MAX_INPUT_BYTES"};
    }
    const std::string terminated_source(source);
    std::unique_ptr<DotGraphDiagram, decltype(&dotgraph_diagram_free)> diagram(
        dotgraph_parse(terminated_source.c_str()), &dotgraph_diagram_free);
    if (!diagram) {
        return {nullptr, dotgraph_get_last_error()};
    }

    diagram->routing_mode = options.routing_mode;
    if (options.routing_shape_buffer >= 0.0f)
        diagram->routing_shape_buffer = options.routing_shape_buffer;
    if (options.routing_nudging_distance >= 0.0f)
        diagram->routing_nudging_distance = options.routing_nudging_distance;
    if (options.routing_segment_penalty >= 0.0f)
        diagram->routing_segment_penalty = options.routing_segment_penalty;
    if (options.routing_angle_penalty >= 0.0f)
        diagram->routing_angle_penalty = options.routing_angle_penalty;
    if (options.routing_crossing_penalty >= 0.0f)
        diagram->routing_crossing_penalty = options.routing_crossing_penalty;
    if (options.routing_nudge_orthogonal_ends >= 0)
        diagram->routing_nudge_orthogonal_ends =
            options.routing_nudge_orthogonal_ends;
    if (options.routing_nudge_shared_paths >= 0)
        diagram->routing_nudge_shared_paths = options.routing_nudge_shared_paths;

    for (DotGraphNode* node = diagram->nodes; node; node = node->next) {
        flex::Text text;
        text.set_content(node->label ? node->label : node->id);
        text.set_font_family(options.font_family);
        text.set_font_size(options.font_size);
        dotgraph_set_node_size(
            diagram.get(), node->id,
            text.measured_width() + options.node_pad_x,
            text.measured_height() + options.node_pad_y);
    }

    try {
        DotGraphRenderer renderer;
        LayoutSnapshot snapshot = renderer.layout(diagram.get());
        return {create_flexui_dotgraph(box, snapshot, options), {}};
    } catch (const std::exception& error) {
        return {nullptr, error.what()};
    }
}

} // namespace dotgraph
