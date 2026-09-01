# FlexUI Completion Wakeup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wake an event-driven gCanvas desktop loop after a worker publishes a service completion, then consume a bounded number of scripted completions on the application owner thread.

**Architecture:** `ApplicationCompletionMailbox` remains the only completion fact source. It receives an optional non-owning `noexcept` wakeup function and opaque context at construction, invokes it only after a successful queue publication, and quiesces active callbacks before close returns. `GCanvasWindowHost` injects its existing GLFW empty-event wakeup and drains completions through `DesktopApplication::try_dispatch_service_completion()` before the frame callback, with a configurable per-pump bound.

**Tech Stack:** C++17, TurboUtils Disruptor, FlexUI Controller/DesktopApplication, gCanvas/GLFW, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 13.3, 13.5, 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5/P6/P7.

## Global Constraints

- The wakeup callback carries no completion data and cannot become a second queue or fact source.
- The callback type is a plain `noexcept` function pointer plus borrowed opaque context; no `std::function`, allocation, lock, UI, renderer, or plugin type crosses the mailbox hot path.
- A successful queue publish happens-before its wakeup callback. Invalid, stale, full, closing, and closed publications do not notify.
- `close()` and generation advance wait for active publishers, including their wakeup callback, before returning or replacing generation state.
- The callback context must outlive the mailbox and all producers. `GCanvasWindowHost` satisfies this because its native window outlives its application/mailbox.
- `glfwPostEmptyEvent()` is the only cross-thread GLFW operation used; GLFW documents that it may be called from any thread and wakes `glfwWaitEvents()`/`glfwWaitEventsTimeout()`.
- Only the application owner thread consumes completion records or invokes TurboScript.
- Raw completion users remain unchanged. The gCanvas host auto-consumes only when the mounted controller has `on_service_completion`; otherwise the owning records remain available to the raw polling API.
- One pump consumes at most the configured positive bound. Reaching the bound schedules another empty event instead of spinning.
- Existing enum numeric values remain stable by appending new error/stage values.

---

### Task 1: Mailbox wakeup contract

**Files:**
- Modify: `flexUI/include/flexUI/application_completion.h`
- Modify: `flexUI/modules/controller/application_completion.cpp`
- Test: `flexUI/tests/test_application_completion.cpp`

**Interfaces:**
- Consumes: the existing MPSC `ApplicationCompletionMailbox::try_post()` publication and `ActivePublisher` quiescence guard.
- Produces: `ApplicationCompletionWakeupFn`, `ApplicationCompletionWakeup`, and `ApplicationCompletionMailbox::create(initial_generation, limits, wakeup)`.

- [x] **Step 1: Write failing TinyTest cases** for a wakeup callback that increments an atomic counter. Prove one callback follows each successful publication, rejected invalid/full/stale/closed posts do not call it, and `close()` does not return while a callback deliberately remains inside its function.

```cpp
struct WakeProbe {
  std::atomic<std::uint64_t> calls{0};
  std::atomic<bool> release{true};
};

void record_wakeup(void *context) noexcept {
  auto &probe = *static_cast<WakeProbe *>(context);
  probe.calls.fetch_add(1, std::memory_order_release);
  while (!probe.release.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}
```

- [x] **Step 2: Run the mailbox test before implementation** with `win-dev-user`; require compilation failure because the wakeup types and three-argument `create()` do not exist.

```powershell
cmake --build --preset win-dev-user --target test_application_completion
ctest --preset win-dev-user -R "^test_application_completion$" --output-on-failure
```

- [x] **Step 3: Add the allocation-free notifier API** and validate that a null callback cannot retain a non-null context.

```cpp
using ApplicationCompletionWakeupFn = void (*)(void *context) noexcept;

struct ApplicationCompletionWakeup {
  ApplicationCompletionWakeupFn callback = nullptr;
  void *context = nullptr;
  explicit operator bool() const noexcept { return callback != nullptr; }
};
```

Append `InvalidWakeup` to `ApplicationCompletionErrorCode`. Store the immutable wakeup in `Impl`; after `disruptor_publisher_publish()` succeeds, call `wakeup.callback(wakeup.context)` while `ActivePublisher` is still alive. Do not call it on any rejection path.

- [x] **Step 4: Run `test_application_completion`** under Debug/ASan and Release and require all mailbox lifecycle and concurrent-producer cases to pass.

---

### Task 2: DesktopApplication builder injection

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Test: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `ApplicationCompletionWakeup` from Task 1.
- Produces: `DesktopApplicationBuilder::completion_wakeup(ApplicationCompletionWakeup)` and construction-time forwarding into the mailbox.

- [x] **Step 1: Add a failing application test** that configures a probe callback, builds a headless application, publishes a valid pending-request completion from a worker, and observes one notification while the record remains available to `try_receive_service_completion()`.

```cpp
builder.completion_wakeup({record_wakeup, &probe});
```

The test must also prove a moved-from builder does not dereference its empty implementation when `completion_wakeup()` is called.

- [x] **Step 2: Run `test_desktop_application` before implementation** and require compilation failure for the missing builder method.

- [x] **Step 3: Store the notifier in `ApplicationConfig`** and pass it to `ApplicationCompletionMailbox::create()`. Document that the callback and context are borrowed until application close has quiesced producers; the builder neither owns nor invokes the callback itself.

```cpp
DesktopApplicationBuilder &
completion_wakeup(ApplicationCompletionWakeup wakeup) noexcept;
```

