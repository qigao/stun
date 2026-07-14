#pragma once

namespace dotgraph {

static const char* get_svg_template() {
    return R"svg(
<svg width="{{width}}" height="{{height}}" viewBox="0 0 {{width}} {{height}}" xmlns="http://www.w3.org/2000/svg">
<style>
  .node { cursor: pointer; transition: all 0.3s ease; }
  .node:hover .shape { filter: drop-shadow(0 4px 8px rgba(0,0,0,0.15)); }
  .node-text { font-family: 'Segoe UI', system-ui, sans-serif; user-select: none; pointer-events: none; }
  .edge-path { transition: stroke-opacity 0.3s ease; }
  .cluster-rect { rx: 8; ry: 8; }
  .label-text { font-family: 'Segoe UI', system-ui, sans-serif; font-weight: 500; font-size: 11px; }
</style>
<rect width="100%" height="100%" fill="{{bgcolor}}"/>
<defs>
  <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
    <path d="M0 0 L10 3.5 L0 7" fill="{{arrow_fill}}" opacity="{{arrow_opacity}}"/>
  </marker>
</defs>
<g>
  {{#clusters}}
  <rect class="cluster-rect" x="{{x}}" y="{{y}}" width="{{w}}" height="{{h}}" fill="none" stroke="{{color}}" stroke-width="{{penwidth}}" stroke-dasharray="{{stroke_dasharray}}" opacity="{{opacity}}"/>
  {{#has_label}}<text class="label-text" x="{{label_x}}" y="{{label_y}}" text-anchor="middle" fill="{{label_fontcolor}}">{{label}}</text>{{/has_label}}
  {{/clusters}}

  {{#edges}}
  <path class="edge-path" d="{{path_d}}" fill="none" stroke="{{color}}" stroke-width="{{penwidth}}" stroke-opacity="{{stroke_opacity}}" {{#directed}}marker-end="url(#arrowhead)"{{/directed}} />
  {{#has_label}}
  <rect x="{{label_rect_x}}" y="{{label_rect_y}}" width="{{label_rect_w}}" height="{{label_rect_h}}" fill="#ffffff" rx="{{label_rect_rx}}"/>
  <text class="label-text" x="{{label_x}}" y="{{label_y}}" text-anchor="middle" dominant-baseline="middle" fill="{{label_fontcolor}}">{{label}}</text>
  {{/has_label}}
  {{/edges}}

  {{#nodes}}
  <g id="node-{{id}}" class="node">
    <g class="shape">
      {{#is_ellipse}}<ellipse cx="{{cx}}" cy="{{cy}}" rx="{{rx}}" ry="{{ry}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_ellipse}}
      {{#is_box}}<rect x="{{rect_x}}" y="{{rect_y}}" width="{{w}}" height="{{h}}" rx="{{box_rx}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_box}}
      {{#is_diamond}}<polygon points="{{diamond_points}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_diamond}}
      {{#is_circle}}<circle cx="{{cx}}" cy="{{cy}}" r="{{r}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_circle}}
      {{#is_doublecircle}}<circle cx="{{cx}}" cy="{{cy}}" r="{{r}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/><circle cx="{{cx}}" cy="{{cy}}" r="{{r_inner}}" fill="none" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_doublecircle}}
      {{#is_triangle}}<polygon points="{{triangle_points}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_triangle}}
      {{#is_hexagon}}<polygon points="{{hexagon_points}}" fill="{{fillcolor}}" stroke="{{color}}" stroke-width="{{penwidth}}"/>{{/is_hexagon}}
      {{#is_plaintext}}{{/is_plaintext}}
    </g>
    <text class="node-text" x="{{cx}}" y="{{text_y}}" text-anchor="middle" dominant-baseline="middle" font-size="{{fontsize}}" font-weight="{{fontweight}}" fill="{{fontcolor}}">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>)svg";
}

} // namespace dotgraph
