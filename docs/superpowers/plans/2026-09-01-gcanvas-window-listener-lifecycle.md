# gCanvas Window Listener Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give a future `GCanvasWindowHost` owner-scoped, reentrancy-safe window subscriptions and native focus/close notifications without breaking existing persistent listeners.

**Architecture:** `Window` keeps its existing `void add_*_listener` compatibility API. New `subscribe_*_listener` methods return one move-only RAII `WindowListenerSubscription`, backed by a weak reference to window-owned listener state. Each typed registry uses monotonic IDs, tombstones during dispatch, and deferred compaction so a callback may remove itself or a later callback safely. GLFW remains confined to `gCanvas::Window` and publishes focus/close through the same registry.

**Tech Stack:** C++17, gCanvas Window, GLFW, CMake Presets, existing contract executables.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 14-15 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5.

## Global Constraints

- Existing `add_*_listener` calls remain persistent until `reset_listener()` or window destruction; ignored return values therefore cannot silently unsubscribe.
- Scoped subscription destruction removes only that subscription and is safe after its window has already been destroyed.
- Listener registration, delivery, removal, and window destruction remain confined to the window owner thread.
- Removal during callback delivery prevents a not-yet-called listener from running in the same delivery; listeners added during delivery begin with the next event.
- `reset_listener()` clears every event category, including character, focus, and close callbacks.
- Focus and close are notifications only; close veto and native pointer capture remain outside this phase.

---

### Task 1: Reentrant typed listener registry and RAII subscription

**Files:**
- Create: `vendor/gCanvas/src/window_listener_state.hpp`
- Create: `vendor/gCanvas/tests/window_listener_test.cpp`
- Modify: `vendor/gCanvas/include/gcanvas/window.hpp`
- Modify: `vendor/gCanvas/CMakeLists.txt`

**Interfaces:**
- Consumes: typed `std::function<void(Event)>` callbacks and the window owner-thread lifecycle.
- Produces: move-only `WindowListenerSubscription`, per-event subscribe/remove, persistent compatibility registration, and complete reset.

- [x] **Step 1: Write registry tests for move, scoped destruction, window-state destruction, self-removal, later-listener removal, add-during-delivery, and reset.**
- [x] **Step 2: Build the focused test and confirm the missing API fails.**
- [x] **Step 3: Implement listener state and the public subscription value with weak ownership.**
- [x] **Step 4: Run the focused listener test.**

### Task 2: Integrate every Window callback and native focus/close events

**Files:**
- Modify: `vendor/gCanvas/include/gcanvas/event.hpp`
- Modify: `vendor/gCanvas/include/gcanvas/window.hpp`
- Modify: `vendor/gCanvas/src/window_impl.hpp`
- Modify: `vendor/gCanvas/src/window_impl.cpp`
- Modify: `vendor/gCanvas/tests/contract_test.cpp`

**Interfaces:**
- Consumes: GLFW size, cursor, mouse button, key, character, scroll, focus, and close callbacks.
- Produces: persistent `add_*` and scoped `subscribe_*` paths sharing one fact source.

- [x] **Step 1: Add focus/close event values and compile-time public contract checks.**
- [x] **Step 2: Route legacy registration, scoped registration, reset, and all event publication through listener state.**
- [x] **Step 3: Register GLFW focus and close callbacks when the context activates native event delivery.**
- [x] **Step 4: Build and run gCanvas contract, listener, backend-link, and hidden OpenGL GPU tests.**

### Task 3: Document the completed prerequisite and remaining host boundary

**Files:**
- Modify: `vendor/gCanvas/docs/host-and-resource-protocol.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: verified subscription/focus/close semantics.
- Produces: explicit owner-thread and dispatch-mutation contract plus an accurate P5 checklist.

- [x] **Step 1: Document ownership, delivery mutation semantics, and close notification limits.**
- [x] **Step 2: Mark only listener-scoped removal and native focus/close event prerequisites complete; retain native capture and full host items.**
- [x] **Step 3: Run formatting, diff, CodeGraph, and focused/adjacent regression verification.**

## Compatibility, Migration, and Rollback

- Existing source compatibility is preserved because all legacy `add_*` signatures and behavior remain unchanged.
- New consumers should store the returned scoped subscription as a member declared after the borrowed callback target and destroy it before that target.
- The change adds no dependency and does not change gCanvas renderer or frame behavior.
- Rollback is local: remove scoped APIs/state and restore the six legacy vectors; no persisted data, package format, or controller ABI changes.
