# Stun GraphLayout — self-owned layout kernel

The Stun GraphLayout modules provide renderer-independent C++17 graph
**placement, routing and optimization** algorithms, separately linked by chart.
They are not widgets, docking, rendering or UI layout systems. These algorithms
build independently of Salts, FlexUI, Cairo, OpenGL and the Adaptagrams vendor library.

## Scope: Adaptagrams-like layout algorithms only (2026-10-10)

This development branch is intentionally limited to independently implemented
C++ algorithms with similar capabilities to the five Adaptagrams libraries:
`libvpsc`, `libcola`, `libavoid`, `libtopology` and `libdialect`.
The upstream repository is a **behavioral reference**, not a source-code
dependency or an API to copy. The internal `libproject` feasibility techniques
are in scope only when needed to support Cola-style constrained placement.

| Upstream capability | Native algorithm status | Remaining work |
| --- | --- | --- |
| `libvpsc` | Weighted VPSC and pinned/2D projection exist | Coupled-constraint performance, richer separation/cluster cases |
| `libcola` | Gradient Stress, SMACOF, spring/repulsion exist | Quality/scale and constrained graph-layout parity |
| `libavoid` | Orthogonal and Polyline A*; bounded orthogonal joint Nudging exists | Global crossing/nudging, ports/checkpoints |
| `libtopology` | Baseline signed-crossing audit and sampled bounded displacement | Continuous topology guarantees, arbitrary connector routing and compound constraints |
| `libdialect` | Tidy Tree is a partial building block | Decompose, Arrange, Expand/Emend, Transform pipeline |

**Not in scope:** Lean/mathlib/formal proofs, UI/layout widgets, OpenGL
rendering, chart-specific Sankey/Gantt/Sequence features, and a broad chart
framework rewrite. C++ unit/property tests, runtime invariants, sanitizers,
cross-platform CI, bounded resources and Adaptagrams comparison are required.
Keep the PR Draft / **DO NOT MERGE** until tested algorithm acceptance is complete.

## Independent algorithm modules

The project deliberately builds algorithms as **separate targets**, not as
copies of algorithms under individual chart frontends. Each chart owns its
AST-to-Graph IR adapter, its layout-policy selection and its output mapping;
shared solvers have no knowledge of DOT, Mermaid, Infographic or rendering.

| Target | Provides | Requires |
| --- | --- | --- |
| `Stun::GraphCore` | Stable Graph/Node/Edge/Layout IR (`graph.h`) | C++17 only |
| `Stun::GraphLayered` | SCC/DAG ranks and placement | GraphCore |
| `Stun::GraphTidyTree` | Bounded ordered-forest contours, variable-size nodes | GraphCore |
| `Stun::GraphOrthogonal` | Orthogonal obstacle-aware A* | GraphCore |
| `Stun::GraphPortBindings` | Bind measured named rectangle ports to exact side/offset | GraphCore |
| `Stun::GraphNudging` | Bounded multi-edge orthogonal lane offsets & pairwise interaction audit | GraphOrthogonal |
| `Stun::GraphVPSC` | 1D separation constraints | GraphCore |
| `Stun::GraphProjection` | 2D pins, alignments, non-overlap | GraphVPSC |
| `Stun::GraphStressCore` | Shared shortest-path pair construction, objective evaluation and projected backtracking | GraphProjection |
| `Stun::GraphStressGradient` | Standalone Stress gradient descent | GraphStressCore |
| `Stun::GraphStressSmacof` | Standalone SMACOF Laplacian majorization | GraphStressCore |
| `Stun::GraphForce` | Spring plus softened repulsion | GraphProjection |

The **algorithm code and CMake targets** above are independently built and
linked. Gradient and SMACOF are now separate object implementations with two
explicit public entry points. They share exactly one shortest-path Stress
objective in `GraphStressCore`, and neither links the other. No runtime
optimizer selector, provider fallback, or duplicate distance/energy kernel.

Chart adapter dependencies remain explicit: DOT and Mermaid flowcharts
consume Layered and Orthogonal; Infographic's collision projection consumes
GraphProjection. The former explicit Polyline/libavoid option is still a
separately selected provider, **not an automatic fallback**. This phase is scoped to Adaptagrams-like graph layout only: chart-specific
Sankey, lane/timeline, Gantt, and other unrelated algorithms are excluded.
Existing Tidy Tree is retained solely as a useful building block for
DiAlEcT/HOLA-style decomposition; it is not a new chart-feature workstream.

