#include "dotgraph/flexui_dotgraph.h"

#include "dotgraph/dotgraph_parser_wrapper.h"
#include "flexui_dotgraph_validation.h"
#include <charts/ui/dotgraph_frame.h>
#include <flexUI/ui_document.h>
#include <flexUI/widget_registry.h>
#include "flex/core/text.h"
#include "flexUI/box.h"
#include "flexUI/computed_style.h"
#include "flexUI/element.h"
#include "flexUI/render_command.h"
#include "flexUI/widget.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dotgraph {
namespace {

constexpr float kGraphMargin = 50.0f;
constexpr float kDefaultStrokeWidth = 1.5f;
constexpr float kArrowLength = 9.0f;
constexpr float kArrowHalfAngleRadians = 0.52f;
constexpr float kEdgeLabelPadding = 12.0f;
constexpr float kEdgeLabelHeightPadding = 6.0f;

struct ViewTransform {
    float scale = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    Point map(const Point& point) const {
        return {point.x * scale + offset_x, point.y * scale + offset_y};
    }
};

struct ContentBounds {
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    bool has_content = false;

    void include(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        if (!has_content) {
            min_x = max_x = x;
            min_y = max_y = y;
            has_content = true;
            return;
        }
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
    }

    void include_box(double x, double y, double width, double height) {
        include(x, y);
        include(x + width, y + height);
    }
};

// All DOT geometry/paint is projected into the immutable typed UiDocument
// before any Box ownership; no live Element setters or hidden second tree.
flexUI::UiNodePresentation dot_bounds(float x, float y, float w, float h,
                                     bool absolute = true) {
    flexUI::UiNodePresentation p;
    p.has_bounds = true;
    p.absolute = absolute;
    p.relative = !absolute;
    p.x = x;
    p.y = y;
    p.width = w;
    p.height = h;
    return p;
}

void set_border(flexUI::UiNodePresentation& p,
                const flex::Color& color, float width,
                flexUI::BorderStyle style) {
    p.border_color = color;
    p.border_width = width;
    p.border_style = style;
}

flexUI::BorderStyle resolve_border_style(const std::string& style) {
    if (style.find("dashed") != std::string::npos) {
        return flexUI::BorderStyle::Dashed;
    }
    if (style.find("dotted") != std::string::npos) {
        return flexUI::BorderStyle::Dotted;
    }
    if (style.find("bold") != std::string::npos) {
        return flexUI::BorderStyle::Solid;
    }
    return flexUI::BorderStyle::Solid;
}

float parse_finite_float(const std::unordered_map<std::string, std::string>& values,
                         const char* key, float fallback) {
    const auto it = values.find(key);
    if (it == values.end()) return fallback;
    char* end = nullptr;
    const float value = std::strtof(it->second.c_str(), &end);
    if (end == it->second.c_str() || *end != '\0' || !std::isfinite(value)) {
        return fallback;
    }
    return value;
}

flex::Color parse_dot_color(const std::string& value,
                            const flex::Color& fallback) {
    if (!value.empty() && value.front() == '#') {
        return flex::Color::from_hex(value.c_str());
    }
    static const std::unordered_map<std::string, flex::Color> named = {
        {"black", flex::Color::Black},
        {"white", flex::Color::White},
        {"red", flex::Color::Red},
        {"green", flex::Color::Green},
        {"blue", flex::Color::Blue},
        {"yellow", flex::Color::Yellow},
        {"gray", flex::Color(0.5f, 0.5f, 0.5f)},
        {"grey", flex::Color(0.5f, 0.5f, 0.5f)},
        {"transparent", flex::Color::Transparent},
    };
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    const auto it = named.find(normalized);
    return it == named.end() ? fallback : it->second;
}

std::string css_color(const flex::Color& color) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) *
                                            255.0f));
    };
    std::ostringstream result;
    result << "rgba(" << channel(color.r) << ", " << channel(color.g) << ", "
           << channel(color.b) << ", " << std::clamp(color.a, 0.0f, 1.0f)
           << ')';
    return result.str();
}

