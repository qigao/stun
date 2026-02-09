#pragma once

namespace mermaid {
namespace flowchart {

/**
 * Returns the premium Flowchart SVG Template.
 * Isolated as a static function to ensure ODR compliance across all C++ versions.
 */
static const char* get_svg_template() {
    return R"svg(
<svg width="{{width}}" height="{{height}}" viewBox="0 0 {{width}} {{height}}" xmlns="http://www.w3.org/2000/svg">
<style>
  .node { cursor: pointer; transition: all 0.4s cubic-bezier(0.16, 1, 0.3, 1); }
  .node:hover .shape { transform: translateY(-3px); filter: drop-shadow(0 8px 16px rgba(0,0,0,0.12)); }
  .node-text { font-family: 'Segoe UI', system-ui, sans-serif; user-select: none; pointer-events: none; }
  .edge-path { stroke-dasharray: 2000; stroke-dashoffset: 2000; animation: draw 2.2s cubic-bezier(0.45, 0, 0.55, 1) forwards; }
  @keyframes draw { to { stroke-dashoffset: 0; } }
  .label-text { font-family: 'Segoe UI', system-ui, sans-serif; font-weight: 600; font-size: 11px; }
</style>
<rect width="100%" height="100%" fill="{{background_color}}"/>
<defs>
  <linearGradient id="nodeGrad" x1="0%" y1="0%" x2="0%" y2="100%">
    <stop offset="0%" style="stop-color:{{primary_color}};stop-opacity:1" />
    <stop offset="100%" style="stop-color:{{primary_color_alt}};stop-opacity:1" />
  </linearGradient>
  <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
    <path d="M0 0 L10 3.5 L0 7" fill="{{line_color}}" opacity="0.4"/>
  </marker>
</defs>
<g>
  {{#edges}}
  <path class="edge-path" d="{{path_d}}" fill="none" stroke="{{line_color}}" stroke-width="{{line_width}}" stroke-opacity="0.5" marker-end="url(#arrowhead)" />
  {{#has_label}}
  <rect x="{{label_rect_x}}" y="{{label_rect_y}}" width="{{label_rect_w}}" height="{{label_rect_h}}" fill="{{background_color}}" rx="6"/>
  <text class="label-text" x="{{label_x}}" y="{{label_y}}" text-anchor="middle" dominant-baseline="middle" fill="{{text_color}}" opacity="0.7">{{label}}</text>
  {{/has_label}}
  {{/edges}}

  {{#nodes}}
  <g id="{{id}}" class="node">
    <g class="shape">
      {{#shape_diamond}}
      <polygon points="{{points_str}}" fill="url(#nodeGrad)" stroke="{{line_color}}" stroke-width="1.5" stroke-opacity="0.4" />
      {{/shape_diamond}}
      {{#shape_circle}}
      <circle cx="{{x}}" cy="{{y}}" r="{{radius}}" fill="url(#nodeGrad)" stroke="{{line_color}}" stroke-width="1.5" stroke-opacity="0.4" />
      {{/shape_circle}}
      {{#shape_rect}}
      <rect x="{{rect_x}}" y="{{rect_y}}" width="{{width}}" height="{{height}}" rx="12" fill="url(#nodeGrad)" stroke="{{line_color}}" stroke-width="1.5" stroke-opacity="0.4" />
      {{/shape_rect}}
    </g>
    <text class="node-text" x="{{x}}" y="{{text_y_node}}" text-anchor="middle" dominant-baseline="middle" font-size="14" font-weight="600" fill="{{text_color}}" opacity="0.85">{{label}}</text>
  </g>
  {{/nodes}}
</g>
</svg>)svg";
}

} // namespace flowchart
} // namespace mermaid