To qualify an individual algorithm without any UI/chart dependencies:

```sh
cmake -S charts/graphlayout -B build/graphlayout -DBUILD_TESTING=ON
cmake --build build/graphlayout --target stun_graph_vpsc
cmake --build build/graphlayout --target stun_graph_force
ctest --test-dir build/graphlayout --output-on-failure
```

## Algorithm v1: layered graph placement

1. Validate bounded graph input, stable node IDs, finite positive sizes and options.
2. Deduplicate adjacency and partition the graph into strongly connected
   components (iterative Kosaraju). Self loops stay internal to their SCC.
3. Build the condensation DAG. Compute longest-path ranks using deterministic
   Kahn processing. Only **inter-SCC** directed edges are guaranteed to advance
   in rank; a directed cycle cannot satisfy that property for every edge.
4. Partition weakly connected components. Within each component, repeatedly
   order nodes by barycenters of their neighbours in adjacent ranks, with
   stable-ID tie ordering. This is a heuristic, **not** optimal crossing minimization.
5. Assign non-overlapping variable-size rectangles by layer; center narrower
   layers; separate components; transform TB/BT/LR/RL directions.
6. Validate finite placements and the SCC/DAG layering contract. Errors are
   returned explicitly; no fallback, partial layout, random jitter or silent
   constraint relaxation is allowed.

**Output**: positions, sizes, ranks, SCC IDs, and overall extent in original
input-index order. Edges, routing geometry, clusters and pin constraints are
not part of the v1 placement output. They will be separate graph-algorithm
stages. The renderer owns neither the algorithm nor its node identities.

## Verification obligations

- Every input index has exactly one placement; geometry is finite and
  non-negative, and bounding rectangles do not overlap.
- All edges crossing SCC boundaries strictly advance in rank.
- The same graph, configuration and IDs give the same result, regardless
  of node/edge insertion order.
- Unsupported or invalid inputs return explicit failure with no partial output.
- Rank ordering and rectangle separation are validated with deterministic C++
  tests and runtime geometry checks; formal proofs are not in this project scope.

## Standalone qualification

```sh
cmake -S charts/graphlayout -B build/graphlayout -G Ninja -DBUILD_TESTING=ON
cmake --build build/graphlayout --parallel
ctest --test-dir build/graphlayout --output-on-failure
```

## Algorithm v2: orthogonal obstacle-aware routing

`stun/graphlayout/orthogonal.h` supplies a separately checked, self-owned
route generator. A route is a sequence of axis-aligned points from exact
rectangle-side port anchors, in input edge order.

1. Check all finite obstacle rectangles and route endpoint fractions. Reject
   overlapping node geometry and over-capacity inputs explicitly.
2. Expand rectangles by the configured clearance; construct a compressed
   rectilinear visibility grid from obstacle boundaries and the two port stubs.
   Points inside expanded obstacles and segments crossing their interior are
   inadmissible. Port stubs cross only their own clearance envelope.
3. Run direction-aware A* over `(grid_vertex, arrival_direction)` with
   nonnegative `Manhattan length + bend_penalty` cost and a Manhattan admissible
   heuristic. Resolve equal candidates by stable grid/state indices.
4. Reconstruct a polyline, remove duplicate/collinear points, and validate the
   full result, including exterior port directions, nonzero orthogonal steps,
   original terminal rectangles, and expanded non-terminal obstacles.
5. On any failed route, clear the entire batch and return a typed failure and
   edge index. There is **no straight-segment fallback**.

The module currently optimizes **individual routes**, not a joint global
crossing/nudging objective. It supports `Auto/North/East/South/West` ports and
fractional offsets, with a fixed convention for self-loops. Dense graphs may
hit `max_grid_vertices`, `max_expansions`, or `max_queue_entries`: those are
explicit resource errors, not evidence that the geometric problem has no path.

The geometric validator checks emitted routes under floating-point tolerances;
its checks do not establish all possible routing cases. Additional obligations include
joint-edge crossing minimization, compound obstacles, exact shape intersection,
and rigorous numerical error bounds. Algorithms run in C++17 and have no
third-party graph layout source or runtime dependency.

## Algorithm v3: weighted VPSC (convex separation projection)

