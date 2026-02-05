#include <flex/modules/flexmaid/flexmaid.h>
#include <flex/modules/flexmaid/mermaid_component.h>
#include <mustache/mustache.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <list>

namespace {

using namespace flex::modules::flexmaid;

// Mustache 渲染数据
struct MustacheEdgeData {
    std::string path_d;
    std::string label;
    bool has_label;
    float label_x, label_y;
    bool is_dotted, is_dashed, is_thick;
    std::string marker_start, marker_end;
};

struct MustacheSubgraphData {
    std::string label;
    float x, y, width, height;
    float text_x, text_y;
};

struct MustacheNodeData {
    std::string id, label;
    float x, y, width, height;
    bool is_sequence;
    float lifeline_y2;
    float rect_x, rect_y, radius, radius_inner;
    float rect_x_end, rect_y_end, rect_x_sub, rect_x_sub_end;
    std::string points_str;
    bool shape_rect, shape_round_rect, shape_circle, shape_diamond, shape_stadium;
    bool shape_cylinder, shape_subroutine, shape_parallelogram, shape_trapezoid;
    bool shape_note, shape_double_circle;
};

struct MustacheContext {
    float width, height;
    std::string background_color, primary_color, secondary_color;
    std::string text_color, line_color, font_family;
    float font_size, line_width, line_width_thick, label_font_size;
    std::vector<MustacheNodeData> nodes;
    std::vector<MustacheEdgeData> edges;
    std::vector<MustacheSubgraphData> subgraphs;
};

enum class MustacheNodeType {
    Root, NodesList, EdgesList, SubgraphsList,
    NodeItem, EdgeItem, SubgraphItem,
    NodeSection, EdgeSection, Value
};

struct MustacheProxy {
    MustacheNodeType type;
    const void* data;
    const MustacheContext* ctx;
    size_t index = 0;
};

const char* SVG_MUSTACHE_TEMPLATE = R"svg(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
  <rect width="100%" height="100%" fill="{{background_color}}"/>
  <defs>
    <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
      <polygon points="0 0, 10 3.5, 0 7" fill="{{line_color}}"/>
    </marker>
    <marker id="diamond" markerWidth="12" markerHeight="12" refX="6" refY="6" orient="auto">
      <polygon points="0 6, 6 0, 12 6, 6 12" fill="{{background_color}}" stroke="{{line_color}}"/>
    </marker>
    <marker id="diamond_filled" markerWidth="12" markerHeight="12" refX="6" refY="6" orient="auto">
      <polygon points="0 6, 6 0, 12 6, 6 12" fill="{{line_color}}"/>
    </marker>
    <marker id="circle" markerWidth="10" markerHeight="10" refX="5" refY="5" orient="auto">
      <circle cx="5" cy="5" r="4" fill="{{background_color}}" stroke="{{line_color}}"/>
    </marker>
  </defs>
  <g>
    {{#subgraphs}}
    <rect x="{{x}}" y="{{y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" fill-opacity="0.1" stroke="{{line_color}}" stroke-dasharray="5,5"/>
    <text x="{{text_x}}" y="{{text_y}}" font-family="{{font_family}}" font-size="12" fill="{{text_color}}">{{label}}</text>
    {{/subgraphs}}
    {{#edges}}
    <path d="{{path_d}}" fill="none" stroke="{{line_color}}" 
          stroke-width="{{#is_thick}}{{line_width_thick}}{{/is_thick}}{{^is_thick}}{{line_width}}{{/is_thick}}" 
          {{#is_dotted}}stroke-dasharray="2,2"{{/is_dotted}} 
          {{#is_dashed}}stroke-dasharray="5,5"{{/is_dashed}} 
          {{#marker_start}}marker-start="url(#{{marker_start}})"{{/marker_start}}
          {{#marker_end}}marker-end="url(#{{marker_end}})"{{/marker_end}}/>
    {{#has_label}}
    <text x="{{label_x}}" y="{{label_y}}" text-anchor="middle" font-family="{{font_family}}" font-size="{{label_font_size}}" fill="{{text_color}}">{{label}}</text>
    {{/has_label}}
    {{/edges}}
    {{#nodes}}
    {{#is_sequence}}<line x1="{{x}}" y1="{{y}}" x2="{{x}}" y2="{{lifeline_y2}}" stroke="{{line_color}}" stroke-dasharray="5,5"/>{{/is_sequence}}
    {{#shape_rect}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_rect}}
    {{#shape_round_rect}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" rx="10" ry="10" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_round_rect}}
    {{#shape_circle}}<circle cx="{{x}}" cy="{{y}}" r="{{radius}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_circle}}
    {{#shape_double_circle}}<circle cx="{{x}}" cy="{{y}}" r="{{radius}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/><circle cx="{{x}}" cy="{{y}}" r="{{radius_inner}}" fill="none" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_double_circle}}
    {{#shape_diamond}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_diamond}}
    {{#shape_stadium}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" rx="{{radius}}" ry="{{radius}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_stadium}}
    {{#shape_cylinder}}<ellipse cx="{{x}}" cy="{{rect_y}}" rx="{{radius}}" ry="10" fill="{{primary_color}}" stroke="{{line_color}}"/><rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}"/><line x1="{{rect_x}}" y1="{{rect_y}}" x2="{{rect_x}}" y2="{{rect_y_end}}" stroke="{{line_color}}"/><line x1="{{rect_x_end}}" y1="{{rect_y}}" x2="{{rect_x_end}}" y2="{{rect_y_end}}" stroke="{{line_color}}"/><ellipse cx="{{x}}" cy="{{rect_y_end}}" rx="{{radius}}" ry="10" fill="{{primary_color}}" stroke="{{line_color}}"/>{{/shape_cylinder}}
    {{#shape_subroutine}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/><line x1="{{rect_x_sub}}" y1="{{rect_y}}" x2="{{rect_x_sub}}" y2="{{rect_y_end}}" stroke="{{line_color}}"/><line x1="{{rect_x_sub_end}}" y1="{{rect_y}}" x2="{{rect_x_sub_end}}" y2="{{rect_y_end}}" stroke="{{line_color}}"/>{{/shape_subroutine}}
    {{#shape_parallelogram}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_parallelogram}}
    {{#shape_trapezoid}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_trapezoid}}
    {{#shape_note}}<path d="{{points_str}}" fill="#fff5ad" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_note}}
    <text x="{{x}}" y="{{y}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
    {{/nodes}}
  </g>
</svg>
)svg";

static int out_verbatim(const char* output, size_t size, void* data) {
    static_cast<std::ostringstream*>(data)->write(output, size);
    return 0;
}

static int out_escaped(const char* output, size_t size, void* data) {
    auto* ss = static_cast<std::ostringstream*>(data);
    for (size_t i = 0; i < size; i++) {
        switch (output[i]) {
            case '<': *ss << "&lt;"; break;
            case '>': *ss << "&gt;"; break;
            case '&': *ss << "&amp;"; break;
            case '"': *ss << "&quot;"; break;
            default: ss->put(output[i]);
        }
    }
    return 0;
}

struct MustacheProviderData {
    std::list<MustacheProxy> pool;
    MustacheProxy root;
};

static void* get_root(void* data) {
    return &static_cast<MustacheProviderData*>(data)->root;
}

static void* get_child_by_name(void* node, const char* name, size_t size, void* data) {
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* mpd = static_cast<MustacheProviderData*>(data);
    auto* pool = &mpd->pool;
    std::string key(name, size);
    const MustacheContext* ctx = proxy->ctx;

    MustacheNodeType effective_type = proxy->type;
    if (effective_type == MustacheNodeType::NodeSection) effective_type = MustacheNodeType::NodeItem;
    if (effective_type == MustacheNodeType::EdgeSection) effective_type = MustacheNodeType::EdgeItem;

    auto make_val = [&](const void* ptr, int type_idx) {
        pool->push_back({MustacheNodeType::Value, ptr, ctx, (size_t)type_idx});
        return (void*)&pool->back();
    };

    if (proxy->type == MustacheNodeType::Root) {
        if (key == "width") return make_val(&ctx->width, 1);
        if (key == "height") return make_val(&ctx->height, 1);
        if (key == "background_color") return make_val(ctx->background_color.c_str(), 0);
        if (key == "primary_color") return make_val(ctx->primary_color.c_str(), 0);
        if (key == "secondary_color") return make_val(ctx->secondary_color.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "font_size") return make_val(&ctx->font_size, 1);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
        if (key == "line_width_thick") return make_val(&ctx->line_width_thick, 1);
        if (key == "label_font_size") return make_val(&ctx->label_font_size, 1);
        if (key == "nodes") { pool->push_back({MustacheNodeType::NodesList, &ctx->nodes, ctx, 0}); return &pool->back(); }
        if (key == "edges") { pool->push_back({MustacheNodeType::EdgesList, &ctx->edges, ctx, 0}); return &pool->back(); }
        if (key == "subgraphs") { pool->push_back({MustacheNodeType::SubgraphsList, &ctx->subgraphs, ctx, 0}); return &pool->back(); }
    } else if (effective_type == MustacheNodeType::NodeItem) {
        auto& n = ctx->nodes[proxy->index];
        auto make_section = [&](bool cond) -> void* {
            if (!cond) return nullptr;
            pool->push_back({MustacheNodeType::NodeSection, proxy->data, ctx, proxy->index});
            return &pool->back();
        };
        if (key == "id") return make_val(n.id.c_str(), 0);
        if (key == "label") return make_val(n.label.c_str(), 0);
        if (key == "x") return make_val(&n.x, 1);
        if (key == "y") return make_val(&n.y, 1);
        if (key == "width") return make_val(&n.width, 1);
        if (key == "height") return make_val(&n.height, 1);
        if (key == "is_sequence") return make_section(n.is_sequence);
        if (key == "lifeline_y2") return make_val(&n.lifeline_y2, 1);
        if (key == "rect_x") return make_val(&n.rect_x, 1);
        if (key == "rect_y") return make_val(&n.rect_y, 1);
        if (key == "radius") return make_val(&n.radius, 1);
        if (key == "radius_inner") return make_val(&n.radius_inner, 1);
        if (key == "rect_x_end") return make_val(&n.rect_x_end, 1);
        if (key == "rect_y_end") return make_val(&n.rect_y_end, 1);
        if (key == "rect_x_sub") return make_val(&n.rect_x_sub, 1);
        if (key == "rect_x_sub_end") return make_val(&n.rect_x_sub_end, 1);
        if (key == "points_str") return make_val(n.points_str.c_str(), 0);
        if (key == "shape_rect") return make_section(n.shape_rect);
        if (key == "shape_round_rect") return make_section(n.shape_round_rect);
        if (key == "shape_circle") return make_section(n.shape_circle);
        if (key == "shape_diamond") return make_section(n.shape_diamond);
        if (key == "shape_stadium") return make_section(n.shape_stadium);
        if (key == "shape_cylinder") return make_section(n.shape_cylinder);
        if (key == "shape_subroutine") return make_section(n.shape_subroutine);
        if (key == "shape_parallelogram") return make_section(n.shape_parallelogram);
        if (key == "shape_trapezoid") return make_section(n.shape_trapezoid);
        if (key == "shape_note") return make_section(n.shape_note);
        if (key == "shape_double_circle") return make_section(n.shape_double_circle);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "font_size") return make_val(&ctx->font_size, 1);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "primary_color") return make_val(ctx->primary_color.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
    } else if (effective_type == MustacheNodeType::EdgeItem) {
        auto& e = ctx->edges[proxy->index];
        auto make_section = [&](bool cond) -> void* {
            if (!cond) return nullptr;
            pool->push_back({MustacheNodeType::EdgeSection, proxy->data, ctx, proxy->index});
            return &pool->back();
        };
        if (key == "path_d") return make_val(e.path_d.c_str(), 0);
        if (key == "label") return make_val(e.label.c_str(), 0);
        if (key == "has_label") return make_section(e.has_label);
        if (key == "label_x") return make_val(&e.label_x, 1);
        if (key == "label_y") return make_val(&e.label_y, 1);
        if (key == "is_dotted") return make_section(e.is_dotted);
        if (key == "is_dashed") return make_section(e.is_dashed);
        if (key == "is_thick") return make_section(e.is_thick);
        if (key == "marker_start") return e.marker_start.empty() ? nullptr : make_val(e.marker_start.c_str(), 0);
        if (key == "marker_end") return e.marker_end.empty() ? nullptr : make_val(e.marker_end.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
        if (key == "line_width_thick") return make_val(&ctx->line_width_thick, 1);
        if (key == "label_font_size") return make_val(&ctx->label_font_size, 1);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
    } else if (effective_type == MustacheNodeType::SubgraphItem) {
        auto& s = ctx->subgraphs[proxy->index];
        if (key == "label") return make_val(s.label.c_str(), 0);
        if (key == "x") return make_val(&s.x, 1);
        if (key == "y") return make_val(&s.y, 1);
        if (key == "width") return make_val(&s.width, 1);
        if (key == "height") return make_val(&s.height, 1);
        if (key == "text_x") return make_val(&s.text_x, 1);
        if (key == "text_y") return make_val(&s.text_y, 1);
        if (key == "primary_color") return make_val(ctx->primary_color.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
    }
    return nullptr;
}

static void* get_child_by_index(void* node, unsigned index, void* data) {
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* mpd = static_cast<MustacheProviderData*>(data);
    auto* pool = &mpd->pool;
    const MustacheContext* ctx = proxy->ctx;

    if (proxy->type == MustacheNodeType::NodesList && index < ctx->nodes.size()) {
        pool->push_back({MustacheNodeType::NodeItem, nullptr, ctx, index});
        return &pool->back();
    }
    if (proxy->type == MustacheNodeType::EdgesList && index < ctx->edges.size()) {
        pool->push_back({MustacheNodeType::EdgeItem, nullptr, ctx, index});
        return &pool->back();
    }
    if (proxy->type == MustacheNodeType::SubgraphsList && index < ctx->subgraphs.size()) {
        pool->push_back({MustacheNodeType::SubgraphItem, nullptr, ctx, index});
        return &pool->back();
    }
    if ((proxy->type == MustacheNodeType::NodeSection || proxy->type == MustacheNodeType::EdgeSection) && index == 0) {
        auto item_type = (proxy->type == MustacheNodeType::NodeSection) ? MustacheNodeType::NodeItem : MustacheNodeType::EdgeItem;
        pool->push_back({item_type, nullptr, ctx, proxy->index});
        return &pool->back();
    }
    return nullptr;
}

static int dump(void* node, int (*out)(const char*, size_t, void*), void* rdata, void*) {
    if (!node) return 0;
    auto* proxy = static_cast<MustacheProxy*>(node);
    if (proxy->type != MustacheNodeType::Value) return 0;
    
    if (proxy->index == 1) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", *static_cast<const float*>(proxy->data));
        return out(buf, strlen(buf), rdata);
    }
    const char* str = static_cast<const char*>(proxy->data);
    return out(str, strlen(str), rdata);
}

} // namespace

namespace flex::modules::flexmaid {

// 构造函数 - 注册默认布局器
FlexMaid::FlexMaid() : theme_(Theme::light()) {
    layouters_[DiagramType::Flowchart] = layout_flowchart;
    layouters_[DiagramType::Sequence] = layout_sequence;
    layouters_[DiagramType::Class] = layout_class;
    layouters_[DiagramType::Pie] = layout_pie;
    layouters_[DiagramType::GitGraph] = layout_gitgraph;
}

ParseResult FlexMaid::parse(const std::string& mermaid_text) {
    return parser_.parse(mermaid_text);
}

LayoutResult FlexMaid::layout(const UnifiedDiagram& diagram) {
    LayoutResult result;
    try {
        auto it = layouters_.find(diagram.type);
        if (it != layouters_.end()) {
            it->second(diagram, result.data, theme_);
        } else {
            layout_flowchart(diagram, result.data, theme_);
        }
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }
    return result;
}

void FlexMaid::register_layouter(DiagramType type, Layouter layouter) {
    layouters_[type] = std::move(layouter);
}

// 从 diagram + layout 构建渲染数据
static MustacheNodeData build_node_data(const Node& node, const LayoutData::Bounds& bounds, 
                                         const UnifiedDiagram& diagram, float canvas_height) {
    MustacheNodeData n{};
    n.id = node.id;
    n.label = node.label;
    n.x = bounds.x;
    n.y = bounds.y;
    n.width = bounds.width;
    n.height = bounds.height;
    n.is_sequence = diagram.is_sequence();
    n.lifeline_y2 = canvas_height - 20;
    
    n.rect_x = bounds.left();
    n.rect_y = bounds.top();
    n.radius = std::min(bounds.width, bounds.height) / 2;
    n.radius_inner = std::max(0.0f, n.radius - 5.0f);
    n.rect_x_end = bounds.right();
    n.rect_y_end = bounds.bottom();
    n.rect_x_sub = n.rect_x + 5;
    n.rect_x_sub_end = n.rect_x + bounds.width - 5;
    
    n.shape_rect = n.shape_round_rect = n.shape_circle = n.shape_diamond = false;
    n.shape_stadium = n.shape_cylinder = n.shape_subroutine = n.shape_parallelogram = false;
    n.shape_trapezoid = n.shape_note = n.shape_double_circle = false;

    if (diagram.is_state() && node.label == "[*]") {
        n.shape_circle = true;
    } else {
        switch (node.shape) {
            case NodeShape::Rectangle: case NodeShape::Class: case NodeShape::Entity:
                n.shape_rect = true; break;
            case NodeShape::RoundRect: case NodeShape::State:
                n.shape_round_rect = true; break;
            case NodeShape::Circle: case NodeShape::CircleFilled:
                n.shape_circle = true; break;
            case NodeShape::DoubleCircle:
                n.shape_double_circle = true; break;
            case NodeShape::Diamond:
                n.shape_diamond = true; break;
            case NodeShape::Stadium:
                n.shape_stadium = true; break;
            case NodeShape::Cylinder:
                n.shape_cylinder = true; break;
            case NodeShape::Subroutine:
                n.shape_subroutine = true; break;
            case NodeShape::Parallelogram: case NodeShape::ParallelogramAlt:
                n.shape_parallelogram = true; break;
            case NodeShape::Trapezoid: case NodeShape::TrapezoidAlt:
                n.shape_trapezoid = true; break;
            case NodeShape::Note:
                n.shape_note = true; break;
            default:
                n.shape_rect = true; break;
        }
    }

    std::ostringstream pts;
    if (n.shape_diamond) {
        pts << bounds.x << "," << n.rect_y << " "
            << n.rect_x_end << "," << bounds.y << " "
            << bounds.x << "," << n.rect_y_end << " "
            << n.rect_x << "," << bounds.y;
        n.points_str = pts.str();
    } else if (n.shape_parallelogram) {
        float skew = bounds.width * 0.2f;
        if (node.shape == NodeShape::ParallelogramAlt) skew = -skew;
        pts << (n.rect_x + skew) << "," << n.rect_y << " "
            << n.rect_x_end << "," << n.rect_y << " "
            << (n.rect_x_end - skew) << "," << n.rect_y_end << " "
            << n.rect_x << "," << n.rect_y_end;
        n.points_str = pts.str();
    } else if (n.shape_trapezoid) {
        float skew = bounds.width * 0.15f;
        bool alt = (node.shape == NodeShape::TrapezoidAlt);
        if (!alt) {
            pts << (n.rect_x + skew) << "," << n.rect_y << " "
                << (n.rect_x_end - skew) << "," << n.rect_y << " "
                << n.rect_x_end << "," << n.rect_y_end << " "
                << n.rect_x << "," << n.rect_y_end;
        } else {
            pts << n.rect_x << "," << n.rect_y << " "
                << n.rect_x_end << "," << n.rect_y << " "
                << (n.rect_x_end - skew) << "," << n.rect_y_end << " "
                << (n.rect_x + skew) << "," << n.rect_y_end;
        }
        n.points_str = pts.str();
    } else if (n.shape_note) {
        float c = 15;
        pts << "M " << n.rect_x << " " << n.rect_y 
            << " L " << (n.rect_x_end - c) << " " << n.rect_y 
            << " L " << n.rect_x_end << " " << (n.rect_y + c)
            << " L " << n.rect_x_end << " " << n.rect_y_end 
            << " L " << n.rect_x << " " << n.rect_y_end << " Z";
        n.points_str = pts.str();
    }
    return n;
}

static MustacheEdgeData build_edge_data(const Edge& edge, const LayoutData::Path& path) {
    MustacheEdgeData e{};
    e.label = edge.label;
    e.has_label = !edge.label.empty() && path.points.size() >= 2;
    if (e.has_label) {
        e.label_x = (path.points.front().first + path.points.back().first) / 2;
        e.label_y = (path.points.front().second + path.points.back().second) / 2;
    }
    e.is_dotted = (edge.style == EdgeStyle::Dotted);
    e.is_dashed = (edge.style == EdgeStyle::Dashed);
    e.is_thick = (edge.style == EdgeStyle::Thick);
    
    auto get_marker = [](EdgeDecoration dec) -> std::string {
        switch (dec) {
            case EdgeDecoration::Arrow: return "arrowhead";
            case EdgeDecoration::Diamond: return "diamond";
            case EdgeDecoration::DiamondFilled: return "diamond_filled";
            case EdgeDecoration::Circle: return "circle";
            default: return "";
        }
    };
    e.marker_start = get_marker(edge.start_decoration);
    e.marker_end = get_marker(edge.end_decoration);
    
    std::ostringstream path_d;
    if (!path.points.empty()) {
        path_d << "M " << path.points[0].first << " " << path.points[0].second;
        for (size_t i = 1; i < path.points.size(); ++i) {
            path_d << " L " << path.points[i].first << " " << path.points[i].second;
        }
    }
    e.path_d = path_d.str();
    return e;
}

std::string FlexMaid::render_svg(const UnifiedDiagram& diagram) {
    auto layout_result = layout(diagram);
    if (!layout_result.success) {
        return "<svg width=\"400\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\">"
               "<text x=\"10\" y=\"50\">Layout error: " + escape_xml(layout_result.error) + "</text></svg>";
    }
    return render_svg(diagram, layout_result.data);
}

std::string FlexMaid::render_svg(const UnifiedDiagram& diagram, const LayoutData& layout) {
    MustacheContext ctx;
    ctx.width = layout.width;
    ctx.height = layout.height;
    ctx.background_color = theme_.background_color;
    ctx.primary_color = theme_.primary_color;
    ctx.secondary_color = theme_.secondary_color;
    ctx.text_color = theme_.text_color;
    ctx.line_color = theme_.line_color;
    ctx.font_family = theme_.font_family;
    ctx.font_size = (float)theme_.font_size;
    ctx.line_width = theme_.line_width;
    ctx.line_width_thick = theme_.line_width * 2.0f;
    ctx.label_font_size = (float)theme_.font_size - 2.0f;

    for (const auto& node : diagram.nodes) {
        auto it = layout.node_bounds.find(node.id);
        if (it != layout.node_bounds.end()) {
            ctx.nodes.push_back(build_node_data(node, it->second, diagram, layout.height));
        }
    }

    for (size_t i = 0; i < diagram.edges.size() && i < layout.edge_paths.size(); ++i) {
        ctx.edges.push_back(build_edge_data(diagram.edges[i], layout.edge_paths[i]));
    }

    for (const auto& sub : diagram.subgraphs) {
        float min_x = 1e9, min_y = 1e9, max_x = -1e9, max_y = -1e9;
        bool has_nodes = false;
        for (const auto& node_id : sub.node_ids) {
            auto it = layout.node_bounds.find(node_id);
            if (it != layout.node_bounds.end()) {
                min_x = std::min(min_x, it->second.left());
                min_y = std::min(min_y, it->second.top());
                max_x = std::max(max_x, it->second.right());
                max_y = std::max(max_y, it->second.bottom());
                has_nodes = true;
            }
        }
        if (has_nodes) {
            MustacheSubgraphData s{};
            s.x = min_x - 20;
            s.y = min_y - 40;
            s.width = (max_x - min_x) + 40;
            s.height = (max_y - min_y) + 60;
            s.label = sub.label;
            s.text_x = s.x + 5;
            s.text_y = s.y + 15;
            ctx.subgraphs.push_back(s);
        }
    }

    std::ostringstream out;
    MustacheProviderData mpd;
    mpd.root = {MustacheNodeType::Root, &ctx, &ctx, 0};
    
    MUSTACHE_TEMPLATE* t = mustache_compile(SVG_MUSTACHE_TEMPLATE, strlen(SVG_MUSTACHE_TEMPLATE), nullptr, nullptr, 0);
    if (!t) return "<svg><text>Mustache compile error</text></svg>";
    
    MUSTACHE_RENDERER renderer = { out_verbatim, out_escaped };
    MUSTACHE_DATAPROVIDER provider = { dump, get_root, get_child_by_name, get_child_by_index, nullptr };
    
    mustache_process(t, &renderer, &out, &provider, &mpd);
    mustache_release(t);
    
    return out.str();
}

std::string FlexMaid::mermaid_to_svg(const std::string& mermaid_text) {
    auto parse_result = parse(mermaid_text);
    if (!parse_result.success) {
        return "<svg width=\"400\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\">"
               "<text x=\"10\" y=\"50\">Parse error: " + escape_xml(parse_result.get_error()) + "</text></svg>";
    }
    return render_svg(*parse_result.diagram);
}

flex::Group* FlexMaid::to_flex(const UnifiedDiagram& diagram, flex::Instance& instance) {
    return MermaidComponent::build(diagram, instance);
}

flex::Group* FlexMaid::to_flex(std::string_view source, flex::Instance& instance) {
    auto result = parse(std::string(source));
    if (!result.success) return nullptr;
    return MermaidComponent::build(*result.diagram, instance);
}

void FlexMaid::set_theme(const Theme& theme) { theme_ = theme; }
const Theme& FlexMaid::get_theme() const { return theme_; }

Theme Theme::light() {
    return {"#ffffff", "#0066cc", "#666666", "#333333", "#333333", "Arial, sans-serif", 14, 2.0f};
}

Theme Theme::dark() {
    return {"#1e1e1e", "#4fc3f7", "#cccccc", "#ffffff", "#cccccc", "Arial, sans-serif", 14, 2.0f};
}

Theme Theme::modern() {
    return {"#f8f9fa", "#007bff", "#6c757d", "#212529", "#495057", "Inter, system-ui, sans-serif", 14, 2.0f};
}

// 布局算法实现
void FlexMaid::layout_flowchart(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    std::string direction = diagram.get_prop("direction", "TD");
    bool is_horizontal = (direction == "LR");
    const float SPACING_X = 200, SPACING_Y = 120;
    
    auto calc_size = [&](const std::string& label, NodeShape shape) -> std::pair<float, float> {
        float tw = label.length() * theme.font_size * 0.6f;
        float th = theme.font_size * 1.2f;
        float w = std::max(80.0f, tw + 20);
        float h = std::max(40.0f, th + 20);
        if (shape == NodeShape::Circle) { float r = std::max(w, h) / 2; w = h = r * 2; }
        return {w, h};
    };

    int index = 0;
    for (const auto& node : diagram.nodes) {
        auto [w, h] = calc_size(node.label, node.shape);
        float x = (index % 3) * SPACING_X + SPACING_X / 2;
        float y = (index / 3) * SPACING_Y + SPACING_Y / 2;
        if (is_horizontal) std::swap(x, y);
        data.node_bounds[node.id] = {x, y, w, h};
        index++;
    }

    float max_x = 0, max_y = 0;
    for (const auto& [id, b] : data.node_bounds) {
        max_x = std::max(max_x, b.right());
        max_y = std::max(max_y, b.bottom());
    }
    data.width = max_x + 50;
    data.height = max_y + 50;

    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, from_it->second.y}, {to_it->second.x, to_it->second.y}};
        }
        data.edge_paths.push_back(path);
    }
}

void FlexMaid::layout_sequence(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    const float SPACING_X = 200, SPACING_Y = 60, TOP_MARGIN = 50;
    
    int index = 0;
    for (const auto& node : diagram.nodes) {
        float tw = node.label.length() * theme.font_size * 0.6f;
        float w = std::max(80.0f, tw + 20);
        float h = 40;
        data.node_bounds[node.id] = {index * SPACING_X + SPACING_X / 2, TOP_MARGIN, w, h};
        index++;
    }

    float current_y = TOP_MARGIN + 60;
    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, current_y}, {to_it->second.x, current_y}};
            current_y += SPACING_Y;
        }
        data.edge_paths.push_back(path);
    }

    data.width = diagram.nodes.size() * SPACING_X;
    data.height = current_y + 50;
}

void FlexMaid::layout_class(const UnifiedDiagram& diagram, LayoutData& data, const Theme&) {
    const float CLASS_WIDTH = 200, CLASS_HEIGHT = 120;
    const float SPACING_X = 300, SPACING_Y = 200;
    
    int cols = std::max(1, (int)std::sqrt(diagram.nodes.size()));
    
    int index = 0;
    for (const auto& node : diagram.nodes) {
        int row = index / cols, col = index % cols;
        data.node_bounds[node.id] = {col * SPACING_X + SPACING_X / 2, row * SPACING_Y + SPACING_Y / 2, CLASS_WIDTH, CLASS_HEIGHT};
        index++;
    }

    int rows = (int)((diagram.nodes.size() + cols - 1) / cols);
    data.width = cols * SPACING_X;
    data.height = rows * SPACING_Y;

    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, from_it->second.y}, {to_it->second.x, to_it->second.y}};
        }
        data.edge_paths.push_back(path);
    }
}

void FlexMaid::layout_pie(const UnifiedDiagram& diagram, LayoutData& data, const Theme&) {
    const float RADIUS = 150, CENTER_X = 200, CENTER_Y = 200;
    data.width = 400;
    data.height = 400;
    
    int index = 0;
    for (const auto& [key, val] : diagram.props) {
        if (key.find("data_") == 0) {
            float x = CENTER_X + RADIUS * std::cos(index * 0.5f);
            float y = CENTER_Y + RADIUS * std::sin(index * 0.5f);
            data.node_bounds[key] = {x, y, 80, 40};
            index++;
        }
    }
}

void FlexMaid::layout_gitgraph(const UnifiedDiagram& diagram, LayoutData& data, const Theme&) {
    const float SPACING_X = 100, BRANCH_Y = 100;
    data.width = (diagram.nodes.size() + 1) * SPACING_X;
    data.height = 300;
    
    int index = 0;
    for (const auto& node : diagram.nodes) {
        data.node_bounds[node.id] = {(index + 1) * SPACING_X, BRANCH_Y, 20, 20};
        index++;
    }

    for (const auto& edge : diagram.edges) {
        LayoutData::Path path;
        auto from_it = data.node_bounds.find(edge.from);
        auto to_it = data.node_bounds.find(edge.to);
        if (from_it != data.node_bounds.end() && to_it != data.node_bounds.end()) {
            path.points = {{from_it->second.x, from_it->second.y}, {to_it->second.x, to_it->second.y}};
        }
        data.edge_paths.push_back(path);
    }
}

std::pair<float, float> FlexMaid::calculate_text_size(const std::string& text) const {
    return {text.length() * theme_.font_size * 0.6f, theme_.font_size * 1.2f};
}

std::pair<float, float> FlexMaid::calculate_node_size(const std::string& label, NodeShape shape) const {
    auto [tw, th] = calculate_text_size(label);
    float w = std::max(80.0f, tw + 20);
    float h = std::max(40.0f, th + 20);
    if (shape == NodeShape::Circle) { float r = std::max(w, h) / 2; w = h = r * 2; }
    return {w, h};
}

std::string FlexMaid::escape_xml(const std::string& text) {
    std::string out;
    out.reserve(text.length());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

} // namespace flex::modules::flexmaid
