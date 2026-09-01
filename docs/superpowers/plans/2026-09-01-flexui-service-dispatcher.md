# FlexUI Automatic Service Dispatcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Automatically route bounded application service requests to their authorized registry endpoints and return synchronous submission failures through the existing completion/controller path.

**Architecture:** Add an owner-thread `ApplicationServiceDispatcher` to `FlexUI::Controller`; it depends only on `DesktopApplication` and `FlexUI::Services`, never on PluginHost or a platform window. `GCanvasWindowHost` owns one dispatcher, pumps completions before requests, submits a bounded number of requests before `on_frame`, and schedules another empty event when the request bound is reached. PluginHost remains an optional registry producer whose stop/join ownership will be added in a separate desktop-composition phase.

**Tech Stack:** C++17, FlexUI request table/completion mailbox/service registry, gCanvas/GLFW, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 5.1, 9.5, 12, 13.2-13.5, 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5-P7.

## Global Constraints

- The application request table remains the only token/script-request identity fact source.
- Registry resolution is repeated defensively after the owning request crosses into the host dispatcher.
- Endpoint success means it copied retained request data and owns exactly one completion attempt; the dispatcher must not synthesize another completion.
- Endpoint `Busy`, `Closed`, `InvalidRequest`, `Rejected`, `InternalFailure`, or a thrown C++ exception is a terminal submission result converted to one failed `ApplicationCompletion`; no fallback endpoint is selected.
- A synthesized terminal completion is posted immediately. `QueueFull` retains exactly that one completion and retries it before receiving another request; no preflight capacity snapshot is treated as authoritative.
- The dispatcher owns at most one pending terminal completion and has no worker thread, queue, plugin handle, or GPU/window dependency.
- All dispatcher methods that advance state are owner-thread-only and non-blocking.
- One pump settles at most the configured positive request bound. Reaching the bound schedules another gCanvas empty event instead of spinning.
- Existing raw request polling remains available when no host dispatcher is constructed.
- Existing enum numeric values remain stable by appending new values.

---

### Task 1: Owner-thread ApplicationServiceDispatcher

**Files:**
- Create: `flexUI/include/flexUI/application_service_dispatcher.h`
- Create: `flexUI/modules/controller/application_service_dispatcher.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`
- Test: `flexUI/tests/test_application_service_dispatcher.cpp`

**Interfaces:**
- Consumes: `DesktopApplication::try_receive_service_request()`, `resolve_service_request()`, `completion_mailbox()`, and `IApplicationServiceEndpoint::try_submit()`.
- Produces: `ApplicationServiceDispatcher::create(DesktopApplication&, ApplicationServiceDispatcherLimits)`, `pump()`, and structured status/error/statistics values.

- [x] **Step 1: Write failing TinyTest cases** for a recording endpoint. Build a scripted headless application that emits service commands from one frame, then prove the dispatcher submits FIFO requests, passes the application mailbox sink, and stops at `max_requests_per_pump = 1`.

```cpp
flexUI::ApplicationServiceDispatcherLimits limits;
limits.max_requests_per_pump = 1;
auto created = flexUI::ApplicationServiceDispatcher::create(*built.application, limits);
check(created);
check(created.dispatcher->pump().status ==
      flexUI::ApplicationServiceDispatchStatus::Progress);
```

- [x] **Step 2: Add failure tests** for zero/above-maximum limits, foreign-thread pump, endpoint rejection converted into a failed scripted completion, endpoint exception conversion, and a full completion mailbox retaining exactly one pending terminal completion until a later pump.

- [x] **Step 3: Run the new target before implementation** under `win-dev-user`; require compilation failure because the dispatcher header/types do not exist.

```powershell
cmake --build --preset win-dev-user --target test_application_service_dispatcher
```

- [x] **Step 4: Implement the dispatcher** with an opaque `Impl`, immutable owner thread/application pointer, bounded per-pump loop, one optional pending terminal completion, and per-pump submitted/failed counters. Map submit codes to stable strings:

```text
Busy            -> service.submit.busy
Closed          -> service.submit.closed
InvalidRequest  -> service.submit.invalid_request
Rejected        -> service.submit.rejected
InternalFailure -> service.submit.internal_failure
exception       -> service.submit.exception
```