`stun/graphlayout/vpsc.h` implements Stun-owned, renderer-independent
**Variable Placement with Separation Constraints**. It solves

`min 0.5 * sum_{i not fixed} weight[i] * (x[i] - desired[i])^2`

subject to `x[right] - x[left] >= gap`, or equality when explicitly selected.
Every weight must be finite and strictly positive. A variable with `fixed=true` has exactly `x[i] = desired[i]`; its weight is not used in the movable objective. Negative gaps are valid.
Input/output indices are preserved and stable IDs establish a deterministic
processing order independent of node insertion order.

- Difference-constraint Bellman-Ford feasibility checking detects strictly
  positive separation cycles, returning a checkable ordered set of directed
  witness arcs (including the reversed orientation for equality and fixed-variable constraints).
  Cycles indistinguishable from numerical roundoff return `InvalidNumerics`,
  **not** an unsupported assertion of mathematical infeasibility.
- Bounded dual coordinate ascent performs exact coordinate updates for the
  convex weighted objective, with `lambda >= 0` for inequalities and signed
  multipliers for equalities. It does **not** copy Adaptagrams block code.
- An independent checker recomputes primal separation violations, dual
  sign violations, KKT stationarity, complementarity and a dual lower bound.
  The solver returns success only within an explicit **normalized floating-point
  tolerance**. A successful certificate is numerical evidence, not an exact
  proof or a claim that C++ floating point arithmetic is formally verified.
- Hard limits bound feasibility relaxations, coordinate updates, iteration
  sweeps and input counts. On exhaustion, return `IterationLimit`/
  `CapacityExceeded`, clearing the entire output. Infeasible or unstable
  cases are also explicit errors; never silently relax constraints or use a
  fallback solver. This algorithm can converge slowly on tightly coupled or
  degenerate constraints: no unconditional iteration or speed bound is claimed.

The VPSC solver is **not yet wired into DOT/Mermaid's node placement**. The
full Cola stress minimization, geometry-dependent overlap-disjunction search,
cluster hierarchy, topology preservation and joint-edge nudging remain separate
future graph algorithms. Numerical KKT residuals, positive-cycle witness checks,
scale tests and upstream differential comparison are the acceptance strategy.

## Algorithm v4: constraint-aware 2D graph projection

`stun/graphlayout/projection.h` composes **two independently certified VPSC
axis solves** into a deterministic graph geometry stage. It acts on a previously
placed `Graph` plus `Layout`; this is not a UI layout, force-directed stress
minimizer, or an implicit alternative to the Layered algorithm.

- **Hard node pins:** exact absolute top-left `(x,y)` constraints. Pinned
  variables are excluded from the movable-variable quadratic objective and
  cannot drift; the 1D KKT checker verifies each pin exactly, rather than
  simulating pins with very high weights. An infeasibility witness may refer
  to a fixed variable (`fixed_variable=true`) as well as original constraints.
- **Axis alignment:** exact signed difference between the centers of any
  two nodes. Explicit horizontal or vertical rectangle separation uses
  an edge-to-edge minimum gap, correctly accounting for variable dimensions.
- **Automatic non-overlap:** detect pairs whose axis-aligned rectangles
  intersect at configured `clearance`, choose the cheaper X or Y separation
  direction, and monotonically add inequalities. Transitive equality groups
  and pins are inspected to avoid selecting an obviously impossible axis.
  A numerical guard protects the geometric postcondition at the VPSC
  solver's floating-point tolerance. Newly created overlaps trigger another
  pass; all pairwise geometry is checked again before returning success.
- **Limits and error semantics:** `max_passes`,
  `max_generated_separations`, `max_pair_checks` and both axis solvers'
  VPSC budgets are explicit. Failure clears the output and report without
  returning partial coordinates or silently relaxing constraints.
  Output preserves node order/IDs, dimensions, ranks and SCC metadata, and
  may legitimately contain negative coordinates when a pin requires it.

The axis choice is a bounded **disjunctive heuristic**, not a complete search
of all feasible non-overlap axis assignments. A viable graph may still be
reported as incomplete or infeasible when the chosen constraints conflict.
Unlike geometric validity, globally minimal displacement is proved only for
**each fixed set of VPSC axis constraints** to numerical tolerance, and does
not imply globally optimal joint 2D compaction. Native routing/placement
composition and additional geometric regression tests remain acceptance tasks.

