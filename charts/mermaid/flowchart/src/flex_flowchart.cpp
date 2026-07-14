#include "flex/runtime/component.h"
#include "flex/runtime/svg.h"
#include "flowchart_renderer.h"
#include "flowchart/flowchart_parser_wrapper.h"
#include "flex/runtime/text.h"
#include <cmath>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <tlog.h>

namespace flex {

namespace {

constexpr size_t kMaxFlowchartSourceBytes = 1024 * 1024;
constexpr size_t kSvgCacheCapacity = 64;

class SvgCache {
public:
    bool get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = values_.find(key);
        if (it == values_.end()) return false;
        value = it->second;
        return true;
    }

    void put(std::string key, std::string value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.find(key) != values_.end()) return;
        if (order_.size() == kSvgCacheCapacity) {
            values_.erase(order_.front());
            order_.pop_front();
        }
        order_.push_back(key);
        values_.emplace(std::move(key), std::move(value));
    }

private:
    std::mutex mutex_;
    std::deque<std::string> order_;
    std::unordered_map<std::string, std::string> values_;
};

SvgCache& flowchart_svg_cache() {
    static SvgCache cache;
    return cache;
}

std::shared_ptr<Node> make_svg_node(const std::string& svg, float width, float height) {
    auto node = Svg::create();
    node->set_data(svg);
    node->set_width(width);
    node->set_height(height);
    node->set_layout_size(width, height);
    return node;
}

bool valid_nonnegative_option(float value) {
    return std::isfinite(value) && value >= 0.0f;
}

} // namespace

