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