const char* shape_name(DotGraphShape shape) {
    switch (shape) {
    case DG_SHAPE_BOX: return "box";
    case DG_SHAPE_CIRCLE: return "circle";
    case DG_SHAPE_DIAMOND: return "diamond";
    case DG_SHAPE_DOUBLECIRCLE: return "doublecircle";
    case DG_SHAPE_TRIANGLE: return "triangle";
    case DG_SHAPE_HEXAGON: return "hexagon";
    case DG_SHAPE_RECORD: return "record";
    case DG_SHAPE_PLAINTEXT: return "plaintext";
    case DG_SHAPE_ELLIPSE:
    default: return "ellipse";
    }
}

std::string element_id(const char* prefix, const std::string& source) {
    std::ostringstream result;
    result << prefix;
    for (unsigned char ch : source) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-') {
            result << static_cast<char>(ch);
        } else {
            result << '_' << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned int>(ch) << '_' << std::dec;
        }
    }
    return result.str();
}

std::string path_number(float value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(3);
    stream << value;
    std::string result = stream.str();
    while (result.size() > 1 && result.back() == '0') result.pop_back();
    if (!result.empty() && result.back() == '.') result.pop_back();
    return result;
}

std::string polygon_path(const std::vector<Point>& points) {
    if (points.empty()) return {};
    std::ostringstream path;
    path << "M " << path_number(static_cast<float>(points.front().x)) << ' '
         << path_number(static_cast<float>(points.front().y));
    for (size_t i = 1; i < points.size(); ++i) {
        path << " L " << path_number(static_cast<float>(points[i].x)) << ' '
             << path_number(static_cast<float>(points[i].y));
    }
    path << " Z";
    return path.str();
}

class DotNodeWidget final : public flexUI::Widget {
public:
    explicit DotNodeWidget(DotGraphShape shape) : shape_(shape) {}

    void emit_render_commands(const flexUI::Element& element,
                              flexUI::RenderCommandList& commands) override {
        const auto* style = element.computed_style;
        if (!style) return;
        const float width = element.width();
        const float height = element.height();
        if (width <= 0.0f || height <= 0.0f) return;

        const auto fill = flexUI::Paint::solid(style->background_color);
        const auto stroke = flexUI::Paint::solid(style->border_color);
        const float stroke_width = *std::max_element(
            std::begin(style->border_width), std::end(style->border_width));

        if (shape_ == DG_SHAPE_PLAINTEXT) return;
        if (shape_ == DG_SHAPE_BOX || shape_ == DG_SHAPE_RECORD) {
            commands.draw_rect(0.0f, 0.0f, width, height,
                               style->border_radius[0], fill, stroke,
                               stroke_width);
            return;
        }
        if (shape_ == DG_SHAPE_CIRCLE || shape_ == DG_SHAPE_DOUBLECIRCLE) {
            const float radius = std::min(width, height) * 0.5f;
            commands.draw_circle(width * 0.5f, height * 0.5f, radius, fill,
                                 stroke, stroke_width);
            if (shape_ == DG_SHAPE_DOUBLECIRCLE && radius > 4.0f) {
                commands.draw_circle(width * 0.5f, height * 0.5f, radius - 4.0f,
                                     flexUI::Paint::none(), stroke, stroke_width);
            }
            return;
        }
        if (shape_ == DG_SHAPE_ELLIPSE ||
            (shape_ != DG_SHAPE_DIAMOND && shape_ != DG_SHAPE_TRIANGLE &&
             shape_ != DG_SHAPE_HEXAGON)) {
            commands.draw_ellipse(width * 0.5f, height * 0.5f, width * 0.5f,
                                  height * 0.5f, fill, stroke, stroke_width);
            return;
        }

        std::vector<Point> points;
        if (shape_ == DG_SHAPE_DIAMOND) {
            points = {{width * 0.5, 0.0}, {width, height * 0.5},
                      {width * 0.5, height}, {0.0, height * 0.5}};
        } else if (shape_ == DG_SHAPE_TRIANGLE) {
            points = {{width * 0.5, 0.0}, {width, height}, {0.0, height}};
        } else {
            points = {{width * 0.25, 0.0}, {width * 0.75, 0.0},
                      {width, height * 0.5}, {width * 0.75, height},
                      {width * 0.25, height}, {0.0, height * 0.5}};
        }
        const std::string path = polygon_path(points);
        commands.fill_path(path, fill);
        commands.stroke_path(path, stroke, stroke_width);
    }

    bool needs_frame_update(const flexUI::Element&) const override { return false; }
    bool state_affects_paint(flexUI::Symbol) const override { return false; }
    bool paints_host_box() const override { return true; }
    const char* type_name() const override { return "DotNodeWidget"; }

private:
    DotGraphShape shape_;
};

