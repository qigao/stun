#include "dotgraph/flexui_dotgraph.h"

#include "dotgraph/dotgraph_parser_wrapper.h"
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

void set_length(flexUI::CssSize& size, float value) {
    size.kind = flexUI::CssSizeKind::Length;
    size.value = value;
    size.expression.clear();
}

void set_box_size(flexUI::Element& element, float width, float height) {
    auto& style = element.style_;
    set_length(style.width_size, width);
    set_length(style.height_size, height);
    style.width = width;
    style.height = height;
    style.width_is_percent = false;
    style.height_is_percent = false;
    element.set_layout_size(width, height);
}

void set_absolute_box(flexUI::Element& element, float x, float y,
                      float width, float height) {
    auto& style = element.style_;
    style.position = flexUI::Position::Absolute;
    style.left = x;
    style.top = y;
    style.right = NAN;
    style.bottom = NAN;
    set_box_size(element, width, height);
    element.set_position_mode(flex::PositionMode::Absolute);
    element.set_position_offsets(y, NAN, NAN, x);
    element.set_position(x, y);
}

void set_uniform_border(flexUI::ComputedStyle& style, const flex::Color& color,
                        float width, flexUI::BorderStyle border_style) {
    style.border_color = color;
    style.has_border_side_colors = true;
    for (size_t i = 0; i < 4; ++i) {
        style.border_colors[i] = color;
        style.border_width[i] = width;
        style.border_style[i] = border_style;
    }
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

flexUI::Element* create_label(flexUI::Box& box, const char* tag,
                              const std::string& text, float x, float y,
                              float width, float height,
                              const std::string& font_family, float font_size,
                              const flex::Color& color) {
    auto* label = box.create(tag);
    label->add_class("dot-label");
    label->set_attribute("data-slot", tag);
    label->add_utilities("absolute flex items-center justify-center text-sm");
    label->set_text(text);
    set_absolute_box(*label, x, y, width, height);
    label->style_.font_family = font_family;
    label->style_.font_size = font_size;
    label->style_.text_color = color;
    label->set_custom_property("--dot-label-color", css_color(color));
    label->style_.text_align = flexUI::TextAlign::Center;
    label->set_z_index(1);
    return label;
}

void validate_options(const FlexUiDotGraphOptions& options) {
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

} // namespace

flexUI::Element* create_flexui_dotgraph(
    flexUI::Box& box, const LayoutSnapshot& snapshot,
    const FlexUiDotGraphOptions& options) {
    validate_options(options);
    const ViewTransform transform = make_view_transform(snapshot, options);

    auto* root = box.create("dotgraph");
    root->add_class("dotgraph");
    root->set_attribute("data-slot", "dotgraph");
    root->set_attribute("role", "graphics-document");
    root->set_attribute("data-render-mode", "tree");
    root->add_utilities("relative overflow-hidden");
    root->style_.position = flexUI::Position::Relative;
    root->style_.overflow_x = flexUI::Overflow::Hidden;
    root->style_.overflow_y = flexUI::Overflow::Hidden;
    if (const auto background = snapshot.graph_extra.find("bgcolor");
        background != snapshot.graph_extra.end()) {
        root->set_attribute("data-dot-bgcolor", background->second);
        root->set_custom_property("--dot-background", background->second);
        root->style_.background_color = parse_dot_color(
            background->second, flex::Color::White);
    }
    set_box_size(*root, options.width, options.height);
    root->set_clip(true);

    for (const auto& cluster : snapshot.clusters) {
        const Point top_left = transform.map({cluster.x, cluster.y});
        const float width = static_cast<float>(cluster.width) * transform.scale;
        const float height = static_cast<float>(cluster.height) * transform.scale;
        auto* element = box.create(
            "dot-cluster", element_id("cluster-", cluster.id));
        element->add_class("dot-cluster");
        element->set_attribute("data-slot", "dotgraph-cluster");
        element->add_utility("absolute");
        element->set_attribute("data-cluster-id", cluster.id);
        element->set_attribute("data-dot-color", cluster.color);
        element->set_attribute("data-dot-style", cluster.style);
        element->set_custom_property("--dot-stroke", cluster.color);
        set_absolute_box(*element, static_cast<float>(top_left.x),
                         static_cast<float>(top_left.y), width, height);
        element->style_.background_color = flex::Color::Transparent;
        set_uniform_border(element->style_,
                           parse_dot_color(cluster.color,
                                           flex::Color(0.61f, 0.64f, 0.69f)),
                           kDefaultStrokeWidth,
                           resolve_border_style(cluster.style));
        element->set_z_index(0);
        if (!cluster.label.empty()) {
            const float label_height = options.font_size * transform.scale + 4.0f;
            auto* label = create_label(
                box, "dot-cluster-label", cluster.label, 0.0f, 0.0f, width,
                label_height, options.font_family,
                options.font_size * transform.scale,
                parse_dot_color(
                    cluster.extra.count("label_fontcolor")
                        ? cluster.extra.at("label_fontcolor")
                        : std::string("#374151"),
                    flex::Color(0.22f, 0.25f, 0.32f)));
            label->add_class("dot-cluster-label");
            element->add_child(label);
        }
        root->add_child(element);
    }

    for (const auto& edge : snapshot.edges) {
        if (edge.points.size() < 2) continue;
        std::vector<Point> mapped;
        mapped.reserve(edge.points.size());
        ContentBounds bounds;
        for (const auto& point : edge.points) {
            Point output = transform.map(point);
            mapped.push_back(output);
            bounds.include(output.x, output.y);
        }
        const float pen_width = std::max(
            parse_finite_float(edge.extra, "penwidth", kDefaultStrokeWidth) *
                transform.scale,
            0.5f);
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

        auto widget = std::make_unique<DotEdgeWidget>(mapped, edge.directed);
        auto* element = box.create_with_widget("dot-edge", std::move(widget));
        element->add_class("dot-edge");
        element->set_attribute("data-slot", "dotgraph-edge");
        element->add_utility("absolute");
        element->set_attribute("data-from", edge.from);
        element->set_attribute("data-to", edge.to);
        element->set_attribute("data-directed", edge.directed ? "true" : "false");
        element->set_attribute("data-dot-color", edge.color);
        element->set_attribute("data-dot-style", edge.style);
        element->set_custom_property("--dot-stroke", edge.color);
        set_absolute_box(*element, left, top, width, height);
        element->style_.background_color = flex::Color::Transparent;
        set_uniform_border(element->style_,
                           parse_dot_color(edge.color,
                                           flex::Color(0.29f, 0.33f, 0.39f)),
                           pen_width, flexUI::BorderStyle::Solid);
        element->set_z_index(1);
        const float opacity = parse_finite_float(edge.extra, "opacity", 1.0f);
        element->style_.opacity = std::clamp(opacity, 0.0f, 1.0f);

        if (!edge.label.empty()) {
            const Point first = mapped.front();
            const Point last = mapped.back();
            const float label_font_size = options.font_size * transform.scale;
            const float label_width =
                std::max(static_cast<float>(edge.label.size()) *
                             label_font_size * 0.6f + kEdgeLabelPadding,
                         label_font_size);
            const float label_height = label_font_size + kEdgeLabelHeightPadding;
            auto* label = create_label(
                box, "dot-edge-label", edge.label,
                static_cast<float>((first.x + last.x) * 0.5) - label_width * 0.5f,
                static_cast<float>((first.y + last.y) * 0.5) - label_height * 0.5f,
                label_width, label_height, options.font_family, label_font_size,
                parse_dot_color(
                    edge.extra.count("label_fontcolor")
                        ? edge.extra.at("label_fontcolor")
                        : edge.color,
                    flex::Color(0.29f, 0.33f, 0.39f)));
            label->add_class("dot-edge-label");
            label->style_.background_color = flex::Color::White;
            for (float& radius : label->style_.border_radius) radius = 3.0f;
            element->add_child(label);
        }
        root->add_child(element);
    }

    for (const auto& node : snapshot.nodes) {
        const Point top_left = transform.map({node.x, node.y});
        const float width = static_cast<float>(node.width) * transform.scale;
        const float height = static_cast<float>(node.height) * transform.scale;
        auto widget = std::make_unique<DotNodeWidget>(node.shape);
        auto* element = box.create_with_widget(
            "dot-node", std::move(widget), element_id("node-", node.id));
        element->add_class("dot-node");
        element->add_class(std::string("shape-") + shape_name(node.shape));
        element->set_attribute("data-slot", "dotgraph-node");
        element->add_utility("absolute");
        element->set_attribute("data-node-id", node.id);
        element->set_attribute("data-shape", shape_name(node.shape));
        element->set_attribute("data-dot-color", node.color);
        element->set_attribute("data-dot-fill", node.fillcolor);
        element->set_attribute("data-dot-font-color", node.fontcolor);
        element->set_attribute("data-dot-style", node.style);
        element->set_custom_property("--dot-fill", node.fillcolor);
        element->set_custom_property("--dot-stroke", node.color);
        element->set_custom_property("--dot-text", node.fontcolor);
        set_absolute_box(*element, static_cast<float>(top_left.x),
                         static_cast<float>(top_left.y), width, height);
        element->style_.background_color = parse_dot_color(
            node.fillcolor, flex::Color(0.91f, 0.91f, 1.0f));
        const float pen_width = std::max(
            parse_finite_float(node.extra, "penwidth", kDefaultStrokeWidth) *
                transform.scale,
            0.5f);
        set_uniform_border(element->style_,
                           parse_dot_color(node.color,
                                           flex::Color(0.29f, 0.33f, 0.39f)),
                           pen_width, resolve_border_style(node.style));
        const float radius = (node.shape == DG_SHAPE_ELLIPSE ||
                              node.shape == DG_SHAPE_CIRCLE ||
                              node.shape == DG_SHAPE_DOUBLECIRCLE)
                                 ? std::min(width, height) * 0.5f
                                 : 0.0f;
        for (float& corner : element->style_.border_radius) corner = radius;
        element->style_.opacity = std::clamp(
            parse_finite_float(node.extra, "opacity", 1.0f), 0.0f, 1.0f);
        element->set_z_index(2);

        if (!node.label.empty()) {
            auto* label = create_label(
                box, "dot-node-label", node.label, 0.0f, 0.0f, width, height,
                options.font_family, options.font_size * transform.scale,
                parse_dot_color(node.fontcolor,
                                flex::Color(0.12f, 0.16f, 0.22f)));
            label->add_class("dot-node-label");
            label->set_attribute("aria-hidden", "true");
            element->add_child(label);
        }
        root->add_child(element);
    }

    return root;
}

FlexUiDotGraphResult create_flexui_dotgraph(
    flexUI::Box& box, std::string_view source,
    const FlexUiDotGraphOptions& options) {
    try {
        validate_options(options);
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