void register_flowchart_component() {
    register_component("flowchart", [](const Props& props) -> std::shared_ptr<Node> {
        std::string code = get_prop_string(props, "code", "graph TD\nA-->B");
        float w = get_prop_float(props, "width", 500.0f);
        float h = get_prop_float(props, "height", 500.0f);

        float font_size = get_prop_float(props, "fontSize", 14.0f);
        std::string font_family = get_prop_string(props, "fontFamily", "Segoe UI");
        float pad_x = get_prop_float(props, "nodePadX", 20.0f);
        float pad_y = get_prop_float(props, "nodePadY", 12.0f);
        std::string routing_mode = get_prop_string(props, "routingMode", "orthogonal");
        float routing_shape_buffer = props.find("routingShapeBuffer") != props.end()
                                          ? get_prop_float(props, "routingShapeBuffer", -1.0f)
                                          : -1.0f;
        float routing_nudging_distance = props.find("routingNudgingDistance") != props.end()
                                             ? get_prop_float(props, "routingNudgingDistance", -1.0f)
                                             : -1.0f;
        float routing_segment_penalty = props.find("routingSegmentPenalty") != props.end()
                                            ? get_prop_float(props, "routingSegmentPenalty", -1.0f)
                                            : -1.0f;
        float routing_angle_penalty = props.find("routingAnglePenalty") != props.end()
                                          ? get_prop_float(props, "routingAnglePenalty", -1.0f)
                                          : -1.0f;
        float routing_crossing_penalty = props.find("routingCrossingPenalty") != props.end()
                                             ? get_prop_float(props, "routingCrossingPenalty", -1.0f)
                                             : -1.0f;
        int routing_nudge_orthogonal_ends = props.find("routingNudgeOrthogonalEnds") != props.end()
                                                ? (get_prop_bool(props, "routingNudgeOrthogonalEnds", false) ? 1 : 0)
                                                : -1;
        int routing_nudge_shared_paths = props.find("routingNudgeSharedPaths") != props.end()
                                             ? (get_prop_bool(props, "routingNudgeSharedPaths", true) ? 1 : 0)
                                             : -1;

        if (code.empty() || code.size() > kMaxFlowchartSourceBytes ||
            !std::isfinite(w) || !std::isfinite(h) || w <= 0.0f || h <= 0.0f ||
            !std::isfinite(font_size) || font_size <= 0.0f ||
            !valid_nonnegative_option(pad_x) || !valid_nonnegative_option(pad_y) ||
            (routing_mode != "orthogonal" && routing_mode != "polyline") ||
            !std::isfinite(routing_shape_buffer) || !std::isfinite(routing_nudging_distance) ||
            !std::isfinite(routing_segment_penalty) || !std::isfinite(routing_angle_penalty) ||
            !std::isfinite(routing_crossing_penalty)) {
            TLOG_ERROR("Invalid flowchart component properties");
            return nullptr;
        }

        std::ostringstream key_stream;
        key_stream << code << '\n'
                   << w << ',' << h << '\n'
                   << font_size << '\n'
                   << font_family << '\n'
                   << pad_x << ',' << pad_y << '\n'
                   << routing_mode << '\n'
                   << routing_shape_buffer << ','
                   << routing_nudging_distance << ','
                   << routing_segment_penalty << ','
                   << routing_angle_penalty << ','
                   << routing_crossing_penalty << ','
                   << routing_nudge_orthogonal_ends << ','
                   << routing_nudge_shared_paths;
        const std::string cache_key = key_stream.str();

        std::string cached_svg;
        if (flowchart_svg_cache().get(cache_key, cached_svg)) {
            return make_svg_node(cached_svg, w, h);
        }

        // 1. 调用布局引擎
        mermaid::flowchart::FlowchartRenderer renderer;
        using DiagramPtr = std::unique_ptr<FlowchartDiagram, decltype(&flowchart_diagram_free)>;
        FlowchartDiagram* parsed_diagram = nullptr;
        const FlowchartParseStatus parse_status = flowchart_parse_ex(code.c_str(), &parsed_diagram);
        DiagramPtr diagram(parsed_diagram, &flowchart_diagram_free);
        if (parse_status != FLOWCHART_PARSE_COMPLETE) {
            TLOG_ERROR("Flowchart parse failed: {}", flowchart_get_last_error());
            return nullptr;
        }
        if (routing_mode == "polyline") {
            flowchart_set_routing_mode(diagram.get(), FC_ROUTE_POLYLINE);
        } else {
            flowchart_set_routing_mode(diagram.get(), FC_ROUTE_ORTHOGONAL);
        }
        if (routing_shape_buffer >= 0.0f) {
            diagram->routing_shape_buffer = routing_shape_buffer;
        }
        if (routing_nudging_distance >= 0.0f) {
            diagram->routing_nudging_distance = routing_nudging_distance;
        }
        if (routing_segment_penalty >= 0.0f) {
            diagram->routing_segment_penalty = routing_segment_penalty;
        }
        if (routing_angle_penalty >= 0.0f) {
            diagram->routing_angle_penalty = routing_angle_penalty;
        }
        if (routing_crossing_penalty >= 0.0f) {
            diagram->routing_crossing_penalty = routing_crossing_penalty;
        }
        if (routing_nudge_orthogonal_ends >= 0) {
            diagram->routing_nudge_orthogonal_ends = routing_nudge_orthogonal_ends;
        }
        if (routing_nudge_shared_paths >= 0) {
            diagram->routing_nudge_shared_paths = routing_nudge_shared_paths;
        }

        for (FlowchartNode* n = diagram->nodes; n; n = n->next) {
            flex::Text t;
            t.set_content(n->label ? n->label : n->id);
            t.set_font_family(font_family);
            t.set_font_size(font_size);
            float tw = t.measured_width();
            float th = t.measured_height();
            flowchart_set_node_size(diagram.get(), n->id, tw + pad_x, th + pad_y);
        }

        std::string svg;
        try {
            auto snapshot = renderer.layout(diagram.get());
            snapshot.options.font_family = font_family;
            snapshot.options.font_size = font_size;
            svg = mermaid::flowchart::FlowchartRenderer::to_svg(snapshot);
        } catch (const std::exception& error) {
            TLOG_ERROR("Flowchart rendering failed: {}", error.what());
            return nullptr;
        }
        if (svg.empty()) {
            TLOG_ERROR("Flowchart SVG renderer produced empty output");
            return nullptr;
        }
        flowchart_svg_cache().put(cache_key, svg);

        return make_svg_node(svg, w, h);
    });
}

} // namespace flex