class DotEdgeWidget final : public flexUI::Widget {
public:
    DotEdgeWidget(std::vector<Point> points, bool directed)
        : points_(std::move(points)), directed_(directed) {}

    void emit_render_commands(const flexUI::Element& element,
                              flexUI::RenderCommandList& commands) override {
        if (points_.size() < 2 || !element.computed_style) return;
        const auto* style = element.computed_style;
        const float stroke_width = std::max(
            *std::max_element(std::begin(style->border_width),
                              std::end(style->border_width)),
            0.5f);
        const auto stroke = flexUI::Paint::solid(style->border_color);

        std::ostringstream path;
        path << "M " << path_number(static_cast<float>(points_[0].x)) << ' '
             << path_number(static_cast<float>(points_[0].y));
        for (size_t i = 1; i < points_.size(); ++i) {
            path << " L " << path_number(static_cast<float>(points_[i].x)) << ' '
                 << path_number(static_cast<float>(points_[i].y));
        }
        commands.stroke_path(path.str(), stroke, stroke_width);

        if (!directed_) return;
        const Point tip = points_.back();
        size_t previous = points_.size() - 1;
        while (previous > 0) {
            --previous;
            const double dx = tip.x - points_[previous].x;
            const double dy = tip.y - points_[previous].y;
            if (dx * dx + dy * dy > 0.0001) break;
        }
        const double angle = std::atan2(tip.y - points_[previous].y,
                                        tip.x - points_[previous].x);
        const Point left{
            tip.x - kArrowLength * std::cos(angle - kArrowHalfAngleRadians),
            tip.y - kArrowLength * std::sin(angle - kArrowHalfAngleRadians)};
        const Point right{
            tip.x - kArrowLength * std::cos(angle + kArrowHalfAngleRadians),
            tip.y - kArrowLength * std::sin(angle + kArrowHalfAngleRadians)};
        const std::string arrow = polygon_path({tip, left, right});
        commands.fill_path(arrow, stroke);
    }

    bool needs_frame_update(const flexUI::Element&) const override { return false; }
    bool state_affects_paint(flexUI::Symbol) const override { return false; }
    bool paints_host_box() const override { return true; }
    const char* type_name() const override { return "DotEdgeWidget"; }

private:
    std::vector<Point> points_;
    bool directed_ = false;
};

ContentBounds snapshot_bounds(const LayoutSnapshot& snapshot) {
    ContentBounds bounds;
    for (const auto& node : snapshot.nodes) {
        bounds.include_box(node.x, node.y, node.width, node.height);
    }
    for (const auto& cluster : snapshot.clusters) {
        bounds.include_box(cluster.x, cluster.y, cluster.width, cluster.height);
    }
    for (const auto& edge : snapshot.edges) {
        for (const auto& point : edge.points) bounds.include(point.x, point.y);
    }
    return bounds;
}

ViewTransform make_view_transform(const LayoutSnapshot& snapshot,
                                  const FlexUiDotGraphOptions& options) {
    const ContentBounds bounds = snapshot_bounds(snapshot);
    if (!bounds.has_content) {
        return {1.0f, kGraphMargin, kGraphMargin};
    }
    const double content_width = std::max(bounds.max_x - bounds.min_x, 1.0);
    const double content_height = std::max(bounds.max_y - bounds.min_y, 1.0);
    const double natural_width = content_width + kGraphMargin * 2.0;
    const double natural_height = content_height + kGraphMargin * 2.0;
    const float scale = static_cast<float>(std::min(
        options.width / natural_width, options.height / natural_height));
    const float drawn_width = static_cast<float>(natural_width) * scale;
    const float drawn_height = static_cast<float>(natural_height) * scale;
    return {
        scale,
        (options.width - drawn_width) * 0.5f +
            static_cast<float>(kGraphMargin - bounds.min_x) * scale,
        (options.height - drawn_height) * 0.5f +
            static_cast<float>(kGraphMargin - bounds.min_y) * scale,
    };
}

