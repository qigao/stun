#include "render/chart_renderer.h"
#include "render/template_manager.h"
#include "render/icon_manager.h"
#include "render/node_layout.h"
#include "mustache/mustache.h"
#include "flexmaid.h"
#include <sstream>
#include <cstring>
#include <algorithm>
#include <list>
#include <cmath>

namespace flex::modules::flexmaid {

// Legacy template kept for fallback - will be removed once all diagram types have dedicated templates
const char* SVG_MUSTACHE_TEMPLATE = R"svg(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
  <rect width="100%" height="100%" fill="{{background_color}}"/>
  <defs>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur in="SourceAlpha" stdDeviation="2"/>
      <feOffset dx="2" dy="2" result="offsetblur"/>
      <feComponentTransfer>
        <feFuncA type="linear" slope="0.2"/>
      </feComponentTransfer>
      <feMerge>
        <feMergeNode/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
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
    <marker id="cross" markerWidth="10" markerHeight="10" refX="5" refY="5" orient="auto">
      <path d="M 2 2 L 8 8 M 8 2 L 2 8" stroke="{{line_color}}" stroke-width="2"/>
    </marker>
  </defs>
  <g>
    {{#subgraphs}}
      <rect x="{{x}}" y="{{y}}" width="{{width}}" height="{{height}}" fill="{{secondary_color}}" stroke="{{line_color}}" stroke-width="1" rx="4" fill-opacity="0.3"/>
      <text x="{{text_x}}" y="{{text_y}}" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}" font-weight="bold">{{label}}</text>
    {{/subgraphs}}
    
    {{#edges}}
      <path d="{{path_d}}" fill="none" stroke="{{line_color}}" stroke-width="{{#is_thick}}{{line_width_thick}}{{/is_thick}}{{^is_thick}}{{line_width}}{{/is_thick}}" 
            {{#is_dotted}}stroke-dasharray="2,2"{{/is_dotted}} {{#is_dashed}}stroke-dasharray="5,5"{{/is_dashed}}
            {{#marker_start}}marker-start="url(#{{marker_start}})"{{/marker_start}} {{#marker_end}}marker-end="url(#{{marker_end}})"{{/marker_end}}/>
      {{#has_label}}
        <rect x="{{label_rect_x}}" y="{{label_rect_y}}" width="{{label_rect_w}}" height="{{label_rect_h}}" fill="{{background_color}}" fill-opacity="0.8"/>
        <text x="{{label_x}}" y="{{label_y}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{label_font_size}}" fill="{{text_color}}">{{label}}</text>
      {{/has_label}}
    {{/edges}}

    {{#nodes}}
      <g id="{{id}}">
        {{#shape_rect}}
          <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>
        {{/shape_rect}}
        {{#shape_round_rect}}
          <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="10"/>
        {{/shape_round_rect}}
        {{#shape_circle}}
          <circle cx="{{x}}" cy="{{y}}" r="{{radius}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
        {{/shape_circle}}
        {{#shape_diamond}}
          <polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
        {{/shape_diamond}}
        {{#shape_note}}
          <path d="{{points_str}}" fill="#fff9c4" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
        {{/shape_note}}
        {{#shape_actor}}
          <circle cx="{{x}}" cy="{{ry_actor}}" r="8" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
          <line x1="{{x}}" y1="{{ry_actor_body}}" x2="{{x}}" y2="{{ry_actor_legs}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
          <line x1="{{x_actor_l}}" y1="{{ry_actor_arms}}" x2="{{x_actor_r}}" y2="{{ry_actor_arms}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
          <line x1="{{x}}" y1="{{ry_actor_legs}}" x2="{{x_actor_l}}" y2="{{ry_actor_feet}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
          <line x1="{{x}}" y1="{{ry_actor_legs}}" x2="{{x_actor_r}}" y2="{{ry_actor_feet}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
        {{/shape_actor}}
        {{#is_sequence}}
          <line x1="{{x}}" y1="{{rect_y_end}}" x2="{{x}}" y2="{{lifeline_y2}}" stroke="{{line_color}}" stroke-width="1" stroke-dasharray="5,5"/>
        {{/is_sequence}}
        
        {{#has_rows}}
          <line x1="{{rect_x}}" y1="{{sep_y}}" x2="{{rect_x_end}}" y2="{{sep_y}}" stroke="{{line_color}}" stroke-width="{{#has_sep2}}1{{/has_sep2}}{{^has_sep2}}{{line_width}}{{/has_sep2}}"/>
          {{#rows}}
            <rect x="{{rect_x}}" y="{{row_y}}" width="{{width}}" height="{{row_h}}" fill="{{row_bg}}" stroke="none"/>
            <line x1="{{rect_x}}" y1="{{row_y}}" x2="{{rect_x_end}}" y2="{{row_y}}" stroke="{{line_color}}" stroke-width="1"/>
            <line x1="{{col_div}}" y1="{{row_y}}" x2="{{col_div}}" y2="{{row_end}}" stroke="{{line_color}}" stroke-width="1"/>
            <text x="{{type_x}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{type}}</text>
            <text x="{{name_x}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{name}}</text>
          {{/rows}}
        {{/has_rows}}

        {{#has_sep2}}
          <line x1="{{rect_x}}" y1="{{sep2_y}}" x2="{{rect_x_end}}" y2="{{sep2_y}}" stroke="{{line_color}}" stroke-width="1"/>
        {{/has_sep2}}

        {{#has_methods}}
          {{#methods}}
            <text x="{{rect_x_sub}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{name}}</text>
          {{/methods}}
        {{/has_methods}}

        <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{#shape_actor}}{{/shape_actor}}{{^shape_actor}}{{label}}{{/shape_actor}}</text>
      </g>
    {{/nodes}}
  </g>
</svg>)svg";

// Mustache 渲染辅助结构 (Internal to CPP)
enum class MustacheNodeType {
    Root, NodesList, EdgesList, SubgraphsList,
    NodeItem, EdgeItem, SubgraphItem, RowItem, MethodItem,
    NodeSection, EdgeSection, Value
};

struct MustacheProxy {
    MustacheNodeType type;
    const void* data;
    const MustacheContext* ctx;
    size_t index = 0;
    size_t parent_index = 0; // For items in lists
};

struct MustacheProviderData {
    std::list<MustacheProxy> pool;
    MustacheProxy root;
};

namespace {

static int out_verbatim(const char* output, size_t size, void* data) {
    if (!data) return 0;
    static_cast<std::ostringstream*>(data)->write(output, size);
    return 0;
}

static int out_escaped(const char* output, size_t size, void* data) {
    if (!data) return 0;
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

static void* get_root(void* data) {
    if (!data) return nullptr;
    return &static_cast<MustacheProviderData*>(data)->root;
}

static void* get_child_by_name(void* node, const char* name, size_t size, void* data) {
    if (!node || !data) return nullptr;
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* mpd = static_cast<MustacheProviderData*>(data);
    auto* pool = &mpd->pool;
    std::string key(name, size);
    const MustacheContext* ctx = proxy->ctx;

    MustacheNodeType effective_type = proxy->type;
    if (effective_type == MustacheNodeType::NodeSection) effective_type = MustacheNodeType::NodeItem;
    if (effective_type == MustacheNodeType::EdgeSection) effective_type = MustacheNodeType::EdgeItem;

    auto make_val = [&](const void* ptr, int type_idx) {
        pool->push_back({MustacheNodeType::Value, ptr, ctx, (size_t)type_idx, 0});
        return (void*)&pool->back();
    };

    auto make_section = [&](bool cond, MustacheNodeType type, size_t idx, size_t pidx = 0) -> void* {
        if (!cond) return nullptr;
        pool->push_back({type, proxy->data, ctx, idx, pidx});
        return &pool->back();
    };

    if (proxy->type == MustacheNodeType::Root) {
        if (key == "width") return make_val(&ctx->width, 1);
        if (key == "height") return make_val(&ctx->height, 1);
        if (key == "background_color") return make_val(ctx->background_color.c_str(), 0);
        if (key == "primary_color") return make_val(ctx->primary_color.c_str(), 0);
        if (key == "secondary_color") return make_val(ctx->secondary_color.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 1);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "font_size") return make_val(&ctx->font_size, 1);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
        if (key == "line_width_thick") return make_val(&ctx->line_width_thick, 1);
        if (key == "label_font_size") return make_val(&ctx->label_font_size, 1);
        if (key == "nodes") { pool->push_back({MustacheNodeType::NodesList, &ctx->nodes, ctx, 0, 0}); return &pool->back(); }
        if (key == "edges") { pool->push_back({MustacheNodeType::EdgesList, &ctx->edges, ctx, 0, 0}); return &pool->back(); }
        if (key == "subgraphs") { pool->push_back({MustacheNodeType::SubgraphsList, &ctx->subgraphs, ctx, 0, 0}); return &pool->back(); }
    } else if (effective_type == MustacheNodeType::NodeItem) {
        auto& n = ctx->nodes[proxy->index];
        if (key == "id") return make_val(n.id.c_str(), 0);
        if (key == "label") return make_val(n.label.c_str(), 0);
        if (key == "x") return make_val(&n.x, 1);
        if (key == "y") return make_val(&n.y, 1);
        if (key == "width") return make_val(&n.width, 1);
        if (key == "height") return make_val(&n.height, 1);
        if (key == "rect_x") return make_val(&n.rect_x, 1);
        if (key == "rect_y") return make_val(&n.rect_y, 1);
        if (key == "rect_x_end") return make_val(&n.rect_x_end, 1);
        if (key == "rect_y_end") return make_val(&n.rect_y_end, 1);
        if (key == "rect_x_sub") return make_val(&n.rect_x_sub, 1);
        if (key == "radius") return make_val(&n.radius, 1);
        if (key == "is_sequence") return make_section(n.is_sequence, MustacheNodeType::NodeSection, proxy->index);
        if (key == "is_architecture") return make_section(n.is_architecture, MustacheNodeType::NodeSection, proxy->index);
        if (key == "has_rows") return make_section(n.has_rows, MustacheNodeType::NodeSection, proxy->index);
        if (key == "has_methods") return make_section(n.has_methods, MustacheNodeType::NodeSection, proxy->index);
        if (key == "has_sep2") return make_section(n.has_sep2, MustacheNodeType::NodeSection, proxy->index);
        if (key == "rows") { pool->push_back({MustacheNodeType::RowItem, &n.rows, ctx, 0, proxy->index}); return &pool->back(); }
        if (key == "methods") { pool->push_back({MustacheNodeType::MethodItem, &n.methods, ctx, 0, proxy->index}); return &pool->back(); }
        if (key == "sep_y") return make_val(&n.sep_y, 1);
        if (key == "sep2_y") return make_val(&n.sep2_y, 1);
        if (key == "points_str") return make_val(n.points_str.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
        if (key == "primary_color") return make_val(n.primary_color.empty() ? ctx->primary_color.c_str() : n.primary_color.c_str(), 0);
        if (key == "text_color") return make_val(n.text_color.empty() ? ctx->text_color.c_str() : n.text_color.c_str(), 0);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "font_size") return make_val(&ctx->font_size, 1);
        if (key == "text_y_node") return make_val(&n.text_y_node, 1);
        if (key == "shape_rect") return make_section(n.shape_rect, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_round_rect") return make_section(n.shape_round_rect, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_circle") return make_section(n.shape_circle, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_diamond") return make_section(n.shape_diamond, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_cylinder") return make_section(n.shape_cylinder, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_note") return make_section(n.shape_note, MustacheNodeType::NodeSection, proxy->index);
        if (key == "shape_actor") return make_section(n.shape_actor, MustacheNodeType::NodeSection, proxy->index);
        if (key == "icon_char") return make_val(n.icon_char.c_str(), 0);
        if (key == "icon_svg") return make_val(n.icon_svg.c_str(), 0);
        if (key == "c4_type_label") return make_val(n.c4_type_label.c_str(), 0);
        if (key == "c4_label_x") return make_val(&n.c4_label_x, 1);
        if (key == "c4_label_y") return make_val(&n.c4_label_y, 1);
        if (key == "name_x") return make_val(&n.name_x, 1);
        if (key == "has_icon") return make_section(!n.icon_svg.empty(), MustacheNodeType::NodeSection, proxy->index);
        if (key == "has_c4_type") return make_section(!n.c4_type_label.empty(), MustacheNodeType::NodeSection, proxy->index);
    } else if (proxy->type == MustacheNodeType::RowItem || proxy->type == MustacheNodeType::MethodItem) {
        auto& parent_node = ctx->nodes[proxy->parent_index];
        auto& r = (proxy->type == MustacheNodeType::RowItem) ? parent_node.rows[proxy->index] : parent_node.methods[proxy->index];
        if (key == "type") return make_val(r.type.c_str(), 0);
        if (key == "name") return make_val(r.name.c_str(), 0);
        if (key == "row_bg") return make_val(r.row_bg.c_str(), 0);
        if (key == "row_y") return make_val(&r.row_y, 1);
        if (key == "row_h") return make_val(&r.row_h, 1);
        if (key == "col_div") return make_val(&r.col_div, 1);
        if (key == "ty") return make_val(&r.ty, 1);
        if (key == "row_end") { static float v; v = r.row_y + r.row_h; return make_val(&v, 1); }
        if (key == "rect_x") return make_val(&parent_node.rect_x, 1);
        if (key == "rect_x_end") return make_val(&parent_node.rect_x_end, 1);
        if (key == "rect_x_sub") return make_val(&parent_node.rect_x_sub, 1);
        if (key == "rect_x_sub_end") return make_val(&parent_node.rect_x_sub_end, 1);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "small_font") { static float v; v = ctx->font_size - 2; return make_val(&v, 1); }
        if (key == "type_x") { static float v; v = parent_node.rect_x + 8; return make_val(&v, 1); }
        if (key == "name_x") { static float v; v = r.col_div + 8; return make_val(&v, 1); }
    } else if (effective_type == MustacheNodeType::EdgeItem) {
        auto& e = ctx->edges[proxy->index];
        if (key == "path_d") return make_val(e.path_d.c_str(), 0);
        if (key == "label") return make_val(e.label.c_str(), 0);
        if (key == "has_label") return make_section(e.has_label, MustacheNodeType::EdgeSection, proxy->index);
        if (key == "label_x") return make_val(&e.label_x, 1);
        if (key == "label_y") return make_val(&e.label_y, 1);
        if (key == "label_rect_x") return make_val(&e.label_rect_x, 1);
        if (key == "label_rect_y") return make_val(&e.label_rect_y, 1);
        if (key == "label_rect_w") return make_val(&e.label_rect_w, 1);
        if (key == "label_rect_h") return make_val(&e.label_rect_h, 1);
        if (key == "is_dotted") return make_section(e.is_dotted, MustacheNodeType::EdgeSection, proxy->index);
        if (key == "is_dashed") return make_section(e.is_dashed, MustacheNodeType::EdgeSection, proxy->index);
        if (key == "is_thick") return make_section(e.is_thick, MustacheNodeType::EdgeSection, proxy->index);
        if (key == "marker_start") {
            if (proxy->type == MustacheNodeType::EdgeSection) return make_val(e.marker_start.c_str(), 0);
            return e.marker_start.empty() ? nullptr : make_section(true, MustacheNodeType::EdgeSection, proxy->index);
        }
        if (key == "marker_end") {
            if (proxy->type == MustacheNodeType::EdgeSection) return make_val(e.marker_end.c_str(), 0);
            return e.marker_end.empty() ? nullptr : make_section(true, MustacheNodeType::EdgeSection, proxy->index);
        }
        if (key == "marker_start_id") return make_val(e.marker_start.c_str(), 0);
        if (key == "marker_end_id") return make_val(e.marker_end.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "line_width") return make_val(&ctx->line_width, 1);
        if (key == "line_width_thick") return make_val(&ctx->line_width_thick, 1);
        if (key == "label_font_size") return make_val(&ctx->label_font_size, 1);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "background_color") return make_val(ctx->background_color.c_str(), 0);
    } else if (effective_type == MustacheNodeType::SubgraphItem) {
        auto& s = ctx->subgraphs[proxy->index];
        if (key == "label") return make_val(s.label.c_str(), 0);
        if (key == "icon_char") return make_val(s.icon_char.c_str(), 0);
        if (key == "x") return make_val(&s.x, 1);
        if (key == "y") return make_val(&s.y, 1);
        if (key == "width") return make_val(&s.width, 1);
        if (key == "height") return make_val(&s.height, 1);
        if (key == "text_x") return make_val(&s.text_x, 1);
        if (key == "text_y") return make_val(&s.text_y, 1);
        if (key == "icon_x") return make_val(&s.icon_x, 1);
        if (key == "icon_y") return make_val(&s.icon_y, 1);
        if (key == "is_architecture") return make_section(s.is_architecture, MustacheNodeType::SubgraphItem, proxy->index);
        if (key == "primary_color") return make_val(ctx->primary_color.c_str(), 0);
        if (key == "secondary_color") return make_val(ctx->secondary_color.c_str(), 0);
        if (key == "line_color") return make_val(ctx->line_color.c_str(), 0);
        if (key == "text_color") return make_val(ctx->text_color.c_str(), 0);
        if (key == "font_family") return make_val(ctx->font_family.c_str(), 0);
    }
    return nullptr;
}

static void* get_child_by_index(void* node, unsigned index, void* data) {
    if (!node || !data) return nullptr;
    auto* proxy = static_cast<MustacheProxy*>(node);
    auto* mpd = static_cast<MustacheProviderData*>(data);
    auto* pool = &mpd->pool;
    const MustacheContext* ctx = proxy->ctx;

    if (proxy->type == MustacheNodeType::NodesList && index < ctx->nodes.size()) {
        pool->push_back({MustacheNodeType::NodeItem, nullptr, ctx, index, 0});
        return &pool->back();
    }
    if (proxy->type == MustacheNodeType::EdgesList && index < ctx->edges.size()) {
        pool->push_back({MustacheNodeType::EdgeItem, nullptr, ctx, index, 0});
        return &pool->back();
    }
    if (proxy->type == MustacheNodeType::SubgraphsList && index < ctx->subgraphs.size()) {
        pool->push_back({MustacheNodeType::SubgraphItem, nullptr, ctx, index, 0});
        return &pool->back();
    }
    if (proxy->type == MustacheNodeType::RowItem || proxy->type == MustacheNodeType::MethodItem) {
        auto& node_data = ctx->nodes[proxy->parent_index];
        const auto& list = (proxy->type == MustacheNodeType::RowItem) ? node_data.rows : node_data.methods;
        if (index < list.size()) {
            pool->push_back({proxy->type, nullptr, ctx, index, proxy->parent_index});
            return &pool->back();
        }
    }
    if ((proxy->type == MustacheNodeType::NodeSection || proxy->type == MustacheNodeType::EdgeSection) && index == 0) {
        auto item_type = (proxy->type == MustacheNodeType::NodeSection) ? MustacheNodeType::NodeItem : MustacheNodeType::EdgeItem;
        pool->push_back({item_type, nullptr, ctx, proxy->index, proxy->parent_index});
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
        return out(buf, (int)strlen(buf), rdata);
    }
    const char* str = static_cast<const char*>(proxy->data);
    return out(str, (int)strlen(str), rdata);
}

static std::string get_marker_id(EdgeDecoration dec, bool is_start = false) {
    std::string base;
    switch (dec) {
        case EdgeDecoration::Arrow: base = "arrowhead"; break;
        case EdgeDecoration::Diamond: return "diamond";  // symmetric
        case EdgeDecoration::DiamondFilled: return "diamond_filled";  // symmetric
        case EdgeDecoration::Circle: return "circle";  // symmetric
        case EdgeDecoration::Cross: return "cross";  // symmetric
        case EdgeDecoration::Triangle: base = "triangle"; break;
        case EdgeDecoration::TriangleFilled: base = "triangle_filled"; break;
        default: return "";
    }
    return is_start ? base + "_start" : base;
}

} // namespace

static MUSTACHE_TEMPLATE* get_partial_callback(const char* name, size_t size, void* data) {
    (void)data;
    std::string partial_name(name, size);
    const auto& partial = TemplateManager::instance().get_partial(partial_name);
    if (partial.empty()) return nullptr;
    return mustache_compile(partial.c_str(), partial.length(), nullptr, nullptr, 0);
}

std::string ChartRenderer::generate_svg(const UnifiedDiagram& diagram, const LayoutData& layout, const Theme& theme) {
    MustacheContext ctx = build_context(diagram, layout, theme);
    
    float max_x = 0, max_y = 0;
    for (const auto& n : ctx.nodes) {
        max_x = std::max(max_x, n.x + n.width / 2 + 20);
        max_y = std::max(max_y, n.text_y + 20);
        if (n.has_rows && !n.rows.empty())
            max_y = std::max(max_y, n.rows.back().row_y + n.rows.back().row_h + 20);
        if (n.has_methods && !n.methods.empty())
            max_y = std::max(max_y, n.methods.back().row_y + n.methods.back().row_h + 20);
    }
    for (const auto& s : ctx.subgraphs) {
        max_x = std::max(max_x, s.x + s.width + 20);
        max_y = std::max(max_y, s.y + s.height + 20);
    }
    ctx.width = std::max({ctx.width, max_x, layout.width});
    ctx.height = std::max({ctx.height, max_y, layout.height});

    MUSTACHE_RENDERER renderer = { out_verbatim, out_escaped };
    MUSTACHE_DATAPROVIDER provider = { dump, get_root, get_child_by_name, get_child_by_index, get_partial_callback };

    MustacheProviderData mpd;
    mpd.root = {MustacheNodeType::Root, nullptr, &ctx, 0, 0};

    const std::string& tpl_str = TemplateManager::instance().get_template(diagram.type);
    std::ostringstream ss;
    MUSTACHE_TEMPLATE* tpl = mustache_compile(tpl_str.c_str(), tpl_str.length(), nullptr, nullptr, 0);
    if (tpl) {
        mustache_process(tpl, &renderer, &ss, &provider, &mpd);
        mustache_release(tpl);
    }
    return ss.str();
}

MustacheContext ChartRenderer::build_context(const UnifiedDiagram& diagram, const LayoutData& layout, const Theme& theme) {
    MustacheContext ctx{};
    ctx.width = layout.width;
    ctx.height = layout.height;
    ctx.background_color = theme.background_color;
    ctx.primary_color = theme.primary_color;
    ctx.secondary_color = theme.secondary_color;
    ctx.text_color = theme.text_color;
    ctx.line_color = theme.line_color;
    ctx.font_family = theme.font_family;
    ctx.font_size = (float)theme.font_size;
    ctx.line_width = theme.line_width;
    ctx.line_width_thick = theme.line_width * 2.0f;
    ctx.label_font_size = (float)theme.font_size - 2.0f;

    // Handle offsets (Legacy Sequence/Flowchart logic)
    float min_y = 0;
    for (const auto& sub : diagram.subgraphs) {
        for (const auto& node_id : sub.node_ids) {
            auto it = layout.node_bounds.find(node_id);
            if (it != layout.node_bounds.end()) {
                min_y = std::min(min_y, it->second.top() - 40);
            }
        }
    }
    float offset_y = (min_y < 0) ? -min_y + 10 : 0;

    for (const auto& [id, node] : diagram.nodes) {
        if (layout.node_bounds.count(id)) {
            LayoutData::Bounds adjusted = layout.node_bounds.at(id);
            adjusted.y += offset_y;
            ctx.nodes.push_back(build_node_data(node, adjusted, diagram, theme));
        }
    }

    for (size_t i = 0; i < diagram.edges.size() && i < layout.edge_paths.size(); ++i) {
        LayoutData::Path adjusted_path = layout.edge_paths[i];
        for (auto& p : adjusted_path.points) p.second += offset_y;
        ctx.edges.push_back(build_edge_data(diagram.edges[i], adjusted_path, theme));
    }

    for (const auto& sub : diagram.subgraphs) {
        ctx.subgraphs.push_back(build_subgraph_data(sub, layout, theme, offset_y));
    }

    return ctx;
}

MustacheNodeData ChartRenderer::build_node_data(const Node& node, const LayoutData::Bounds& bounds, const UnifiedDiagram& diagram, const Theme& theme) {
    MustacheNodeData n{};
    n.id = node.id; n.label = node.label; n.x = bounds.x; n.y = bounds.y; n.width = bounds.width; n.height = bounds.height;
    n.is_sequence = (diagram.type == DiagramType::Sequence);
    n.is_architecture = (diagram.type == DiagramType::Architecture);
    n.lifeline_y2 = (n.is_sequence) ? 800.0f : 0; n.text_y = bounds.bottom() + 15; n.text_y_node = bounds.y;
    n.is_participant = (diagram.type == DiagramType::Sequence && node.get_prop("is_note") != "true");
    n.rect_x = bounds.left(); n.rect_y = bounds.top(); n.radius = std::min(bounds.width, bounds.height) / 2;
    n.radius_inner = std::max(0.0f, n.radius - 5.0f); n.rect_x_end = bounds.right(); n.rect_y_end = bounds.bottom();
    n.rect_x_sub = n.rect_x + 5; n.rect_x_sub_end = n.rect_x + bounds.width - 5;
    
    // Icon for architecture diagrams - use SVG icons
    std::string icon = node.get_prop("icon");
    if (!icon.empty()) {
        n.icon_svg = IconManager::instance().render_svg(icon, n.x, n.y, 24.0f, "#ffffff");
    }
    
    // Icon and type label for C4 diagrams - use NodeLayout
    std::string c4_type = node.get_prop("c4_type");
    if (!c4_type.empty()) {
        std::string icon_name, type_label;
        bool is_db = false;
        if (c4_type == "Person" || c4_type == "Person_Ext") { icon_name = "person"; type_label = "person"; }
        else if (c4_type == "System" || c4_type == "System_Ext") { icon_name = "system"; type_label = "system"; }
        else if (c4_type == "SystemDb" || c4_type == "SystemDb_Ext") { icon_name = "database"; type_label = "system_db"; is_db = true; }
        else if (c4_type == "Container" || c4_type == "Container_Ext") { icon_name = "container"; type_label = "container"; }
        else if (c4_type == "ContainerDb" || c4_type == "ContainerDb_Ext") { icon_name = "database"; type_label = "container_db"; is_db = true; }
        else if (c4_type == "Component" || c4_type == "Component_Ext") { icon_name = "component"; type_label = "component"; }
        
        // DB types use cylinder shape
        if (is_db) {
            n.shape_rect = false;
            n.shape_cylinder = true;
            n.radius = bounds.width / 2;  // cylinder rx = width/2
        }
        
        // Use C4Grid layout: type(top-left), name(bottom-left), icon(bottom-right)
        NodeLayout layout;
        layout.compute(NodeLayoutMode::C4Grid, n.rect_x, n.rect_y, bounds.width, bounds.height);
        
        n.c4_label_x = layout.type_label.x;
        n.c4_label_y = layout.type_label.y;
        n.name_x = layout.name.x;
        n.text_y_node = layout.name.y;
        
        if (!icon_name.empty() && !is_db) {
            // Don't show icon for DB types - cylinder shape is the icon
            n.icon_svg = IconManager::instance().render_svg(icon_name, layout.icon.x, layout.icon.y, 24.0f, "#ffffff");
        }
        if (!type_label.empty()) {
            n.c4_type_label = "<<" + type_label + ">>";
        }
    }
    
    switch (node.shape) {
        case NodeShape::Rectangle: case NodeShape::Class: case NodeShape::Entity: n.shape_rect = true; break;
        case NodeShape::RoundRect: case NodeShape::State: n.shape_round_rect = true; break;
        case NodeShape::Circle: case NodeShape::CircleFilled: n.shape_circle = true; break;
        case NodeShape::DoubleCircle: n.shape_double_circle = true; break;
        case NodeShape::Diamond: n.shape_diamond = true; break;
        case NodeShape::Stadium: n.shape_stadium = true; break;
        case NodeShape::Cylinder: n.shape_cylinder = true; break;
        case NodeShape::Subroutine: n.shape_subroutine = true; break;
        case NodeShape::Parallelogram: case NodeShape::ParallelogramAlt: n.shape_parallelogram = true; break;
        case NodeShape::Trapezoid: case NodeShape::TrapezoidAlt: n.shape_trapezoid = true; break;
        case NodeShape::Note: n.shape_note = true; break;
        case NodeShape::Actor: n.shape_actor = true; break;
        default: n.shape_rect = true; break;
    }

    std::ostringstream pts;
    if (n.shape_diamond) { pts << bounds.x << "," << n.rect_y << " " << n.rect_x_end << "," << bounds.y << " " << bounds.x << "," << n.rect_y_end << " " << n.rect_x << "," << bounds.y; n.points_str = pts.str(); }
    else if (n.shape_parallelogram) { float skew = bounds.width * 0.2f; if (node.shape == NodeShape::ParallelogramAlt) skew = -skew; pts << (n.rect_x + skew) << "," << n.rect_y << " " << n.rect_x_end << "," << n.rect_y << " " << (n.rect_x_end - skew) << "," << n.rect_y_end << " " << n.rect_x << "," << n.rect_y_end; n.points_str = pts.str(); }
    else if (n.shape_trapezoid) { float skew = bounds.width * 0.15f; if (node.shape == NodeShape::TrapezoidAlt) { pts << n.rect_x << "," << n.rect_y << " " << n.rect_x_end << "," << n.rect_y << " " << (n.rect_x_end - skew) << "," << n.rect_y_end << " " << (n.rect_x + skew) << "," << n.rect_y_end; } else { pts << (n.rect_x + skew) << "," << n.rect_y << " " << (n.rect_x_end - skew) << "," << n.rect_y << " " << n.rect_x_end << "," << n.rect_y_end << " " << n.rect_x << "," << n.rect_y_end; } n.points_str = pts.str(); }
    else if (n.shape_note) { float c = 15; pts << "M " << n.rect_x << " " << n.rect_y << " L " << (n.rect_x_end - c) << " " << n.rect_y << " L " << n.rect_x_end << " " << (n.rect_y + c) << " L " << n.rect_x_end << " " << n.rect_y_end << " L " << n.rect_x << " " << n.rect_y_end << " Z M " << (n.rect_x_end - c) << " " << n.rect_y << " L " << (n.rect_x_end - c) << " " << (n.rect_y + c) << " L " << n.rect_x_end << " " << (n.rect_y + c); n.points_str = pts.str(); }
    else if (n.shape_actor) { n.ry_actor = n.rect_y + 10; n.ry_actor_body = n.ry_actor + 8; n.ry_actor_arms = n.ry_actor_body + 10; n.ry_actor_legs = n.ry_actor_body + 20; n.ry_actor_feet = n.ry_actor_legs + 12; n.x_actor_l = n.x - 12; n.x_actor_r = n.x + 12; n.text_y_node = n.ry_actor_feet + 15; }

    return n;
}

MustacheEdgeData ChartRenderer::build_edge_data(const Edge& edge, const LayoutData::Path& path, const Theme& theme) {
    MustacheEdgeData e{};
    e.label = edge.label; e.has_label = !edge.label.empty() && path.points.size() >= 2;
    std::ostringstream d_pts;
    for (size_t i = 0; i < path.points.size(); ++i) d_pts << (i == 0 ? "M " : " L ") << path.points[i].first << " " << path.points[i].second;
    e.path_d = d_pts.str();

    if (e.has_label) {
        size_t mid_idx = path.points.size() / 2;
        if (mid_idx > 0) { e.label_x = (path.points[mid_idx-1].first + path.points[mid_idx].first) / 2; e.label_y = (path.points[mid_idx-1].second + path.points[mid_idx].second) / 2; }
        else { e.label_x = path.points[0].first; e.label_y = path.points[0].second; }
        float tw = (float)edge.label.length() * ((float)theme.font_size - 2) * 0.6f;
        float th = ((float)theme.font_size - 2) * 1.2f;
        e.label_rect_w = tw + 4; e.label_rect_h = th; e.label_rect_x = e.label_x - e.label_rect_w / 2; e.label_rect_y = e.label_y - e.label_rect_h / 2;
    }
    
    e.is_dotted = (edge.style == EdgeStyle::Dotted); e.is_dashed = (edge.style == EdgeStyle::Dashed); e.is_thick = (edge.style == EdgeStyle::Thick);
    e.marker_start = get_marker_id(edge.start_decoration, true); e.marker_end = get_marker_id(edge.end_decoration, false);
    return e;
}

MustacheSubgraphData ChartRenderer::build_subgraph_data(const Subgraph& sub, const LayoutData& layout, const Theme& theme, float offset_y) {
    MustacheSubgraphData s{};
    float min_x = 1e9, min_y = 1e9, max_x = -1e9, max_y = -1e9;
    bool has_nodes = false;
    for (const auto& node_id : sub.node_ids) {
        auto it = layout.node_bounds.find(node_id);
        if (it != layout.node_bounds.end()) {
            float adj_y = it->second.y + offset_y;
            min_x = std::min(min_x, it->second.left()); 
            min_y = std::min(min_y, adj_y - it->second.height / 2);
            max_x = std::max(max_x, it->second.right()); 
            max_y = std::max(max_y, adj_y + it->second.height / 2 + 20);
            has_nodes = true;
        }
    }
    if (has_nodes) {
        s.x = min_x - 20; s.y = min_y - 40; s.width = (max_x - min_x) + 40; s.height = (max_y - min_y) + 60;
        s.label = sub.label;
        
        std::string icon = sub.get_prop("icon");
        if (!icon.empty()) {
            s.icon_char = IconManager::instance().render_svg(icon, s.x + 17, s.y + 17, 16.0f, "#333333");
        }
        
        s.icon_x = s.x + 5; s.icon_y = s.y + 5; s.text_x = s.icon_x + 30; s.text_y = s.y + 20;
    }
    return s;
}

} // namespace flex::modules::flexmaid
