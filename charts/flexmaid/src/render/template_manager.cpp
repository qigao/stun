#include "render/template_manager.h"

namespace flex::modules::flexmaid {

namespace templates {

const char* DEFS = R"TPL(
<defs>
  <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
    <feGaussianBlur in="SourceAlpha" stdDeviation="2"/>
    <feOffset dx="2" dy="2" result="offsetblur"/>
    <feComponentTransfer><feFuncA type="linear" slope="0.2"/></feComponentTransfer>
    <feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
  <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
    <polygon points="0 0, 10 3.5, 0 7" fill="{{line_color}}"/>
  </marker>
  <marker id="arrowhead_start" markerWidth="10" markerHeight="7" refX="1" refY="3.5" orient="auto">
    <polygon points="10 0, 0 3.5, 10 7" fill="{{line_color}}"/>
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
  <marker id="triangle" markerWidth="12" markerHeight="12" refX="10" refY="6" orient="auto">
    <polygon points="0 0, 12 6, 0 12" fill="{{background_color}}" stroke="{{line_color}}" stroke-width="1"/>
  </marker>
  <marker id="triangle_start" markerWidth="12" markerHeight="12" refX="2" refY="6" orient="auto">
    <polygon points="12 0, 0 6, 12 12" fill="{{background_color}}" stroke="{{line_color}}" stroke-width="1"/>
  </marker>
  <marker id="triangle_filled" markerWidth="10" markerHeight="10" refX="8" refY="5" orient="auto">
    <polygon points="0 0, 10 5, 0 10" fill="{{line_color}}"/>
  </marker>
  <marker id="triangle_filled_start" markerWidth="10" markerHeight="10" refX="2" refY="5" orient="auto">
    <polygon points="10 0, 0 5, 10 10" fill="{{line_color}}"/>
  </marker>
</defs>
)TPL";

const char* EDGE = R"TPL(
<path d="{{path_d}}" fill="none" stroke="{{line_color}}" stroke-width="{{#is_thick}}{{line_width_thick}}{{/is_thick}}{{^is_thick}}{{line_width}}{{/is_thick}}" {{#is_dotted}}stroke-dasharray="2,2"{{/is_dotted}}{{#is_dashed}}stroke-dasharray="5,5"{{/is_dashed}}{{#marker_start}} marker-start="url(#{{marker_start_id}})"{{/marker_start}}{{#marker_end}} marker-end="url(#{{marker_end_id}})"{{/marker_end}}/>
{{#has_label}}
<rect x="{{label_rect_x}}" y="{{label_rect_y}}" width="{{label_rect_w}}" height="{{label_rect_h}}" fill="{{background_color}}" fill-opacity="0.8"/>
<text x="{{label_x}}" y="{{label_y}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{label_font_size}}" fill="{{text_color}}">{{label}}</text>
{{/has_label}}
)TPL";

const char* SUBGRAPH = R"TPL(
<rect x="{{x}}" y="{{y}}" width="{{width}}" height="{{height}}" fill="{{secondary_color}}" stroke="{{line_color}}" stroke-width="1" rx="4" fill-opacity="0.3"/>
<text x="{{text_x}}" y="{{text_y}}" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}" font-weight="bold">{{label}}</text>
)TPL";

const char* FLOWCHART = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#subgraphs}}{{> subgraph}}{{/subgraphs}}
  {{#edges}}{{> edge}}{{/edges}}
  {{#nodes}}
  <g id="{{id}}">
    {{#shape_rect}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>{{/shape_rect}}
    {{#shape_round_rect}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="10"/>{{/shape_round_rect}}
    {{#shape_stadium}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="{{radius}}"/>{{/shape_stadium}}
    {{#shape_circle}}<circle cx="{{x}}" cy="{{y}}" r="{{radius}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_circle}}
    {{#shape_diamond}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_diamond}}
    {{#shape_parallelogram}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_parallelogram}}
    {{#shape_trapezoid}}<polygon points="{{points_str}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>{{/shape_trapezoid}}
    {{#shape_subroutine}}
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{rect_x_sub}}" y1="{{rect_y}}" x2="{{rect_x_sub}}" y2="{{rect_y_end}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{rect_x_sub_end}}" y1="{{rect_y}}" x2="{{rect_x_sub_end}}" y2="{{rect_y_end}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    {{/shape_subroutine}}
    <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>
)TPL";

const char* CLASS = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#nodes}}
  <g id="{{id}}">
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="#9370DB" stroke-width="1" rx="0"/>
    <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}" font-weight="bold">{{label}}</text>
    {{#has_rows}}
    <line x1="{{rect_x}}" y1="{{sep_y}}" x2="{{rect_x_end}}" y2="{{sep_y}}" stroke="#9370DB" stroke-width="1"/>
    {{#rows}}
    <text x="{{rect_x_sub}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{name}}</text>
    {{/rows}}
    {{/has_rows}}
    {{#has_sep2}}
    <line x1="{{rect_x}}" y1="{{sep2_y}}" x2="{{rect_x_end}}" y2="{{sep2_y}}" stroke="#9370DB" stroke-width="1"/>
    {{/has_sep2}}
    {{#has_methods}}
    {{#methods}}
    <text x="{{rect_x_sub}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{name}}</text>
    {{/methods}}
    {{/has_methods}}
  </g>
  {{/nodes}}
  {{#edges}}{{> edge}}{{/edges}}
</g>
</svg>
)TPL";

const char* ER = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#edges}}{{> edge}}{{/edges}}
  {{#nodes}}
  <g id="{{id}}">
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="0"/>
    <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}" font-weight="bold">{{label}}</text>
    {{#has_rows}}
    <line x1="{{rect_x}}" y1="{{sep_y}}" x2="{{rect_x_end}}" y2="{{sep_y}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    {{#rows}}
    <rect x="{{rect_x}}" y="{{row_y}}" width="{{width}}" height="{{row_h}}" fill="{{row_bg}}" stroke="none"/>
    <line x1="{{rect_x}}" y1="{{row_y}}" x2="{{rect_x_end}}" y2="{{row_y}}" stroke="{{line_color}}" stroke-width="1"/>
    <line x1="{{col_div}}" y1="{{row_y}}" x2="{{col_div}}" y2="{{row_end}}" stroke="{{line_color}}" stroke-width="1"/>
    <text x="{{type_x}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{type}}</text>
    <text x="{{name_x}}" y="{{ty}}" font-family="{{font_family}}" font-size="{{small_font}}" fill="{{text_color}}">{{name}}</text>
    {{/rows}}
    {{/has_rows}}
  </g>
  {{/nodes}}
</g>
</svg>
)TPL";

const char* SEQUENCE = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#nodes}}
  <g id="{{id}}">
    {{#shape_actor}}
    <circle cx="{{x}}" cy="{{ry_actor}}" r="8" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{x}}" y1="{{ry_actor_body}}" x2="{{x}}" y2="{{ry_actor_legs}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{x_actor_l}}" y1="{{ry_actor_arms}}" x2="{{x_actor_r}}" y2="{{ry_actor_arms}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{x}}" y1="{{ry_actor_legs}}" x2="{{x_actor_l}}" y2="{{ry_actor_feet}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{x}}" y1="{{ry_actor_legs}}" x2="{{x_actor_r}}" y2="{{ry_actor_feet}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    {{/shape_actor}}
    {{^shape_actor}}
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>
    {{/shape_actor}}
    {{#shape_note}}
    <path d="{{points_str}}" fill="#fff9c4" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    {{/shape_note}}
    {{#is_sequence}}
    <line x1="{{x}}" y1="{{rect_y_end}}" x2="{{x}}" y2="{{lifeline_y2}}" stroke="{{line_color}}" stroke-width="1" stroke-dasharray="5,5"/>
    {{/is_sequence}}
    <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
  </g>
  {{/nodes}}
  {{#edges}}{{> edge}}{{/edges}}
</g>
</svg>
)TPL";

const char* C4 = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#subgraphs}}
  <rect x="{{x}}" y="{{y}}" width="{{width}}" height="{{height}}" fill="none" stroke="{{line_color}}" stroke-width="1" stroke-dasharray="5,5" rx="4"/>
  <text x="{{text_x}}" y="{{text_y}}" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}" font-weight="bold">{{label}}</text>
  {{/subgraphs}}
  {{#edges}}{{> edge}}{{/edges}}
  {{#nodes}}
  <g id="{{id}}">
    {{#shape_cylinder}}
    <ellipse cx="{{x}}" cy="{{rect_y}}" rx="{{radius}}" ry="10" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="none"/>
    <line x1="{{rect_x}}" y1="{{rect_y}}" x2="{{rect_x}}" y2="{{rect_y_end}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <line x1="{{rect_x_end}}" y1="{{rect_y}}" x2="{{rect_x_end}}" y2="{{rect_y_end}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    <ellipse cx="{{x}}" cy="{{rect_y_end}}" rx="{{radius}}" ry="10" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}"/>
    {{/shape_cylinder}}
    {{^shape_cylinder}}
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>
    {{/shape_cylinder}}
    {{#has_c4_type}}<text x="{{c4_label_x}}" y="{{c4_label_y}}" font-family="{{font_family}}" font-size="10" fill="#ffffff" font-style="italic">{{c4_type_label}}</text>{{/has_c4_type}}
    {{#has_icon}}{{{icon_svg}}}{{/has_icon}}
    <text x="{{name_x}}" y="{{text_y_node}}" font-family="{{font_family}}" font-size="{{font_size}}" fill="#ffffff" font-weight="bold">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>
)TPL";

const char* BLOCK = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#subgraphs}}{{> subgraph}}{{/subgraphs}}
  {{#edges}}{{> edge}}{{/edges}}
  {{#nodes}}
  <g id="{{id}}">
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>
    <text x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>
)TPL";

const char* ARCHITECTURE = R"TPL(
<svg width="{{width}}" height="{{height}}" xmlns="http://www.w3.org/2000/svg">
<rect width="100%" height="100%" fill="{{background_color}}"/>
{{> defs}}
<g>
  {{#subgraphs}}
  <rect x="{{x}}" y="{{y}}" width="{{width}}" height="{{height}}" fill="none" stroke="{{line_color}}" stroke-width="1" stroke-dasharray="5,5" rx="4"/>
  {{{icon_char}}}
  <text x="{{text_x}}" y="{{text_y}}" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
  {{/subgraphs}}
  {{#edges}}{{> edge}}{{/edges}}
  {{#nodes}}
  <g id="{{id}}">
    <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" fill="{{primary_color}}" stroke="{{line_color}}" stroke-width="{{line_width}}" rx="4"/>
    {{#has_icon}}{{{icon_svg}}}{{/has_icon}}
    <text x="{{x}}" y="{{rect_y_end}}" text-anchor="middle" dominant-baseline="hanging" font-family="{{font_family}}" font-size="{{font_size}}" fill="{{text_color}}">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>
)TPL";

} // namespace templates

TemplateManager& TemplateManager::instance() {
    static TemplateManager mgr;
    return mgr;
}

TemplateManager::TemplateManager() {
    load_embedded_templates();
}

void TemplateManager::load_embedded_templates() {
    partials_["defs"] = templates::DEFS;
    partials_["edge"] = templates::EDGE;
    partials_["subgraph"] = templates::SUBGRAPH;
    
    templates_[DiagramType::Flowchart] = templates::FLOWCHART;
    templates_[DiagramType::Class] = templates::CLASS;
    templates_[DiagramType::ER] = templates::ER;
    templates_[DiagramType::Sequence] = templates::SEQUENCE;
    templates_[DiagramType::C4] = templates::C4;
    templates_[DiagramType::Block] = templates::BLOCK;
    templates_[DiagramType::Architecture] = templates::ARCHITECTURE;
}

const std::string& TemplateManager::get_template(DiagramType type) const {
    auto it = templates_.find(type);
    if (it != templates_.end()) return it->second;
    static const std::string& flowchart = templates_.at(DiagramType::Flowchart);
    return flowchart;
}

const std::string& TemplateManager::get_partial(const std::string& name) const {
    auto it = partials_.find(name);
    if (it != partials_.end()) return it->second;
    static std::string empty;
    return empty;
}

void TemplateManager::set_template_dir(const std::string& dir) {
    template_dir_ = dir;
}

void TemplateManager::reload() {
    load_embedded_templates();
}

} // namespace flex::modules::flexmaid