- [x] **Step 4: Run desktop application, service-request, and mailbox regressions** under Debug/ASan and Release.

---

### Task 3: Event-driven gCanvas completion dispatch

**Files:**
- Modify: `vendor/gCanvas/include/gcanvas/window.hpp`
- Modify: `vendor/gCanvas/src/window_impl.cpp`
- Modify: `flexUI/include/flexUI/gcanvas_window_host.h`
- Modify: `flexUI/modules/desktop/gcanvas_window_host.cpp`
- Test: `flexUI/tests/test_gcanvas_window_host.cpp`

**Interfaces:**
- Consumes: `gcanvas::Window::trigger_events()`, `DesktopApplicationBuilder::completion_wakeup()`, `ScriptController::has_service_completion_handler()`, and `DesktopApplication::try_dispatch_service_completion()`.
- Produces: a thread-safe `noexcept` gCanvas wakeup adapter, `GCanvasWindowHostConfig::max_service_completions_per_pump`, and bounded completion processing before `DesktopApplication::frame()`.

- [x] **Step 1: Add failing window-host tests** using a fake module with one-shot `on_frame` and `on_service_completion`. The first frame publishes a service command, a worker posts its completion, and `pump_once()` consumes it and commits the returned text mutation. Add a limit test with two pending requests and `max_service_completions_per_pump = 1`, proving successive pumps consume one record each. Add configuration tests for zero and above-maximum bounds.

```cpp
struct GCanvasWindowHostConfig {
  static constexpr std::size_t kDefaultMaxServiceCompletionsPerPump = 64;
  static constexpr std::size_t kMaximumServiceCompletionsPerPump = 4096;
  // existing fields...
  std::size_t max_service_completions_per_pump =
      kDefaultMaxServiceCompletionsPerPump;
};
```

- [x] **Step 2: Configure the GPU-gated host test and run it before implementation**; require the new assertions to fail while existing OpenGL/Vulkan lifecycle cases remain buildable.

```powershell
cmake --fresh --preset win-dev-user -DFLEX_BUILD_GCANVAS_GPU_TESTS=ON
cmake --build --preset win-dev-user --target test_gcanvas_window_host
ctest --preset win-dev-user -R "^test_gcanvas_window_host$" --output-on-failure
```

- [x] **Step 3: Make `Window::trigger_events()` explicitly `noexcept`** and add a free `noexcept` adapter in the FlexUI desktop module. During `GCanvasWindowHost::create()`, override the builder notifier with this adapter before `build()`; no gCanvas type enters the core mailbox header.

```cpp
void wake_gcanvas_event_loop(void *) noexcept {
  gcanvas::Window::trigger_events();
}
```

- [x] **Step 4: Implement bounded owner-thread drain** before `application->frame()`. Skip consumption when no controller handler exists. Map dispatch failures to appended `GCanvasWindowHostStage::ServiceCompletion` and `GCanvasWindowHostErrorCode::ServiceCompletionFailed`, close through the existing primary-error path, and call `Window::trigger_events()` after exactly the configured maximum was consumed so remaining work gets another pump.

- [x] **Step 5: Run the hidden OpenGL/Vulkan host test** plus `test_gcanvas_application_input`, `test_desktop_application`, and `test_application_completion` under Debug/ASan and Release where generated.

---

### Task 4: Protocol documentation and final verification

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`
- Modify: `docs/superpowers/plans/2026-09-01-flexui-completion-wakeup.md`

**Interfaces:**
- Consumes: the verified callback lifetime, publication order, gCanvas wakeup, bounded drain, and error behavior from Tasks 1-3.
- Produces: the documented event-driven completion protocol and checked roadmap evidence.

- [x] **Step 1: Update the design sequence** to show worker publish → callback-only empty event → owner wait return → bounded mailbox drain → controller transaction → frame. State that notifications may be redundant, carry no data, and cannot replace mailbox polling.

- [x] **Step 2: Update P5/P6/P7 checklist evidence** for event-driven service wakeup and idle behavior. Do not mark PluginHost ownership, typed payload schema, examples, or performance benchmarks complete.

- [x] **Step 3: Run plan self-review** and remove every placeholder match while confirming all type names and bounds match implementation.

```powershell
rg.exe -n "T[D]B|T[O]DO|implement[ ]later|appropriat[e]|similar[ ]to" docs/superpowers/plans/2026-09-01-flexui-completion-wakeup.md
```

- [x] **Step 4: Run final verification** with `git diff --check`, CodeGraph sync/affected, focused Debug/ASan and Release tests, TurboScript-on desktop/controller regressions, and feature-off builds. Restore `win-dev-user` to its standard GPU-test setting after the gated test run.

## Compatibility, Migration, and Rollback

- Existing mailbox construction remains source compatible because wakeup is an optional final argument. The notifier structs and appended enum value do not change earlier enum numeric values.
- Headless and non-gCanvas applications receive no callback by default. Raw completion polling remains available and unchanged.
- The gCanvas host owns automatic scripted delivery only when `on_service_completion` is present. Without that export it leaves records untouched for explicit raw polling.
- One accepted completion adds one indirect `noexcept` call only when a notifier is configured. No notification occurs on queue rejection.
- Rollback removes the optional notifier argument/builder setter and host drain without changing completion record layout, plugin ABI, request token semantics, persisted data, or TurboScript record schema.
