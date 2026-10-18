# Shared native modules

This directory owns reusable implementation modules shared by Stun components.
Each child directory has its own CMake target declarations and tests; the root
project includes this directory through `add_subdirectory(shared)`.

Modules:

- `gcanvas/`: canvas core and optional rendering/presentation backends.
- `markdown/`: the canonical C11 Markdown parser and its semantic/HTML output
  implementation. MarkdownWidget must consume this single parser; no duplicate
  parser or fallback implementation is permitted.
- `layout/`: backend-neutral layout algorithms. UI-specific widget-tree,
  style-cascade, event-dispatch, and rendering orchestration remain in their
  owning UI modules.

## Migration invariants

- Keep the project in this repository; these are CMake subdirectories, not Git
  submodules.
- Preserve public APIs and existing target names unless a separate change is
  explicitly required.
- Keep module dependencies explicit and directed; reusable algorithms must not
  acquire dependencies on concrete UI or rendering backends.
- Move implementations rather than copying them. Do not maintain parallel old
  and new implementations or a compatibility fallback.
- Preserve existing unit, CommonMark, GFM, backend, and platform CI coverage.
