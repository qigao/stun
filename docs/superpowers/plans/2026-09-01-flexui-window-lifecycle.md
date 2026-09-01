# FlexUI Window Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the application close/shutdown, focus-loss cleanup, and Windows native pointer-capture contracts required before constructing `GCanvasWindowHost`.

**Architecture:** `DesktopApplication` remains the single source of truth for its owner-thread lifecycle and explicitly unmounts its controller during shutdown while retaining readable published UI state until destruction. `GCanvasApplicationInputRouter` translates focus loss into FlexUI focus/capture cleanup. `gCanvas::Window` owns the platform capture adapter so FlexUI never imports GLFW native or Win32 types.

**Tech Stack:** C++17, FlexUI Controller/Desktop modules, gCanvas Window, GLFW 3.4 native access, Win32 `SetCapture`/`ReleaseCapture`, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5.

## Global Constraints

- Every lifecycle transition and input operation runs on the application/window owner thread.
- `Ready -> CloseRequested -> Shutdown` is monotonic; repeated close requests and completed shutdown calls are idempotent.
- Close request stops new dispatch, reload, and frame callbacks; it does not destroy UI state or native resources.
- Shutdown calls optional controller `on_unmount` exactly once and reports its error after cleanup; published Box/program/source state remains readable until application destruction.
- A shutdown attempt during controller dispatch fails without claiming Shutdown, so the owner may retry after the callback returns.
- Focus gain does not change FlexUI state; focus loss clears both focused element and internal mouse capture.
- Native pointer capture uses Win32 mouse capture semantics. Unsupported platforms report capability false and reject capture changes instead of substituting cursor confinement.
- No FlexUI Core target links GLFW, Win32, or `gCanvas::Window`.

---

### Task 1: DesktopApplication close and shutdown state machine

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `ScriptController::unmount()`, existing owner-thread authority, and published application state.
- Produces: `DesktopApplicationState`, `state()`, `request_close()`, `shutdown()`, and lifecycle-specific structured errors.

- [x] **Step 1: Add TinyTest cases for monotonic/idempotent transitions, event/reload rejection after close, owner-thread rejection, successful unmount, unmount callback failure after cleanup, and retry after shutdown attempted during dispatch.**
- [x] **Step 2: Build `test_desktop_application` and confirm compilation fails on the missing lifecycle API.**
- [x] **Step 3: Implement lifecycle state and fail-fast operation gates without destroying the published Box.**
- [x] **Step 4: Run `test_desktop_application`, `test_script_controller`, and TurboScript application regression tests.**

### Task 2: gCanvas focus routing and Windows native pointer capture

**Files:**
- Modify: `vendor/gCanvas/include/gcanvas/window.hpp`
- Modify: `vendor/gCanvas/src/window_impl.cpp`
- Modify: `vendor/gCanvas/tests/contract_test.cpp`
- Modify: `flexUI/include/flexUI/gcanvas_application_input.h`
- Modify: `flexUI/modules/desktop/gcanvas_application_input.cpp`
- Modify: `flexUI/tests/test_gcanvas_application_input.cpp`

**Interfaces:**
- Consumes: `gcanvas::focus_event`, `flexUI::host::clear_focus_and_capture()`, and the native GLFW window handle.
- Produces: `Window::supports_pointer_capture()`, `has_pointer_capture()`, `set_pointer_capture(bool)`, and `GCanvasApplicationInputRouter::focus()`.

- [x] **Step 1: Add tests for focus gain/no-op, focus-loss cleanup, wrong-thread preservation, public pointer-capture capability signatures, and unsupported-platform fail-fast semantics.**
- [x] **Step 2: Build focused targets and confirm missing API failures.**
- [x] **Step 3: Implement focus routing and Win32 capture behind the gCanvas Window boundary.**
- [x] **Step 4: Run listener, input-router, host-bridge, and hidden OpenGL/Vulkan window tests.**

### Task 3: Documentation and completion gate

**Files:**
- Modify: `vendor/gCanvas/docs/host-and-resource-protocol.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: verified lifecycle, focus, and capture contracts.
- Produces: the exact ownership and failure rules the next `GCanvasWindowHost` implementation may assume.

- [x] **Step 1: Document lifecycle state ownership, retained readable state after shutdown, focus-loss ordering, and Win32 capture capability.**
- [x] **Step 2: Mark only the completed P5 prerequisites; retain `GCanvasWindowHost`, main-loop, IME, services, and end-to-end items.**
- [x] **Step 3: Run new-file formatting, `git diff --check`, CodeGraph sync/affected, Debug+ASan tests, and Release tests.**

## Compatibility, Migration, and Rollback

- Existing applications remain `Ready` after build and retain current dispatch/reload behavior until they explicitly request close.
- Existing gCanvas Window input and rendering APIs are unchanged; pointer capture is additive and Windows-only in this phase.
- Hosts migrate by calling `request_close()` from native close notification, stopping new event delivery, calling `shutdown()`, then destroying application, renderer, context, and window in that order.
- Rollback removes only additive lifecycle/focus/capture APIs and their tests; no persisted data, XML/CSS format, TurboScript ABI, or renderer frame contract changes.