flexUI::UiNodeDefinition make_label_definition(
    const char* tag, const std::string& text, float x, float y,
    float width, float height, const std::string& font_family,
    float font_size, const flex::Color& color) {
    flexUI::UiNodeDefinition result;
    result.tag = tag;
    result.properties["class"] = "dot-label " + std::string(tag);
    result.properties["attr.data-slot"] = std::string(tag);
    result.properties["utility"] = std::string(
        "absolute flex items-center justify-center text-sm");
    result.properties["text"] = text;
    // Preserve the native DOT label's CSS customization hook as well as its
    // typed font color; downstream theme rules may inspect this variable.
    result.properties["var.dot-label-color"] = css_color(color);
    auto p = dot_bounds(x, y, width, height);
    p.font_family = font_family;
    p.font_size = font_size;
    p.text_color = color;
    p.text_align = flexUI::TextAlign::Center;
    p.z_index = 1;
    result.presentation = std::move(p);
    return result;
}

struct DotEdgePrepared {
    std::vector<Point> points;
    bool directed = false;
};

struct DotWidgetCatalog {
    std::unordered_map<std::string, DotGraphShape> nodes;
    std::unordered_map<std::string, DotEdgePrepared> edges;
};

void require_registered(flexUI::WidgetRegistryError error) {
    if (error) {
        throw std::logic_error("DOT native UI registry: " + error.message);
    }
}

flexUI::WidgetRegistry dotgraph_registry(
    const std::shared_ptr<const DotWidgetCatalog>& catalog) {
    auto registry = flexUI::WidgetRegistry::builtins();
    require_registered(registry.register_element(
        "dotgraph", flexUI::UiContentModel::Children));
    require_registered(registry.register_element(
        "dot-cluster", flexUI::UiContentModel::Children));
    for (const auto* tag : {
             "dot-cluster-label", "dot-edge-label", "dot-node-label"}) {
        require_registered(registry.register_element(
            tag, flexUI::UiContentModel::Text));
    }
    require_registered(registry.register_widget(
        "dot-node", [catalog](const flexUI::UiNodeDefinition &node) {
            auto found = catalog->nodes.find(node.native_widget_key);
            if (found == catalog->nodes.end()) {
                throw std::invalid_argument("DOT native node shape missing");
            }
            return std::make_unique<DotNodeWidget>(found->second);
        }, flexUI::UiContentModel::Children));
    require_registered(registry.register_widget(
        "dot-edge", [catalog](const flexUI::UiNodeDefinition &node) {
            auto found = catalog->edges.find(node.native_widget_key);
            if (found == catalog->edges.end()) {
                throw std::invalid_argument("DOT native edge path missing");
            }
            return std::make_unique<DotEdgeWidget>(
                found->second.points, found->second.directed);
        }, flexUI::UiContentModel::Children));
    return registry;
}

} // namespace

