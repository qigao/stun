# FlexUI Host Input Routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route normalized desktop input through `DesktopApplication` with deterministic pointer capture, structured errors, and a backend-neutral contract before binding a real gCanvas window.

**Architecture:** `EventDispatcher` remains the sole owner of hit-test, hover, active, focus, and capture state. A controller-side host adapter gates text/IME against the active `Box` but dispatches accepted events through `DesktopApplication`; backend-specific adapters only normalize native values into `flexUI::Event` and never touch controller internals.

**Tech Stack:** C++17, FlexUI Core/Controller, TinyTest, CMake Presets, gCanvas window event types.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 12, 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5/P6.

## Global Constraints

- UI state, `DesktopApplication`, `Box`, controller, and host input dispatch remain owner-thread-only.
- Pointer coordinates are finite logical pixels; framebuffer size and DPI are separate host metrics.
- Widget-consumed events do not reach C++ global callbacks or script handlers.
- Capture changes routing target, while physical hit-testing remains the source for hover and click matching.
- Existing `Box`-only host helpers remain source compatible.
- No GLFW or gCanvas window dependency enters `flexUI` Core.

---

### Task 1: Deterministic pointer capture routing

**Files:**
- Modify: `flexUI/include/flexUI/event_dispatcher.h`
- Modify: `flexUI/src/event_dispatcher.cpp`
- Create: `flexUI/tests/test_event_dispatcher.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Box::dispatch_event(Event&)`, `Widget::wants_mouse_capture()`.
- Produces: one widget invocation per route element; capture is released after a consuming `MouseUp`; physical hit target controls hover and click matching.

- [x] **Step 1: Write failing capture tests**

```cpp
Event down = Event::mouse_down(20.0F, 20.0F);
box.dispatch_event(down);
Event move = Event::mouse_move(220.0F, 20.0F);
box.dispatch_event(move);
check_equal(probe->move_calls, 1);
check(move.target == capture_element);
check(box.hovered_element() == physical_element);
```

Also release outside the capture bounds and verify `capturing_element() == nullptr` and no click callback.

- [x] **Step 2: Run the focused test and confirm the old implementation fails**

Run: `cmake --build --preset win-dev-user --target test_event_dispatcher && ctest --preset win-dev-user -R '^test_event_dispatcher$' --output-on-failure`

Expected: duplicate delivery, wrong target, or stale capture assertion fails.

- [x] **Step 3: Separate physical hit target from routed target**

```cpp
Element *pointer_hit_target = hit_test(root, event.x, event.y);
Element *target = capturing_ != nullptr ? capturing_ : pointer_hit_target;
event.target = target;
```

Pass both targets to special-state handling, route the event exactly once, then reconcile capture after widget processing.

- [x] **Step 4: Run focused and adjacent event tests**

Run `test_event_dispatcher`, `test_host_bridge`, `test_desktop_application`, and `test_style_engine` under `win-dev-user`.

### Task 2: Application-aware text and IME bridge

**Files:**
- Create: `flexUI/include/flexUI/application_host_bridge.h`
- Create: `flexUI/modules/controller/application_host_bridge.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `DesktopApplication::dispatch_event(Event&)`, `host::box_wants_text_input(Box*)`.
- Produces: `HostApplicationDispatchResult` preserving `DesktopApplicationError` and distinguishing ignored input from successful dispatch.

- [x] **Step 1: Write failing host/application tests**

```cpp
const auto ignored = host::dispatch_text_input_if_focused(*application, "x");
check(ignored);
check_false(ignored.dispatched);

application->box().set_focus(input);
const auto accepted = host::dispatch_text_input_if_focused(*application, "x");
check(accepted);
check_true(accepted.dispatched);
check_equal(input_widget->text(), "x");
```

- [x] **Step 2: Run the focused test and confirm missing symbols fail**

Run: `cmake --build --preset win-dev-user --target test_desktop_application`.

Expected: compilation fails because the application host bridge is not defined.

- [x] **Step 3: Implement the thin adapter**

```cpp
struct HostApplicationDispatchResult {
  bool dispatched = false;
  DesktopApplicationError error;
  explicit operator bool() const noexcept { return !error; }
};
```

Provide text input and composition start/update/end functions. Gate using the active `Box`; accepted input must call `DesktopApplication::dispatch_event()` and retain its full error.

- [x] **Step 4: Run feature-off and TurboScript-on tests**

Run `test_desktop_application`, `test_turboscript_application`, and the XML/TBS demo under their public presets.

### Task 3: gCanvas native event normalizer

**Files:**
- Create: `flexUI/include/flexUI/gcanvas_input.h`
- Create: `flexUI/modules/desktop/gcanvas_input.cpp`
- Create: `flexUI/modules/desktop/CMakeLists.txt`
- Create: `flexUI/tests/test_gcanvas_input.cpp`
- Modify: `flexUI/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `gcanvas::mouse_move_event`, `mouse_button_event`, `key_event`, `char_event`, and `scroll_event`.
- Produces: validated logical-pixel `Event` values or a structured normalization error; wheel events use the last valid logical pointer position.

- [x] **Step 1: Test supported mappings and rejected inputs**

```cpp
auto result = normalizer.mouse_button({gcanvas::MOUSE_BUTTON_LEFT,
                                       gcanvas::ACTION_PRESS, {}, 12.0, 24.0});
check(result);
check(result.event.type == EventType::MouseDown);
check_equal(result.event.x, 12.0F);
```

Cover non-finite coordinates, unsupported buttons/actions, key repeat, modifiers, null UTF-8, resize metrics, and wheel cursor reuse.

- [x] **Step 2: Implement strict scalar conversions and enum mapping**

Reject values that cannot be represented as finite `float`; do not clamp, guess a button, or auto-switch backend.

- [x] **Step 3: Add the optional desktop adapter target**

Link it to `FlexUI::Core` and `gCanvas::Core`; a later Window host may consume it without making
the normalization contract depend on GLFW.

- [x] **Step 4: Verify Debug/ASan, TurboScript ON, and Release**

Run the focused normalizer and application tests, then the adjacent host/event suites with the public presets. Run `git diff --check` and `codegraph affected` before completion.
