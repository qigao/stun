# Mermaid Charts (C/C++)

This folder contains the C/C++ Mermaid chart engines used by the project.

## What is here

- Individual chart modules live in subfolders (e.g. `flowchart`, `sequence`, `gantt`).
- Each module includes a parser, a renderer, and optional Flex integration.

## Build

This project uses CMake. The top-level build already adds `charts/mermaid`.
Build the full project as usual.

## Flowchart component (Flex)

Component name: `flowchart`

Props:

- `code` (string): Mermaid flowchart source.
- `width` (number): Layout box width. Default `500`.
- `height` (number): Layout box height. Default `500`.
- `fontSize` (number): Node text size. Default `14`.
- `fontFamily` (string): Node font family. Default `"Segoe UI"`.
- `nodePadX` (number): Horizontal padding added to measured text. Default `20`.
- `nodePadY` (number): Vertical padding added to measured text. Default `12`.
- `routingMode` (string): `orthogonal` or `polyline`. Default `orthogonal`.
- `routingShapeBuffer` (number): Extra space around nodes for routing. Default unset.
- `routingNudgingDistance` (number): Nudging distance for route separation. Default unset.
- `routingSegmentPenalty` (number): Penalize extra segments (orthogonal). Default unset.
- `routingAnglePenalty` (number): Penalize sharp bends (polyline). Default unset.
- `routingCrossingPenalty` (number): Penalize edge crossings. Default unset.
- `routingNudgeOrthogonalEnds` (bool): Nudge orthogonal segments at shapes. Default unset.
- `routingNudgeSharedPaths` (bool): Nudge shared paths with common endpoints. Default unset.

Example:

```flex
flowchart chart {
  x: 40, y: 40
  width: 640, height: 480
  fontSize: 16
  fontFamily: "Segoe UI"
  nodePadX: 24
  nodePadY: 14
  routingMode: "orthogonal"
  routingShapeBuffer: 6
  routingNudgingDistance: 4
  routingSegmentPenalty: 12
  routingCrossingPenalty: 2
  routingNudgeOrthogonalEnds: true
  routingNudgeSharedPaths: true
  code: "flowchart TD\nA[Start]-->B{Check}\nB-->|Yes|C[OK]\nB-->|No|D[Retry]"
}
```

## Layout modes

Flowchart layout supports two modes:

- `professional` (default): layered layout with edge routing.
- `legacy`: simple layer layout.

The mode is stored in the flowchart AST. Use the C API to set it if needed:

```c
flowchart_set_layout_mode(diagram, FC_LAYOUT_LEGACY);
```

## Flowchart SVG link styles

Both `legacy` and `professional` renderers resolve numeric `linkStyle` by
zero-based AST edge declaration order. For `A --> B; B --> C; C --> D`,
`linkStyle 1 stroke:greenyellow,stroke-width:2px` styles B→C and its arrowhead.
Parallel edges with identical endpoints retain separate declaration indices.

The initial SVG style profile supports CSS named colors, hex colors with
3/4/6/8 digits, `none`, `transparent`, and `currentColor` for `stroke`;
finite nonnegative unitless or `px` values for `stroke-width`; and `fill:none`.
Absent properties use `RenderOptions`. The final `default` declaration applies
under the final indexed declaration, even if the default appears later in the
source. Repeating an indexed rule replaces its previous property list as a
whole, matching the pinned Mermaid 11.12.0 baseline. Within one rule, later
properties override earlier ones.

Unsupported effective properties, colors, width units and invalid/out-of-range
indices cause `FlowchartRenderer::layout` to throw `std::invalid_argument`.
For example, opacity, dash patterns, paint URLs and `rgb(...)` are outside this
initial profile. Flex catches layout exceptions and reports render failure.
Parsed named-edge curves still do not alter SVG routing; #238 tracks that
remaining work. The legacy JSON projection is unchanged.

`RenderedEdge` owns its resolved stroke/width and declaration index, so SVG
export does not require the parsed diagram to remain alive. This extends the
C++ snapshot layout; rebuild C++ consumers together. Public C AST layouts are
unchanged. Snapshot dimensions default to zero so SVG export computes bounds
when the caller has not supplied explicit dimensions.

The focused installed-SDK test lane enables `STUN_CI_JSON_GOLDEN=ON` and
`STUN_CI_RENDERER_STYLE=ON` in `flowchart/tests/standalone`. It builds the real
parser, renderer, Mustache template and native routing adapter, checks both
layout modes, and independently parses the emitted SVG with Python's XML
parser. It shares the existing fast runtime job without a window or Mesa build.