flexUI::Element* create_flexui_dotgraph(
    flexUI::Box& box, const LayoutSnapshot& snapshot,
    const FlexUiDotGraphOptions& options) {
    detail::validate_options(options);
    const ViewTransform transform = make_view_transform(snapshot, options);
    auto definition = charts::ui::dotgraph_frame_definition();
    auto& root = definition.root;
    auto root_style = dot_bounds(0.0f, 0.0f, options.width, options.height, false);
    root_style.clipped = true;
    if (const auto background = snapshot.graph_extra.find("bgcolor");
        background != snapshot.graph_extra.end()) {
        root.properties["attr.data-dot-bgcolor"] = background->second;
        root.properties["var.dot-background"] = background->second;
        root_style.background_color = parse_dot_color(
            background->second, flex::Color::White);
    }
    root.presentation = std::move(root_style);

    // Registry factories receive only immutable native identifiers and
    // precomputed shapes/edge paths. Multiple graph instances never share
    // mutable layout/Widget maps or a second Element ownership tree.
    auto catalog = std::make_shared<DotWidgetCatalog>();
    root.children.reserve(snapshot.clusters.size() + snapshot.edges.size() +
                          snapshot.nodes.size());

    for (const auto& cluster : snapshot.clusters) {
        const Point top_left = transform.map({cluster.x, cluster.y});
        const float width = static_cast<float>(cluster.width) * transform.scale;
        const float height = static_cast<float>(cluster.height) * transform.scale;
        flexUI::UiNodeDefinition element;
        element.tag = "dot-cluster";
        element.id = element_id("cluster-", cluster.id);
        element.properties["class"] = std::string("dot-cluster");
        element.properties["utility"] = std::string("absolute");
        element.properties["attr.data-slot"] = std::string("dotgraph-cluster");
        element.properties["attr.data-cluster-id"] = cluster.id;
        element.properties["attr.data-dot-color"] = cluster.color;
        element.properties["attr.data-dot-style"] = cluster.style;
        element.properties["var.dot-stroke"] = cluster.color;
        auto p = dot_bounds(
            static_cast<float>(top_left.x), static_cast<float>(top_left.y),
            width, height);
        p.background_color = flex::Color::Transparent;
        set_border(p,
                   parse_dot_color(cluster.color,
                                   flex::Color(0.61f, 0.64f, 0.69f)),
                   kDefaultStrokeWidth, resolve_border_style(cluster.style));
        p.z_index = 0;
        element.presentation = std::move(p);
        if (!cluster.label.empty()) {
            const float label_height = options.font_size * transform.scale + 4.0f;
            auto label = make_label_definition(
                "dot-cluster-label", cluster.label, 0.0f, 0.0f, width,
                label_height, options.font_family,
                options.font_size * transform.scale,
                parse_dot_color(
                    cluster.extra.count("label_fontcolor")
                        ? cluster.extra.at("label_fontcolor")
                        : std::string("#374151"),
                    flex::Color(0.22f, 0.25f, 0.32f)));
            element.children.push_back(std::move(label));
        }
        root.children.push_back(std::move(element));
    }

    std::size_t edge_index = 0;
    for (const auto& edge : snapshot.edges) {
        if (edge.points.size() < 2) continue;
        // Bound the geometry before copying it into the native widget.
        if (edge.points.size() > 16384) {
            throw std::invalid_argument("DOT native edge has too many points");
        }
        std::vector<Point> mapped;
        mapped.reserve(edge.points.size());
        ContentBounds bounds;
        for (const auto& point : edge.points) {
            Point output = transform.map(point);
            if (!std::isfinite(output.x) || !std::isfinite(output.y)) {
                throw std::invalid_argument("DOT edge point is nonfinite");
            }
            mapped.push_back(output);
            bounds.include(output.x, output.y);
        }
        const float pen_width = std::max(
            parse_finite_float(edge.extra, "penwidth", kDefaultStrokeWidth) *
                transform.scale, 0.5f);
        const float padding = std::max(kArrowLength, pen_width * 2.0f);
        const float left = static_cast<float>(bounds.min_x) - padding;
        const float top = static_cast<float>(bounds.min_y) - padding;
        const float width = static_cast<float>(bounds.max_x - bounds.min_x) +
                            padding * 2.0f;
        const float height = static_cast<float>(bounds.max_y - bounds.min_y) +
                             padding * 2.0f;
        for (auto& point : mapped) {
            point.x -= left;
            point.y -= top;
        }
        const std::string key = "edge-" + std::to_string(edge_index++);
        catalog->edges.emplace(
            key, DotEdgePrepared{mapped, edge.directed});
        flexUI::UiNodeDefinition element;
        element.tag = "dot-edge";
        element.native_widget_key = key;
        element.properties["class"] = std::string("dot-edge");
        element.properties["utility"] = std::string("absolute");
        element.properties["attr.data-slot"] = std::string("dotgraph-edge");
        element.properties["attr.data-from"] = edge.from;
        element.properties["attr.data-to"] = edge.to;
        element.properties["attr.data-directed"] =
            std::string(edge.directed ? "true" : "false");
        element.properties["attr.data-dot-color"] = edge.color;
        element.properties["attr.data-dot-style"] = edge.style;
        element.properties["var.dot-stroke"] = edge.color;
        auto p = dot_bounds(left, top, width, height);
        p.background_color = flex::Color::Transparent;
        set_border(p, parse_dot_color(
                       edge.color, flex::Color(0.29f, 0.33f, 0.39f)),
                   pen_width, flexUI::BorderStyle::Solid);
        p.z_index = 1;
        p.opacity = std::clamp(
            parse_finite_float(edge.extra, "opacity", 1.0f), 0.0f, 1.0f);
        element.presentation = std::move(p);
        if (!edge.label.empty()) {
            const Point first = mapped.front();
            const Point last = mapped.back();
            const float label_font_size = options.font_size * transform.scale;
            const float label_width = std::max(
                static_cast<float>(edge.label.size()) *
                    label_font_size * 0.6f + kEdgeLabelPadding,
                label_font_size);
            const float label_height = label_font_size + kEdgeLabelHeightPadding;
            auto label = make_label_definition(
                "dot-edge-label", edge.label,
                static_cast<float>((first.x + last.x) * 0.5) -
                    label_width * 0.5f,
                static_cast<float>((first.y + last.y) * 0.5) -
                    label_height * 0.5f,
                label_width, label_height, options.font_family,
                label_font_size,
                parse_dot_color(
                    edge.extra.count("label_fontcolor")
                        ? edge.extra.at("label_fontcolor") : edge.color,
                    flex::Color(0.29f, 0.33f, 0.39f)));
            label.presentation->background_color = flex::Color::White;
            label.presentation->border_radius = 3.0f;
            element.children.push_back(std::move(label));
        }
        root.children.push_back(std::move(element));
    }

    std::size_t node_index = 0;
    for (const auto& node : snapshot.nodes) {
        const Point top_left = transform.map({node.x, node.y});
        const float width = static_cast<float>(node.width) * transform.scale;
        const float height = static_cast<float>(node.height) * transform.scale;
        const std::string key = "node-" + std::to_string(node_index++);
        catalog->nodes.emplace(key, node.shape);
        flexUI::UiNodeDefinition element;
        element.tag = "dot-node";
        element.id = element_id("node-", node.id);
        element.native_widget_key = key;
        element.properties["class"] =
            std::string("dot-node shape-") + shape_name(node.shape);
        element.properties["utility"] = std::string("absolute");
        element.properties["attr.data-slot"] = std::string("dotgraph-node");
        element.properties["attr.data-node-id"] = node.id;
        element.properties["attr.data-shape"] = std::string(shape_name(node.shape));
        element.properties["attr.data-dot-color"] = node.color;
        element.properties["attr.data-dot-fill"] = node.fillcolor;
        element.properties["attr.data-dot-font-color"] = node.fontcolor;
        element.properties["attr.data-dot-style"] = node.style;
        element.properties["var.dot-fill"] = node.fillcolor;
        element.properties["var.dot-stroke"] = node.color;
        element.properties["var.dot-text"] = node.fontcolor;
        auto p = dot_bounds(
            static_cast<float>(top_left.x), static_cast<float>(top_left.y),
            width, height);
        p.background_color = parse_dot_color(
            node.fillcolor, flex::Color(0.91f, 0.91f, 1.0f));
        const float pen_width = std::max(
            parse_finite_float(node.extra, "penwidth", kDefaultStrokeWidth) *
                transform.scale, 0.5f);
        set_border(p, parse_dot_color(
                       node.color, flex::Color(0.29f, 0.33f, 0.39f)),
                   pen_width, resolve_border_style(node.style));
        p.border_radius =
            (node.shape == DG_SHAPE_ELLIPSE || node.shape == DG_SHAPE_CIRCLE ||
             node.shape == DG_SHAPE_DOUBLECIRCLE)
                ? std::min(width, height) * 0.5f : 0.0f;
        p.opacity = std::clamp(
            parse_finite_float(node.extra, "opacity", 1.0f), 0.0f, 1.0f);
        p.z_index = 2;
        element.presentation = std::move(p);
        if (!node.label.empty()) {
            auto label = make_label_definition(
                "dot-node-label", node.label, 0.0f, 0.0f, width, height,
                options.font_family, options.font_size * transform.scale,
                parse_dot_color(node.fontcolor,
                                flex::Color(0.12f, 0.16f, 0.22f)));
            label.properties["attr.aria-hidden"] = std::string("true");
            element.children.push_back(std::move(label));
        }
        root.children.push_back(std::move(element));
    }

    // Whole-tree validation (limits, ids, typed style) and all registered
    // Widget factories happen before a single Box ownership commit.
    auto compiled = flexUI::compile_ui_definition(std::move(definition));
    if (!compiled) {
        throw std::invalid_argument(
            "DOT native UI definition rejected: " + compiled.error.message);
    }
    const auto registry = dotgraph_registry(catalog);
    flexUI::UiMountOptions mount;
    mount.mode = flexUI::UiMountMode::DetachedFragment;
    mount.id_policy = flexUI::UiIdPolicy::Global; // preserve #node-B selectors
    mount.required_slots = {"dotgraph"};
    auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry, mount);
    if (!installed) {
        throw std::invalid_argument(
            "DOT native UI assembly rejected: " + installed.error.message);
    }
    return installed.tree.root;
}


} // namespace dotgraph