`QueueFull` while publishing a synthesized failure returns `Blocked` and retains the completion. Any other mailbox failure returns `CompletionPostFailed`; resolution/receive invariant failures return their own nested error and do not silently discard state.

- [x] **Step 5: Build and run** `test_application_service_dispatcher`, `test_application_service_requests`, `test_desktop_application`, and `test_application_completion` under Debug/ASan.

---

### Task 2: Bounded gCanvas request submission

**Files:**
- Modify: `flexUI/include/flexUI/gcanvas_window_host.h`
- Modify: `flexUI/modules/desktop/gcanvas_window_host.cpp`
- Modify: `flexUI/tests/test_gcanvas_window_host.cpp`

**Interfaces:**
- Consumes: `ApplicationServiceDispatcher` from Task 1 and `gcanvas::Window::trigger_events()`.
- Produces: `GCanvasWindowHostConfig::max_service_requests_per_pump`, request-dispatch host stage/error mapping, and automatic endpoint submission before `DesktopApplication::frame()`.

- [x] **Step 1: Extend the GPU host fixture** so its endpoint records submitted requests and posts synchronous completions. Keep raw-consumer cases separate; after the first frame emits a command, one `pump_once()` submits it and the following completion stage delivers it without test-side routing.

- [x] **Step 2: Add a request bound test** with two emitted commands and `max_service_requests_per_pump = 1`; prove one endpoint submission per pump. Add zero/above-maximum configuration rejection; rejection-to-script-failure is covered by the platform-independent dispatcher test.

- [ ] **Step 3: Run the GPU host test before integration** and require the automatic-routing assertions to fail while existing OpenGL/Vulkan lifecycle cases still build.

- [x] **Step 4: Integrate one dispatcher per host.** Append `ServiceDispatch` stage and `ServiceDispatchFailed` code; create the dispatcher after application publication, drain completions first, pump requests second, then call `on_frame`. A dispatcher error follows the existing primary-error close/shutdown path. `Progress` at the configured bound and `Blocked` with retained terminal completion both call `Window::trigger_events()` once.

- [x] **Step 5: Run hidden OpenGL/Vulkan host and adjacent input/application tests** under Debug/ASan and Release.

---

### Task 3: Protocol documentation and final verification

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`
- Modify: `docs/superpowers/plans/2026-09-01-flexui-service-dispatcher.md`

**Interfaces:**
- Consumes: verified dispatcher state, terminal failure conversion, gCanvas ordering, and feature-off dependency behavior.
- Produces: current architecture and roadmap evidence without claiming PluginHost lifecycle ownership.

- [x] **Step 1: Update diagrams and ownership text** to show completion drain → bounded request submit → frame, and explicitly state that `FlexUI::Controller` does not depend on PluginHost.

- [x] **Step 2: Update P5/P6/P7 evidence** for automatic service routing, bounds, errors, and tests. Keep PluginHost application ownership, typed payload schema, examples, benchmarks, and reload races unchecked.

- [x] **Step 3: Run plan placeholder/type self-review.**

```powershell
rg.exe -n "T[D]B|T[O]DO|implement[ ]later|appropriat[e]|similar[ ]to" docs/superpowers/plans/2026-09-01-flexui-service-dispatcher.md
```

- [x] **Step 4: Run final verification** with `git diff --check`, CodeGraph sync/affected, focused Debug/ASan and Release tests, TurboScript-on controller/application tests, and standard plugin/GPU feature-off configure. Restore `win-dev-user` defaults afterward.

## Compatibility, Migration, and Rollback

- Existing applications and raw request consumers are unchanged unless a dispatcher is explicitly constructed; gCanvas host construction opts into automatic routing.
- Existing gCanvas config member order is preserved by appending the request bound. New enum values are appended.
- Plugin ABI, request/completion record layouts, registry semantics, CMake feature switches, and persisted formats do not change.
- Endpoint rejection becomes a failed controller completion only in the new automatic dispatcher path; manual raw consumers retain full control over their own failure policy.
- Rollback removes the dispatcher and gCanvas integration without changing request-table, mailbox, PluginHost, or TurboScript ABI state.