## First consumer: Infographic collision projection

`charts/infographic/src/layout/layout_engine.cpp` previously constructed an
empty-edge `cola::ConstrainedFDLayout` solely for rectangle collision relief.
It now delegates this narrow geometry-only purpose to `project_graph()` with
an explicit one-unit clearance. The rest of Infographic's layouts, renderer,
SVG and FlexUI remain separately owned. There is no hidden Cola fallback.
Headless `StunGraphInfographicAdapterTests` compile real Infographic layout
sources and check node containment, non-overlap and multi-level parent-index
mapping in the GraphLayout CMake/CTest matrix. This is **not** a complete
charts-enabled build/link/runtime qualification, nor a replacement for
Adaptagrams' force-directed or topology-preserving algorithms.

## Algorithm v5: graph-distance stress descent with optional VPSC projection

`stun/graphlayout/stress.h` provides two separately compiled Stun-owned
**nonconvex stress** optimizers on node centers. For each pair reachable in the *undirected*
interpretation of the graph, let `h(i,j)` be the unweighted shortest-path hop
count and `L` the configured ideal edge length. The objective is:

`F = 0.5 * sum_{reachable i<j} (||center_i-center_j|| - L*h(i,j))^2 / h(i,j)^2`

Disconnected node pairs have zero stress weight. Self-loops and duplicate
edges do not introduce duplicate terms. The Gradient algorithm is **not** claiming SMACOF,
global optimality or a general spring-electric force model. It starts from a
caller-supplied finite `Layout`, accumulates gradients in lexical stable-ID
order, and uses a bounded backtracking line search on the objective. The
objective is recomputed after each candidate's constraint projection; **only
strictly decreasing candidates** are accepted. `accepted_objectives` contains
the post-projection initial energy and every accepted energy, allowing an
independent monotonicity audit. `evaluate_stress` re-evaluates the exposed
result from the original graph and positions.

- **Hard geometry:** if the caller requests pins, center alignments, minimum
  separation or non-overlap, every trial goes through `project_graph()`, i.e.
  the already certified VPSC axis solvers. The initial seed is projected before
  its baseline energy is measured. The projection's non-overlap axis selection
  remains heuristic, so projected line search is **not** proof of constrained
  stationarity or globally minimum stress. Metadata, node dimensions and
  original indices are preserved.
- **Deterministic failure:** coincident connected node centers without
  sufficient projection are an explicit `InvalidGeometry` error. No pseudo-
  random displacement, Cola fallback or partial-result success is allowed.
  Failed initial projection returns `ProjectionFailed` and the exact
  `ProjectionError` subtype.
- **Bounded work:** configurable limits on nodes, edges, shortest-path BFS
  operations, reachable pairs, optimization iterations and line-search trials.
  Exhausted line searches return the best feasible accepted iterate with
  `LineSearchStalled`; an exhausted outer limit returns `IterationBudget`.
  Neither termination code is mislabeled as global optimality. The finite
  nonincreasing energy trace is a numerical postcondition, not a claim of global optimality.
- **Scope:** node-center stress only. It does not yet solve joint edge routing,
  component packing, force-directed Coulomb repulsion, compound constraints,
  or full Cola/Adaptagrams feature equivalence. The separately selected
  SMACOF and Force algorithms are documented below; incremental/large-graph
  acceleration and Adaptagrams differential quality tests remain future work.

The dedicated `StunGraphStressTests` cover analytic two-node equilibrium,
multi-hop chains, cycles, disconnected graphs, constrained pins and
non-overlap, permutation stability, invalid geometry, infeasibility,
resource caps, and forty deterministic graph families.

## Algorithm v6: SMACOF graph-distance Stress majorization

The public entry points are `layout_stress_gradient(...)` and
`layout_stress_smacof(...)`, exported by separately linked
`Stun::GraphStressGradient` and `Stun::GraphStressSmacof` targets.
**Neither optimizer falls back to the other** if a bound is exceeded or a
linear solve fails. Both share the independently callable `evaluate_stress`
objective in `GraphStressCore` and minimize the same all-pairs, shortest-path,
weighted node-center objective described in v5.

SMACOF builds a quadratic upper bound at the current centers `Z`:

