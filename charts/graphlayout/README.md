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

`stun/graphlayout/orthogonal.h` provides a self-owned orthogonal edge router,
independent of C++ UI/render APIs or third-party graph layout implementations.

1. Check bounded, finite rectangles and exact node-side port anchors. Invalid,
   overlapping or over-capacity obstacles fail with a typed status.
2. Expand node rectangles by `clearance` and construct a compressed visibility
   grid using rectangle boundary axes plus port stub axes. No grid segment is
   allowed through an expanded rectangle interior; each port's own initial
   stub is exempt from its node's clearance envelope.
3. Apply direction-aware A* with Manhattan heuristic, nonnegative path length
   and bend cost. Stable grid/state order breaks ties deterministically.
4. Reconstruct and simplify paths. An independent postcondition checker tests
   orthogonality, endpoint direction, obstacle non-penetration and clearance.
5. On *any* error the entire batch is empty. No arbitrary straight fallback
   or partial-result success. Error includes failing edge index.

The initial scope is **single-edge orthogonal shortest paths on the chosen
visibility grid**, not a proof of globally optimal multi-edge crossing/nudging.
No Lean theorem or native SDK integration claim is made by these C++ tests.
Finite input, hard resource budgets and deterministic operation are required.
