# FlexUI gCanvas Plugin Composition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one optional desktop composition boundary that owns PluginHost and GCanvasWindowHost together, routes the PluginHost registry into DesktopApplication, and guarantees plugin stop/join before the application completion mailbox is destroyed.

**Architecture:** Add `GCanvasPluginWindowHost` in an optional `FlexUI::GCanvasPluginWindowHost` target. Construction consumes one already-built `PluginHostBuildResult`, verifies that its host and registry are one consistent Started snapshot, injects that registry into `DesktopApplicationBuilder`, then constructs the existing gCanvas host. Runtime methods delegate window work and, after application shutdown, stop plugins with a bounded retryable timeout; destruction uses PluginHost's infinite-join safety net while the application object is still alive.

**Tech Stack:** C++17, FlexUI PluginHost/Controller/gCanvas desktop adapters, TinyTest, CMake Presets, hidden OpenGL/Vulkan test windows.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 5.1, 9.5, 13.3-13.5, 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P4-P7.

## Global Constraints

- `FlexUI::Controller` and `FlexUI::GCanvasWindowHost` remain buildable without PluginHost; only the new composition target links `FlexUI::PluginHost`.
- The registry returned by the consumed PluginHost is the application request queue's sole authorization/routing fact source; mismatched host/registry inputs fail before window creation.
- Shutdown order is request/mailbox close → controller unmount → PluginHost stop/join → composition destruction; the application object remains alive through every retryable PluginHost timeout.
- PluginHost `JoinTimedOut`, `StopFailed`, and `JoinFailed` remain nested errors and keep the composition retryable; there is no fallback service or forced DLL unload.
- A normal explicit shutdown uses a caller-supplied non-negative timeout. Destruction may wait indefinitely because unloading live plugin code or destroying a borrowed completion sink is unsafe.
- Existing Plugin ABI, PluginHost, GCanvasWindowHost, DesktopApplication and persisted formats remain unchanged.

---

### Task 1: Optional gCanvas Plugin Composition API

**Files:**
- Create: `flexUI/include/flexUI/gcanvas_plugin_window_host.h`
- Create: `flexUI/modules/desktop/gcanvas_plugin_window_host.cpp`
- Modify: `flexUI/modules/desktop/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`
- Test: `flexUI/tests/test_gcanvas_plugin_window_host.cpp`

**Interfaces:**
- Consumes: `PluginHostBuildResult`, `ApplicationCapabilityManifest`, `DesktopApplicationBuilder`, `GCanvasWindowHostConfig`, `PluginHost::stop(timeout)` and `GCanvasWindowHost::{pump_once,run,request_close}`.
- Produces: `GCanvasPluginWindowHost::create(...)`, `pump_once(double)`, `run()`, `request_close()`, `shutdown(std::chrono::milliseconds)`, and accessors for the owned application/plugin/window hosts.

- [x] **Step 1: Write a failing end-to-end TinyTest.** Build the existing echo test DLL into a PluginHost, resolve and retain its endpoint, create a scripted application that emits `test.echo/1:echo`, and require three bounded pumps to emit, submit, and dispatch the completion without test-side request routing. Then call `shutdown(1s)` and require application `Shutdown`, plugin `Stopped`, and the cached endpoint to return `Closed`.

- [x] **Step 2: Add construction and retry tests.** Reject a default/invalid `PluginHostBuildResult`, a host/registry mismatch, a negative stop timeout, and a non-owner call. Submit the echo plugin's `block` operation, require `shutdown(1ms)` to return nested `JoinTimedOut` while retaining application/plugin ownership, then require `shutdown(1s)` to complete on retry.

- [x] **Step 3: Run the new target before implementation.** Configure `win-dev-user` with `FLEXUI_ENABLE_PLUGINS=ON` and `FLEX_BUILD_GCANVAS_GPU_TESTS=ON`; require compilation failure because `flexUI/gcanvas_plugin_window_host.h` does not exist.

- [x] **Step 4: Implement the optional composition.** Validate inputs before creating a window; inject `plugins.registry` and the explicit capability manifest into the application builder. Store the window host before the plugin host so reverse member destruction stops/unloads plugins while the application mailbox still exists. Convert window and plugin errors once at the composition boundary and retain both nested causes if shutdown encounters both.

- [x] **Step 5: Implement retryable shutdown.** `request_close()` only closes the window/application boundary. `shutdown(timeout)` finishes controller shutdown first and then calls `PluginHost::stop(timeout)`. `pump_once()` and `run()` automatically use the configured timeout only after the application reaches `Shutdown`; a timeout returns failure without releasing either owner. Repeated shutdown after both components stop is idempotent.

- [x] **Step 6: Build and run** `test_gcanvas_plugin_window_host`, `test_gcanvas_window_host`, `test_plugin_host`, `test_application_service_dispatcher`, and `test_desktop_application` under Debug/ASan and Release with plugins/GPU tests enabled.

---

### Task 2: Architecture and Feature-Boundary Evidence

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`
- Modify: `docs/superpowers/plans/2026-09-01-flexui-gcanvas-plugin-composition.md`

**Interfaces:**
- Consumes: verified composition ownership, retryable timeout, automatic service routing and feature-off target behavior.
- Produces: diagrams and checklist evidence that distinguish the generic gCanvas host from the optional plugin composition.

- [x] **Step 1: Update ownership and shutdown diagrams.** Show `GCanvasPluginWindowHost` above the independent PluginHost and GCanvasWindowHost targets, registry injection at construction, and close/unmount → stop/join → destruction ordering.

- [x] **Step 2: Update P5/P6/P7 evidence.** Mark only desktop PluginHost composition and its shutdown/timeout tests complete. Keep package discovery, typed payload schema, OS sandbox, examples, benchmarks, hot reload, and reload/close stress items unchanged.

- [x] **Step 3: Verify feature boundaries.** Configure/build core dispatcher and gCanvas host with `FLEXUI_ENABLE_PLUGINS=OFF` and confirm no composition target is generated. Configure with plugins on/GPU tests off and confirm PluginHost tests build without requiring gCanvas composition tests.

- [x] **Step 4: Run final checks.** Run placeholder scan, `git diff --check`, CodeGraph sync/affected, focused Debug/ASan and Release tests, then restore the default `win-dev-user` configuration.

## Compatibility, Migration, and Rollback

- Existing code continues using `GCanvasWindowHost` or `PluginHost` independently. Only callers opting into the new conditional target transfer both lifetimes to the composition.
- The new API is additive and C++-only; it does not alter the versioned C plugin ABI or existing enum values/layouts.
- A caller migrates by building PluginHost as before, moving its complete build result into composition `create()`, and removing manual registry injection and stop ordering.
- Rollback removes the conditional composition target/header/test without changing PluginHost, Controller, gCanvas host, request table or mailbox behavior.