`Q(X|Z) = 0.5 * Σ_(i,j) w_ij * (||x_i-x_j||² - 2*d_ij * (z_i-z_j)·(x_i-x_j)/||z_i-z_j|| + d_ij²)`.

For noncoincident connected centers, Cauchy–Schwarz proves
`Stress(X) <= Q(X|Z)` and `Stress(Z) = Q(Z|Z)`. Its unconstrained minimizer
solves the weighted Laplacian normal equation
`V X = B(Z)Z`. We **solve for displacements**
`V Δ = (B(Z) - V) Z`, avoiding cancellation from large absolute coordinates.
For each weak component with no hard pins, a deterministic lexical-ID gauge
anchor removes the translation nullspace; the component is subsequently
translated to preserve its centroid. Hard pins are eliminated exactly from
the reduced system, using their current (already projected) coordinate as a
fixed value. Two Jacobi-preconditioned conjugate-gradient solves (X/Y) have an
explicit `max_linear_iterations` per-axis/per-step bound. An exhausted bound
returns `LinearSolveLimit` with empty output, not a best-effort claim of a
completed SMACOF step. Invalid or coincident pair centers also fail explicitly.

**Important distinction:** A `ProjectionConstraints` result is computed by
the existing per-axis VPSC solver. Its Euclidean displacement projection is
not necessarily a minimizer of the Laplacian majorizer. Therefore every
interpolated *projected* candidate is evaluated against the **actual Stress**
objective, and only a strict decrease above `relative_tolerance` is accepted.
The reported `majorizer_improvements` are pre-projection quadratic reductions,
whereas `accepted_objectives` are post-projection actual energies. The latter
must decrease monotonically for *both* algorithms; a line-search stall returns
a finite, auditable iterate with the distinct `MajorizationStalled` status.
No global nonconvex optimum or unconditional decrease from VPSC projection
alone is claimed.

Standalone comparison (optional target; disabled in regular builds):

```sh
cmake -S charts/graphlayout -B build/graphlayout-bench -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSTUN_GRAPHLAYOUT_BUILD_BENCHMARKS=ON
cmake --build build/graphlayout-bench --target stun_graphlayout_stress_compare
build/graphlayout-bench/stun_graphlayout_stress_compare
```

The CSV compares both algorithms on identical deterministic chain, ring and
sparse graphs at equal **outer iteration budgets**, reporting final energy,
actual accepted steps, linear-CG iterations, elapsed time and rectangle overlap
counts. Results are machine- and workload-dependent; SMACOF may reach much
lower energy while doing significantly more work per outer iteration. This
benchmark compares Stun optimizers, **not** upstream Cola/ELK.

## Algorithm v7: bounded spring/electrostatic Force-directed layout

`stun/graphlayout/force.h` adds a **separately selected** C++17 graph-layout
optimizer. It does **not** replace the v5/v6 Stress objective with an implicit
alternative: the user chooses `layout_force()` explicitly. Rendering, UI,
Polyline/Orthogonal routing, and the existing node ID/order ownership contract
are unchanged.

For node centers `c_i` and deduplicated undirected input edges `E`, the
**precisely exposed objective** is

```
E(c) = 0.5*k * sum_(i,j in edges) (||c_i-c_j|| - L)^2
     + alpha*L^3 * sum_(i<j) 1/sqrt(||c_i-c_j||^2 + epsilon^2)
```

Here `L` is the target spring length, `k >= 0` the spring strength,
`alpha >= 0` the (dimensionless) repulsion coefficient, and `epsilon > 0`
the Coulomb softening length. The first term is an **edge** spring, not the
all-shortest-path-pairs Stress used by SMACOF. The second term repels **all
pairs**, including disconnected components; hence those components are no
longer isolated optimizations. Self-loops and reversed/duplicate graph edges
cannot introduce duplicate spring energy.

- **Derivative and monotonicity:** fixed lexical node-ID accumulation order;
  an analytic pair gradient verified by central finite differences in tests;
  bounded backtracking line search; all accepted iterates strictly decrease
  the **actual** total energy after any VPSC projection. An independent
  evaluator recomputes both energy terms and pair counts, and the accepted
  energy trace is emitted for auditing.
- **Constraints:** optional hard pinned nodes, aligned centers, explicit
  rectangular minimum separation and collision clearance are applied using
  the existing 2D VPSC projection at initialization and on each candidate.
  Unlike the unconstrained force step, this projection is **not** a theorem
  of constrained stationarity; strict post-projection energy acceptance is
  explicitly checked.
