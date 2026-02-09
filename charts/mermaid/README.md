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
