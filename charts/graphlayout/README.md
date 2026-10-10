# Stun GraphLayout — self-owned layout kernel

`stun_graphlayout` is a dependency-free C++17 graph **placement** solver. It
is not a widget, docking, rendering or UI layout system. It is built independently
of Salts, FlexUI, Cairo, OpenGL and the current Adaptagrams vendor library.

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
- A future Lean model will prove SCC condensation acyclicity, rank monotonicity
  and rectangle-separation invariants; **no Lean proof is claimed yet**.

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

The geometric validator checks emitted routes but it is **not** a Lean proof,
and the normal floating-point caveats remain. Additional obligations include
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
future graph algorithms. Lean formalization of the convex KKT sufficiency
theorem, positive-cycle witness and composition contracts is pending.

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
not imply globally optimal joint 2D compaction. Exact Lean proof and native
routing/placement composition are future acceptance tasks.