- **Determinism and work bounds:** `max_nodes` (up to 256), `max_edges`,
  `max_pairs`, `max_pair_evaluations`, `max_iterations` and `max_backtracks`
  are validated and enforced. Pairwise repulsion is **O(n^2)** per energy or
  gradient evaluation; Barnes-Hut/quadtree acceleration is NOT implemented.
  Pinned geometry remains exact when feasible; node left-to-right order is
  **not** an invariant of force-directed optimization.
- **Degeneracy:** `epsilon` keeps potential energy finite when centers
  coincide, but the derivative then provides no direction to separate them.
  Without an explicit successful VPSC collision projection, the optimizer
  fails with `InvalidGeometry`, rather than inventing random jitter. If the
  nonlinear step or the geometry constraints fail, the error is explicit.
  `GradientTolerance`, `LineSearchStalled`, and `IterationBudget` denote
  **different finite iterate termination reasons**, not a global optimum.
- **Scope:** no Cola source is copied; no silent solver fallback, no graph
  topology preservation, global force-energy optimum, multi-edge nudging,
  or complete Cola feature parity is claimed.

Standalone `StunGraphForceTests` exercise an analytic two-node objective,
finite-difference derivatives, disconnected repulsion, cycles and duplicate
edges, reversed edge insertion and node permutations, hard pins and
non-overlap, degenerate seeds, work budgets and 30 deterministic graph
families. The standalone CMake/CTest matrix is independent of the full Charts
package and is **not** evidence that the full `STUN_BUILD_CHARTS=ON` stack
compiles and renders correctly.

An optional comparison (disabled in normal CI) evaluates **every output** of
Gradient Stress, SMACOF and Force under both objective functions, and also
reports edge-length RMSE, rectangle-overlap count and elapsed time:

```sh
cmake -S charts/graphlayout -B build/graphlayout-bench -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSTUN_GRAPHLAYOUT_BUILD_BENCHMARKS=ON
cmake --build build/graphlayout-bench --target stun_graphlayout_force_compare
build/graphlayout-bench/stun_graphlayout_force_compare
```

**Never compare the raw Stress energy to the raw Force energy:** they are
mathematically different objectives. The CSV provides columns for both only
so the same solutions can be compared under the **same chosen metric**. Local
figures vary by CPU, compiler, geometry seed and budget; no upstream
Adaptagrams/ELK benchmark or cross-platform performance claim follows.

## Algorithm v8: ordered-forest tidy tree

`Stun::GraphTidyTree` is an independent C++17, renderer-free solver for rooted
ordered **forests**. Its `Tree` IR has input-index-stable parents, unique IDs,
node-specific measured sizes and an explicit root sentinel. In an iterative
bottom-up contour pass each new sibling subtree is placed right of all earlier
sibling contours at every common depth; the parent is centered between its
first and last children's centers. A depth's Y position is based on the **maximum
height** in the preceding rank, preventing overlap between variable-height
nodes. A second forest-contour pass separates independent trees.

The solver is deterministic for the **same ordered input**; arbitrary sibling
permutation is not required to preserve coordinates because sibling order is
intentional. Contour merging is bounded **O(n^2) worst case**. Explicit node,
contour and rectangle-verification budgets fail transactionally; this does not
claim optimal width or Buchheim linear time.

Infographic's Tree uses this solver directly and retains full pre-order parent
indices, stable card dimensions, and expands the canvas instead of scaling
cards below their requested size. Its tree renderer now requires complete
layout/parent geometry and renders **all descendants**; the former shallow
root/children fallback is removed. Other Infographic chart templates still use
the independent VPSC projection only where necessary.

`charts/mermaid/mindmap` owns a renderer-independent Mindmap AST adapter over
the same Tidy Tree solver. The caller supplies a node-measurement callback; the
adapter does not invent label dimensions from Unicode byte counts or import a
UI dependency. Mindmap rendering and full parser-SVG integration are separate
acceptance tasks.

## Algorithm v9: native obstacle-avoiding Polyline (libavoid-like scope)

`Stun::GraphPolyline` is independently linkable C++17. Its
`route_polyline` / `validate_polyline_routes` API reuses the shared
`Port`, `RouteRequest`, `Routes` and typed `RouteStatus` contracts,
not the Orthogonal binary solver.

