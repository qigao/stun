#include "flex/runtime/component.h"
#include "flex/core/svg.h"
#include "dotgraph/flex_dotgraph.h"
#include "dotgraph_renderer.h"
#include "dotgraph/dotgraph_parser_wrapper.h"
#include "flex/runtime/text.h"
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <list>
#include <memory>

namespace flex {

namespace {

constexpr size_t kSvgCacheCapacity = 64;

struct SvgCache {
    std::unordered_map<std::string, std::string> entries;
    std::list<std::string> order;
};

std::shared_ptr<Svg> make_svg_node(const std::string& data, float width, float height) {
    auto node = Svg::create();
    node->set_data(data);
    node->set_layout_size(width, height);
    return node;
}

} // namespace

void register_dotgraph_component() {
    register_component("dotgraph", [](const Props& props) -> std::shared_ptr<Node> {
        std::string code = get_prop_string(props, "code", "digraph { A -> B }");
        float w = get_prop_float(props, "width", 500.0f);
        float h = get_prop_float(props, "height", 500.0f);

        float font_size = get_prop_float(props, "fontSize", 13.0f);
        std::string font_family = get_prop_string(props, "fontFamily", "Segoe UI");
        float pad_x = get_prop_float(props, "nodePadX", 24.0f);
        float pad_y = get_prop_float(props, "nodePadY", 14.0f);
        std::string routing_mode = get_prop_string(props, "routingMode", "orthogonal");

        if (w <= 0.0f || h <= 0.0f || font_size <= 0.0f ||
            pad_x < 0.0f || pad_y < 0.0f) {
            std::cerr << "DotGraph component requires positive dimensions and font size"
                      << " with non-negative node padding\n";
            return nullptr;
        }
        if (routing_mode != "orthogonal" && routing_mode != "polyline") {
            std::cerr << "DotGraph component has invalid routingMode: "
                      << routing_mode << "\n";
            return nullptr;
        }
        if (code.size() > DOTGRAPH_MAX_INPUT_BYTES) {
            std::cerr << "DotGraph component input exceeds "
                      << DOTGRAPH_MAX_INPUT_BYTES << " bytes\n";
            return nullptr;
        }

        float routing_shape_buffer = props.find("routingShapeBuffer") != props.end()
            ? get_prop_float(props, "routingShapeBuffer", -1.0f) : -1.0f;
        float routing_nudging_distance = props.find("routingNudgingDistance") != props.end()
            ? get_prop_float(props, "routingNudgingDistance", -1.0f) : -1.0f;
        float routing_segment_penalty = props.find("routingSegmentPenalty") != props.end()
            ? get_prop_float(props, "routingSegmentPenalty", -1.0f) : -1.0f;
        float routing_angle_penalty = props.find("routingAnglePenalty") != props.end()
            ? get_prop_float(props, "routingAnglePenalty", -1.0f) : -1.0f;
        float routing_crossing_penalty = props.find("routingCrossingPenalty") != props.end()
            ? get_prop_float(props, "routingCrossingPenalty", -1.0f) : -1.0f;
        int routing_nudge_orthogonal_ends = props.find("routingNudgeOrthogonalEnds") != props.end()
            ? (get_prop_bool(props, "routingNudgeOrthogonalEnds", false) ? 1 : 0) : -1;
        int routing_nudge_shared_paths = props.find("routingNudgeSharedPaths") != props.end()
            ? (get_prop_bool(props, "routingNudgeSharedPaths", true) ? 1 : 0) : -1;

        // Cache key
        std::ostringstream ks;
        ks << code << '\n' << w << ',' << h << '\n'
           << font_size << '\n' << font_family << '\n'
           << pad_x << ',' << pad_y << '\n' << routing_mode << '\n'
           << routing_shape_buffer << ',' << routing_nudging_distance << ','
           << routing_segment_penalty << ',' << routing_angle_penalty << ','
           << routing_crossing_penalty << ',' << routing_nudge_orthogonal_ends << ','
           << routing_nudge_shared_paths;
        const std::string cache_key = ks.str();

        static thread_local SvgCache cache;
        auto cached = cache.entries.find(cache_key);
        if (cached != cache.entries.end()) {
            cache.order.remove(cache_key);
            cache.order.push_front(cache_key);
            return make_svg_node(cached->second, w, h);
        }

        // Parse
        std::unique_ptr<DotGraphDiagram, decltype(&dotgraph_diagram_free)> diagram(
            dotgraph_parse(code.c_str()), &dotgraph_diagram_free);
        if (!diagram) {
            std::cerr << "DOT parse failed: " << dotgraph_get_last_error() << "\n";
            return nullptr;
        }

        if (routing_mode == "polyline")
            diagram->routing_mode = DG_ROUTE_POLYLINE;
        else
            diagram->routing_mode = DG_ROUTE_ORTHOGONAL;

        if (routing_shape_buffer >= 0.0f) diagram->routing_shape_buffer = routing_shape_buffer;
        if (routing_nudging_distance >= 0.0f) diagram->routing_nudging_distance = routing_nudging_distance;
        if (routing_segment_penalty >= 0.0f) diagram->routing_segment_penalty = routing_segment_penalty;
        if (routing_angle_penalty >= 0.0f) diagram->routing_angle_penalty = routing_angle_penalty;
        if (routing_crossing_penalty >= 0.0f) diagram->routing_crossing_penalty = routing_crossing_penalty;
        if (routing_nudge_orthogonal_ends >= 0) diagram->routing_nudge_orthogonal_ends = routing_nudge_orthogonal_ends;
        if (routing_nudge_shared_paths >= 0) diagram->routing_nudge_shared_paths = routing_nudge_shared_paths;

        // Measure node text
        for (DotGraphNode* n = diagram->nodes; n; n = n->next) {
            flex::Text t;
            t.set_content(n->label ? n->label : n->id);
            t.set_font_family(font_family);
            t.set_font_size(font_size);
            float tw = t.measured_width();
            float th = t.measured_height();
            dotgraph_set_node_size(diagram.get(), n->id, tw + pad_x, th + pad_y);
        }

        // Layout + render
        dotgraph::DotGraphRenderer renderer;
        std::string svg;
        try {
            auto snapshot = renderer.layout(diagram.get());
            svg = dotgraph::DotGraphRenderer::to_svg(snapshot);
        } catch (const std::exception& error) {
            std::cerr << "DOT layout failed: " << error.what() << "\n";
            return nullptr;
        }
        if (svg.empty()) {
            std::cerr << "DOT SVG generation failed\n";
            return nullptr;
        }
        if (cache.entries.size() >= kSvgCacheCapacity) {
            const std::string oldest = cache.order.back();
            cache.order.pop_back();
            cache.entries.erase(oldest);
        }
        cache.entries.emplace(cache_key, svg);
        cache.order.push_front(cache_key);

        return make_svg_node(svg, w, h);
    });
}

} // namespace flex
