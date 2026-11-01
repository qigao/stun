# Interactive GraphCanvas + Stun GraphLayout: canonical design (proposal)

> **Status:** Design only, 2026-11-01. No GraphCanvas / incremental solver implementation is claimed.
> **Tracking:** [GraphCanvas #378](https://github.com/qigao/stun/issues/378) (end-to-end);
> [GraphLayout #161](https://github.com/qigao/stun/issues/161) (algorithms only);
> [FlexUI native Widget #375](https://github.com/qigao/stun/issues/375) (host/SDK/plugin ABI).
> Linked native Widget [Draft #376](https://github.com/qigao/stun/pull/376) and
> installed Core/WidgetSDK [Draft #377](https://github.com/qigao/stun/pull/377)
> are dependencies under qualification, **not master features**.

## 1. Problem / decisions

A DOT/Mermaid renderer computes a static diagram. A Node Editor must preserve a
user's *document*, intent, graph geometry, current tool and undo history while
nodes, ports, edges and groups change interactively. Recomputing the entire
layout on every pointer movement destroys manually assigned positions and
frequently breaks visual continuity.

**Decisions:**

1. Keep the existing **one** `stun::graphlayout` C++17 algorithm kernel. It
   knows only graphs, geometry, constraints and routes. No UI, FlexUI, ImGui,
   gCanvas, Qt, scripting, PluginHost or duplicate engine dependencies.
2. Use FlexUI's existing **Box / Element / WidgetRegistry / EventDispatcher /
   RenderCommandList** and, once qualified, its linked `NativeCanvasWidget`
   adapter. No independent event, styling or drawing tree.
3. Keep graph document, typed ports, selection, edit history and user layout
   outside the Box in an application-owned GraphEdit model. The Box contains
   the corresponding UI semantic Elements; this is not a second UI tree.
4. Manual editing is the default. Assisted/local rearrangement and full
   automatic layout are **explicit commands**, not implicit side effects.
5. Treat a drag as reversible preview plus one atomic document transaction.
   Route failures are typed failures, **not** a straight-line fallback.
6. Match resource bounds and upstream semantics exactly: neither the C++17
   algorithms nor their current input size limits imply large interactive
   editor readiness. No automatic routing "best effort" on failure.

## 2. Current actual reusable code

| Source and target | Accepted capability | Important boundary |
| --- | --- | --- |
| `shared/layout/include/stun/graphlayout/graph.h` — `Stun::GraphCore` | stable node string ID and caller-order `Graph` / `Layout` | `Edge` is indexed by node position, has **no persistent edge ID, socket ID or editing history** |
| `layered.h` — `Stun::GraphLayered` | deterministic SCC / layered placement | stateless whole-graph layout, `Options::max_nodes=4096` |
| `projection.h`, `vpsc.h` — `Stun::GraphProjection`, `GraphVPSC` | exact hard pins; alignments; separations; collision avoidance; bounded verification | batch solver, no persistent edit-session cache |
| `orthogonal.h`, `polyline.h` — `GraphOrthogonal`, `GraphPolyline` | rectangle-obstacle paths and validated exact terminal anchors, no hidden fallback | default `max_nodes=256`, `max_routes=1024`; routed edges are returned in request order |
| `port_bindings.h` — `GraphPortBindings` | measured named ports → exact boundary side and fractional offset | rejects synthetic/interior/unmeasured sockets |
| `nudging.h` / `polyline_nudging.h` | bounded multi-edge local crossing/overlap improvement | not globally crossing-optimal or generally incremental |
| `topology.h`, `topology_edits.h` | conservative signed crossing audit, optional explicit bend edits | not a universal drag gate: self-loops, ambiguous/shared paths and compound obstacles are not universally supported |
| `charts/dotgraph/src/dotgraph_layout.cpp`, `flexui_dotgraph_snapshot.cpp` | parsed DOT → GraphLayout → immutable Box-owned visual snapshot | not a mutable editor; do **not** mutate/render the parser AST as editor state |

Original GraphLayout scope remains independently documented in
[its README](../../shared/layout/README.md) and tracked by #161.
No forced `Stun::GraphLayout` monolith or solver/provider selector.

## 3. Ownership and data flow

```text
Application document (the only authoritative graph/document state)
   GraphEditSnapshot + typed commands + Undo/Redo journal
     | immutable input revision and stable IDs
     v
GraphEdit adapter/controller (UI-neutral data/geometry bridge, NOT a solver)
   stable IDs -> GraphCore node indices and RouteRequest ordering
   edit delta -> affected nodes/obstacles/routes; route cache/validation
     | explicitly selects existing bounded algorithm
     v
Stun GraphLayout (Layered / Projection / VPSC / Routing / Topology, as needed)
     | validated geometry tagged with input revision
     v
GraphCanvas (FlexUI Widget, Box-owned semantic parts, viewport transform)
   one EventDispatcher / pointer capture / focus / RenderCommandList
     | host's existing backend
     v
FlexUI renderer -> gCanvas / native backend
```

**No second graph execution runtime:** business compute graphs, Orbit actors and
TurboMedia processing graphs are not this document's graph geometry model.
The GraphEdit adapter should initially live alongside the real Charts/FlexUI
graph editor integration, **not** inside the renderer-independent algorithm
targets. Move only proven reusable pure geometry work into `shared/layout`.

### 3.1 Proposed contracts (pseudocode — NOT installed headers or ABI)

```cpp
using NodeId = std::string;  // canonical, validated and unique per document
using EdgeId = std::string;  // stable across reorder/insert/delete
using PortId = std::string;  // unique within the owning node
using Revision = std::uint64_t;

enum class EditMode { Manual, Assisted, Auto };
enum class EditOp { MoveNodes, ResizeNode, ConnectPorts, DeleteEdge,
                    ChangePort, ChangeGroup, CollapseGroup, ApplyAutoLayout };

struct GraphEditSnapshot {
    Revision document_revision;
    Revision geometry_revision;
    std::vector<EditorNode> nodes; // IDs, dimensions, user world positions
    std::vector<EditorPort> ports; // measured local anchors and explicit side
    std::vector<EditorEdge> edges; // EdgeId + NodeId + PortId endpoint pair
    std::vector<EditorGroup> groups;
    std::vector<CommittedRoute> routes; // keyed by EdgeId
};

struct GraphEditDelta {
    Revision expected_document_revision;
    EditOp operation;
    std::vector<NodeId> changed_nodes;
    std::vector<PortId> changed_ports; // owning node identity also required
    std::vector<EdgeId> changed_edges;
    std::vector<WorldRect> old_and_new_obstacle_bounds;
};

struct GeometryCandidate {
    Revision input_document_revision;
    Revision input_geometry_revision;
    std::uint64_t mount_generation;
    std::vector<NodePlacement> changed_positions;
    std::vector<EdgeRoute> changed_routes;
    DirtySet affected; // stable IDs; no pointer to Box/Widget/Element
    GeometryStatus status; // capacity/no path/invalid ports/etc.
};
```

These are conceptual **application adapter** types; they do not change
`GraphCore::Edge` or the current `RouteRequest` C++ ABI. The adapter
constructs a stable ID→index table for each immutable input snapshot and
carries an `EdgeId`→route-request-index table back into the result. Sorting
and all tie breaks use stable IDs, **not** unordered container iteration.
A duplicate/renamed ID must fail or be an explicit document command.

The document revision changes on every committed document mutation. Geometry
revision changes on committed route/placement changes; viewport pan/zoom is
an independent view revision. The mounted widget has a separate generation
to reject callbacks and async completions from retired/replaced Box scopes.
Never store raw `Element*`, `Widget*` or graphics pointers in worker input
or in the document.

## 4. Interaction and view policy

**Manual** (default): drag selected nodes, attach measured ports, move handles,
modify group memberships; all unrelated node rectangles stay **exactly**
where the author placed them. No auto-layout on each event.

**Assisted** (explicit user option/action): calculate a bounded local
`project_graph` candidate with exact hard pins for protected/unselected nodes;
apply screen-pixel snap guides translated into world geometry at the current
zoom. If constraints are infeasible, show a typed failure, never silently
relax a pin.

**Auto** (explicit command): layout a selected component or the whole graph
through `layout_layered`, `layout_force` or `layout_stress_*` selected by
the document/controller policy. The chosen algorithm and options are recorded
with the command; store accepted placements as a single undoable edit.
Existing Force/Stress defaults stop at 128 nodes; do not override that without
new independent acceptance.

### 4.1 Gesture and transaction

```text
Idle -> PointerDown -> (threshold) DragPreview -> Commit
             |                    |               |
             +--click/selection   +--Escape-------+--> Cancel
                                  +--focus loss---+--> Cancel
                                  +--unmount------+--> Cancel
```

One host EventDispatcher owns input/capture; ToolManager state determines
whether the **captured interaction**, an exact hit-tested handle, the active
tool, or canvas navigation receives the event. Input handling cannot
independently create a second capture mechanism. Only the UI owner thread
may commit the application's document; render callbacks never mutate it.

Gesture previews are transient and may show a rubber-band or temporary
unvalidated connector, **clearly marked as preview**. They must never be
reported as validated committed routes. At MouseUp, prepare candidate node
positions and routes, validate geometry, and publish the complete document
transaction; otherwise restore the prior committed geometry, clear capture
and show `NoPath` / `CapacityExceeded` / `InvalidPort` / `Conflict`.
An explicitly supported user-authored dangling edge (future) would have to
be a separate persisted edge state, not an implicit fallback.

Undo/Redo consumes/replays whole edit transactions, including geometry and
edge/port mutations. A failed transaction does not append history. Repeated
drag previews should not create multiple Undo steps. End-of-gesture,
Escape, focus-out, document replacement and mount retirement are distinct
required cancellation cases.

### 4.2 Coordinates and semantics

- Document positions/sizes and route points: finite **double** world units.
- Rendering: map world to Widget-local CSS/device-independent coordinates;
  the Box applies its own Element transform; do not double-apply transforms.
- Mouse: existing `Event` window coordinates → host Element-local →
  inverse view transform → world. Recalculate hit thresholds from **screen
  pixels** at each zoom/DPI, so a 7-pixel socket handle stays 7 pixels.
- One invertible pan/zoom transform per GraphCanvas; reject singular,
  nonfinite or outside-bounds transforms. Zoom around the pointer.
- Graph objects needing CSS, focus, selection, inspection or accessibility
  retain stable **Box-owned Element/semantic** identities. Connector strokes,
  grid and selection previews can remain atomic RenderCommands. Any future
  virtualization must preserve equivalent semantic focus and accessible
  identity; it must **not** create a second DOM/tree.
- No direct ImDrawList/ImGui Context/Qt QObject or GPU resources across
  Widget boundaries; the dynamic DLL Widget ABI is independently tracked by
  #375 M2/M3/M4.

## 5. Delta invalidation and bounded incremental routing (M1)

**Central correctness trap:** after moving node A, not only edges incident to
A need attention. A may enter a region previously crossed by unrelated edge
B→C. A candidate valid before the edit may be invalid afterward.

1. Take immutable baseline `(document_revision, geometry_revision)` and
   candidate deltas. Compute swept union of changed nodes' old/new rectangles,
   including routing clearance and changed port-stub bounds.
2. Start dirty edges with all incident edges, changed endpoint ports, explicitly
   edited checkpoints and group-boundary edges.
3. Query an index of **all committed route segments** intersecting that swept
   region (spatial index when justified, bounded scan first). Add these edges
   even when neither endpoint changed.
4. Revalidate the dirty set against the **new complete set of rectangles and
   measured terminals**; regenerate routes using an explicitly selected
   existing `route_orthogonal` or `route_polyline` algorithm. A route cache
   key includes endpoint identity, port geometry, clearance/routing options
   and an obstacle/geometry revision or independently validated dependency
   fingerprint. A different node/obstacle cannot reuse stale cached routes.
5. New routes can intersect committed old routes or introduce new lane/crossing
   interactions. If the selected quality policy requires joint interaction
   constraints, expand the dirty closure and recompute to a **bounded fixed
   point**. There is no universal guarantee of global crossing minimality.
6. Rebuild indices only after all candidate nodes/routes pass independent
   validation. Publish atomically under matching input revisions. Keep
   unaffected node coordinates exact; verify non-dirty routes remain valid.
7. On limit/no path/invalid input, preserve the old committed document and
   return a typed status; never publish half-updated positions/routes, and
   never turn a rejected candidate into a quiet straight-line path.

Initial M0 can recompute the entire small graph at commit when necessary:
**correctness first**. M1 should introduce local invalidation only after
a deterministic equivalence/correctness corpus and recorded full-recompute
baseline. `route_orthogonal` default `max_nodes=256` and
`max_routes=1024` are hard resource contracts; a "local" call that ignores
distant obstacles is not a valid way around those limits.

## 6. Async, cancellation, snapshots and performance

A background graph worker receives only immutable document+geometry snapshots,
typed numeric constraints and a cancellation token. It must never read a live
Box tree or render resource. Every result includes document revision, geometry
revision, operation ID and Box mount generation. The UI owner accepts it only
if **all** fields still match the current pending operation; otherwise it
discards the stale result and retains current geometry. Cancellation joins
before owner retirement and must not race a late completion. The expensive
solver is **never** run synchronously per mouse movement.

Benchmarks run in an **existing real** GraphLayout/GraphCanvas build and
real GUI integration. A first measurement matrix:

| Scale | Density / scenario | Record |
| --- | --- | --- |
| 100 nodes | sparse and dense edges; drag, connect, zoom | frame p50/p95, solver time, rerouted edges, non-dirty position drift, allocations |
| 500 nodes | same + obstacle sweep across unrelated edge | baseline performance **and typed CapacityExceeded where current limits apply** |
| 1,000 nodes | same + repeated undo/redo and cancellation | baseline + budget failures, until independent scaling is qualified |

Frame rate targets (e.g. ~16.7 ms for 60 Hz) are **product goals**, not an
unmeasured pass claim. Keep machine/toolchain/fixture constant for comparable
reports; CPU budgets, sampled snapshots, cancellation latency, dirty-set size,
spatial index cost and memory peaks are all observable. Do not mask large-graph
failure by changing the declared solver options without evidence.

## 7. Groups, subgraphs and graph-specific semantics (M2)

- Model `GroupId`, nesting/parent, child membership, parent-local transforms,
  order, collapsed state and proxy ports as **document data**.
- Group operations preserve stable Node/Edge IDs and old geometry in Undo
  history. Collapse is an explicit graph-view transformation: compute actual
  proxy dimensions and explicit external-port mapping; reroute connections
  via measured boundary ports. Expand restores the original nodes' identities.
- Distinguish **editor-only visual grouping** from a genuine compound obstacle
  in the solver. Existing rectangle routing does not certify arbitrary shaped
  or compound obstacles; unsupported cases return typed errors until #161
  accepts an independently tested new pure algorithm.
- Edge model supports parallel edges, explicit port identity, and self-loops
  at the **document level**. Existing routing may handle some loops, but the
  topology audit explicitly does not; do not apply it to every user edit.

## 8. Milestones and acceptance ownership

| Milestone / tracker | Owner / code location | Minimum objective evidence |
| --- | --- | --- |
| **M0**: Manual GraphCanvas + ToolInteraction | #378 / existing Charts/FlexUI Widget integration; one Box | real native window pointer capture, move/select, pan+zoom, connection, Escape/focus-out cancellation, one Undo step, independent graph instances |
| **M1**: incremental route invalidation | #378 / editor adapter + independent GraphLayout validators | swept obstacle invalidates unrelated edges, exact measured ports, stable route IDs, correctness against complete reroute, typed rollback, stale-result cancellation |
| **M2**: subgraph / groups | #378; pure new compound primitives only in #161 | collapse/expand with external routes and no lost IDs, Undo/Redo, nested groups, explicit unsupported obstacle status |
| **M3**: scaling / optional dense GPU | #378; #161 for algorithmic scale; #375 for external GPU lease | 100/500/1,000 size + density baseline, p50/p95, capacity failures, memory, consistency; no GPU success claim until independent fence/texture gates |
| Widget SDK / dynamic PluginHost | #375, #31, #298 | separate exact-head CI, installed SDK real downstream, plugin unload/capture leases — **not** inferred from a GraphCanvas demo |

### Required regression corpus

1. Two separate canvases: one drag or cancel never leaks selection/capture
   to the other; mount retirement cancels gestures.
2. Stable ID ordering: node/edge insertion and hash-map permutation produce
   identical candidate geometry for the same stable input IDs.
3. Drag node A over unrelated edge B→C: B→C is invalidated and independently
   validated or edited move is rejected; no silent collision.
4. Pin A + align B/C + avoid rectangles: exact pinned coordinates or typed
   infeasibility; no motion of uninvolved nodes.
5. Port move/resize, parallel links, self-loops, zero-width/NaN geometry,
   duplicate IDs/ports, no path, budget exhaustion: explicitly qualified
   success or typed failure without corrupting the document.
6. Undo/Redo restores node, port, route, group and selection model; stale
   async solver completion never resurrects an old generation.
7. Real GUI screenshot with causal mouse interaction + headless
   independent geometry validator and sanitizer evidence on the **exact
   candidate HEAD**.
8. Assert that an untouched graph retains bytewise-identical positions and
   routes, and that no source, CMake or dependency adds a second layout engine.

## 9. Non-goals / policy

- No additional ImGui or Qt renderer, widget hierarchy, plugin host,
  business graph evaluator, or graphics pipeline.
- No vendored Adaptagrams/ELK source or claims of Cola/ELK functional parity.
- No C11/DSO Widget pointer export: C11 plugin ABI is a separate explicit
  versioned contract after #375 host leases are accepted.
- No fake installer/consumer project, probe CI/CMake, parallel synthetic
  search engine, unconditional MSVC requirement, compiler-version gate, or
  Lean proof gate.
- Do not merge Draft #376/#377 based only on producer tests; cross-platform
  and real installed downstream are independent qualifications.

## 10. Behavioral references only (no source import)

- [Azzinoth VisualNodeSystem](https://github.com/Azzinoth/VisualNodeSystem):
  nested subareas, reroute sockets, group comments and graph tool behaviors.
- [Poietic Playground](https://github.com/openpoiesis/poietic-playground):
  distinct drag recognizer, captured-tool → grip → active-tool → navigation
  priority, editable document transactions and previews.
- [imgui-node-editor](https://github.com/thedmd/imgui-node-editor):
  lifetime, interaction, deletion, route and draw-order regression style.
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo):
  minimal `BeginEdit/EndEdit` delegates, graph/curve/sequencer idioms.
- [EdgeDepth Terminal](https://github.com/edgedepthhq/edgedepth-terminal):
  coherent per-frame data snapshot, bounded owner-thread drain and tool
  input arbitration. Its AGPLv3 source is **not** being imported.

**Source of truth:** this document describes decisions and qualification
criteria. The live implementation state is tracked by issues and Draft PRs
above; a checked-in proposal is not implementation evidence.