- Stable, lexicographically ordered visibility vertices are the corners of
  inflated obstacle rectangles plus exact outward endpoint stubs.
  A* minimizes Euclidean length on this graph, including diagonal segments.
- A separate validator checks boundary anchors, stubs, nonzero segments,
  inflated obstacle interiors and terminal original rectangles.
  All visibility, candidate, obstacle-test, expansion, queue, input and
  batch-waypoint budgets return explicit errors and transactional empty output.
- DOT and Mermaid Flowchart explicitly select native Orthogonal or Polyline
  through their own renderer-independent AST adapters. Both adapters compile
  and run as headless GraphLayout tests. No libavoid, arbitrary straight-edge,
  or orthogonal fallback survives in those professional-mode route paths.
- Positive orthogonal nudging distance explicitly selects `Stun::GraphNudging`;
  unsupported crossing/angle/polyline-nudging options and named DOT ports
  lacking measured geometry are rejected, never silently ignored.

**Not yet complete libavoid parity:** global crossing reduction/nudging,
bundling, checkpoints, named port geometry, compound/arbitrary-shape
obstacles and dense graph scaling. Visibility searches are independently
budgeted per edge. Qualification is based on C++ geometry, sanitizers,
cross-platform tests and future Adaptagrams differential benchmarks.
\n## Algorithm v10: bounded orthogonal multi-edge Nudging\n\n`Stun::GraphNudging` is independently linkable and depends only on\n`Stun::GraphOrthogonal`. `route_orthogonal_nudged` first constructs a\nvalidated batch of native orthogonal paths, then performs a bounded greedy\ncoordinate search over candidate **lane doglegs**. Each candidate retains\nits boundary anchors and outward terminal segment directions and is\nindependently checked for obstacle/clearance collisions **before acceptance**.\n\nA pairwise audit separately measures (1) proper horizontal/vertical crossings\nand (2) summed collinear overlap length. Candidates are accepted only if\nthey lexicographically reduce `(crossings, overlap_length)`; shorter total\nlength breaks ties. The final audit must not increase crossing count or,\nwhen crossings tie, shared overlap. Shared terminal portions may be\nunavoidable because ports are fixed. The optimizer cannot guarantee a\nglobally optimal crossing/nudging solution, minimum lane spacing for every\npair, complete routing around arbitrary shapes, or a strictly better result\non every graph. No external `libavoid` provider is called.\n\nThe new `NudgingOptions` impose explicit input-route, pass, lane, candidate,\nsegment-pair-check and route-point budgets. Exhaustion, invalid options or\nany invalid geometry **clears the entire result** and fails explicitly.\n`audit_orthogonal_interactions` can independently verify a caller-provided\nbatch and its geometric interaction metrics.\n\nDOT/Mermaid Flowchart select the new module only with a *positive* explicit\n`routing_nudging_distance` in Orthogonal mode. Unspecified or zero nudging\nretains independently selected native single-edge Orthogonal A*; Polyline\nnudging is not implemented and is explicitly rejected. The per-chart AST\nadapters own that selection; the mathematical solver never knows about DOT,\nMermaid, SVG or any UI/rendering state.\n\nRegression tests cover duplicate and triple shared edges, distinct disjoint\nroutes, obstacle detours, unavoidable crossings, deterministic reruns,\nedge-request permutation, exact anchors, independent route validation,\ninvalid route rejection and resource failures. This is **partial libavoid-like\nNudging**; named port geometry, checkpoints, crossing-minimal global paths\nand joint polyline optimization remain separate algorithm tasks.\n

## Algorithm v11: mandatory Polyline checkpoints and joint Polyline nudging

The independently linked `Stun::GraphPolyline` now has
`route_polyline_checkpoints` and `validate_polyline_checkpoint_routes`.
Every route may specify ordered **exact checkpoint vertices** in addition to
its precise boundary anchors and outward stubs. Euclidean visibility A* runs
from stub through each mandatory point to the target stub; per-edge candidate,
expansion and obstacle-check budgets are **cumulative over all legs**.
Per-request and batch checkpoint limits also apply. Invalid, duplicate,
nonfinite, or inflated-obstacle-interior checkpoints fail explicitly, as does
any other no-path or capacity condition. The old `route_polyline` API is
unchanged and delegates to the same implementation with no checkpoints.

`Stun::GraphPolylineNudging` is another independent CMake target, depending
only on `Stun::GraphPolyline`. It generates bounded candidate reroutes via
one mandatory checkpoint offset per interior segment. An independent audit
counts **proper Polyline crossings** and exact collinear segment overlap;
accepted candidates must lexicographically reduce crossing count and then
shared overlap length, and each candidate is validated against node obstacle
clearance and outward port geometry. Shared terminal stubs may remain.
DOT and Mermaid Flowchart explicitly invoke the new module when a positive
Polyline nudging distance is specified; zero/unset requests continue using
ordinary Polyline routing. Unsupported angle/crossing penalties and named DOT
ports without measured geometry still return explicit errors.

**Important limits:** This is a bounded deterministic **local improvement**,
not globally optimal multi-edge routing or full libavoid nudging parity.
Compound obstacles, topology-preserving routing, true named-port geometry,
checkpoint syntax in Chart ASTs, and differential Adaptagrams benchmarks
are not yet implemented. This work does not use Lean or introduce UI layout.


## Algorithm v11: measured named-port boundary binding (libavoid-like)

`Stun::GraphPortBindings` is a separately linkable native C++17 algorithm.
A chart supplies named port measurements in **node-local pixel coordinates**,
with an explicit outward N/E/S/W side and a stable node index. The binder
checks finite rectangles, unique (node, name), bounded resources and exact
boundary positions within a tightly bounded measurement tolerance before
deriving the `Port(side,offset)` contract used by Orthogonal and Polyline.
The result is sorted independent of input measurement order and is cleared
transactionally on failure. No interior point, missing name, conflicting
compass or ambiguous corner gets a silently invented attachment point.

DOT's renderer-neutral adapter resolves `node:port[:compass]` from caller-
supplied actual local port measurements. The public `DotGraphRenderer`
offers `layout(diagram, measured_ports)` for applications that have measured
port geometry; `layout(diagram)` intentionally rejects unmeasured names.
This does **not** claim record-label field layout or arbitrary-shape boundary
measurement. Real Chart geometry providers must supply such measurements.
Qualification covers exact world anchors, both native route modes, bad
geometry, missing/duplicate names, direction conflicts, strict budgets,
GCC/Clang sanitizers and cross-platform C++ tests.

## Algorithm v13: bounded topology-preserving polyline displacement

`Stun::GraphTopology` is a separately linkable C++17, renderer-neutral
**initial libtopology-like algorithm**. It is independent of Cola, libavoid,
UI and rendering. The input is a graph, nonoverlapping node rectangles,
explicit measured N/E/S/W boundary ports, and baseline polyline connectors.

- `audit_topology` validates routes against nodes, checks nonadjacent
  self-contact and ambiguous edge contacts, then records the **ordered,
  signed crossing partner sequence along every edge**, not merely a crossing
  count. Unmeasured Auto ports, self-loop edges, duplicate IDs and ambiguous
  collinear/shared routing are explicitly unsupported in this first stage.
- `move_topology_preserving` computes a bounded displacement toward a desired
  placement. It interpolates node positions and arclength-weighted waypoints,
  and validates multiple sampled intermediate geometries for node overlap,
  edge/node penetration, segment contact, ordered crossings and signs.
  Successful output reports its exact accepted fraction (possibly below 1.0).
  Requested full movement is not silently assumed feasible.
- Hard caps cover input nodes/edges, per-edge and batch waypoints, cumulative
  segment/obstacle tests, trial count, and interpolation frames. Failure
  returns a typed status and **empty output**; it never invokes a different
  layout engine or rerouting provider.
- The independent suite includes analytic 0/1/2-crossing embeddings, attempted
  reversal of crossing order, intermediate route contacts, impossible moves,
  bad ports, degenerate geometry, exhaustion and 48 deterministic graph moves.

**Limit:** sampled continuation does **not** certify *every instant* of a
continuous movement; this is a conservative numerical topology gate, **not
complete libtopology parity**. In particular, it does not yet preserve
arbitrary nonrectangular connector obstacles, overlapping/shared port stubs,
self-loop embeddings, cluster containment or every possible topological
isotopy. Future acceptance requires more general segment intersection events,
actual routed-connector integration, larger adversarial corpora and upstream
Adaptagrams differential tests. No Lean proofs are in scope.

