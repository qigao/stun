# FlexUI gCanvas Application Routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route validated gCanvas callback values through `DesktopApplication` without binding the incomplete `gCanvas::Window` listener lifecycle.

**Architecture:** `GCanvasInputNormalizer` remains the only owner of native conversion and cached pointer position. A controller-side `GCanvasApplicationInputRouter` borrows one `DesktopApplication`, checks its owner thread before touching normalizer or `Box` state, and preserves either the normalization error or full application dispatch error.

**Tech Stack:** C++17, FlexUI Core/Controller/Desktop modules, gCanvas event contract, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 12, 14-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P5/P6.

## Global Constraints

- `DesktopApplication`, `Box`, router, and normalizer state remain confined to the application owner thread.
- A rejected cross-thread pointer event must not update the normalizer's cached pointer position.
- Normalization failures retain `GCanvasInputError`; application failures retain `DesktopApplicationError`.
- A character event with no eligible focused editor is successful with `processed == false`.
- Resize updates logical viewport state and invalidates the Box; it does not render or present a frame.
- `FlexUI::Core` does not link gCanvas Window, GLFW, or Controller.

---

### Task 1: Application-aware gCanvas input router

**Files:**
- Create: `flexUI/include/flexUI/gcanvas_application_input.h`
- Create: `flexUI/modules/desktop/gcanvas_application_input.cpp`
- Create: `flexUI/tests/test_gcanvas_application_input.cpp`
- Modify: `flexUI/modules/desktop/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `GCanvasInputNormalizer`, `DesktopApplication::is_owner_thread()`, `DesktopApplication::dispatch_event()`, and the application text bridge.
- Produces: `GCanvasApplicationInputRouter::mouse_move`, `mouse_button`, `key`, `character`, `scroll`, and `resize`, each returning `GCanvasApplicationInputResult`.

- [x] **Step 1: Write failing routing and ownership tests**

```cpp
GCanvasApplicationInputRouter router(*application);
const auto moved = router.mouse_move({12.0, 24.0});
check(moved);
check_true(moved.processed);

GCanvasApplicationInputResult wrong_thread;
std::thread worker([&] { wrong_thread = router.mouse_move({4.0, 8.0}); });
worker.join();
check_false(wrong_thread);
check_equal(wrong_thread.error.application_error.code,
            DesktopApplicationErrorCode::WrongThread);

const auto wheel = router.scroll({0.0, 1.0});
check_false(wheel);
check_equal(wheel.error.input_error.code,
            GCanvasInputErrorCode::MissingPointerPosition);
```

Use a fresh router for the wrong-thread case so the subsequent wheel assertion proves rejected input did not mutate pointer state. Also cover focused/unfocused character input, key repeat, mouse button routing, invalid native values, and positive/invalid resize.

- [x] **Step 2: Run the focused target and confirm the missing API fails**

Run: `cmake --build --preset win-dev-user --target test_gcanvas_application_input`.

Expected: compilation fails because `flexUI/gcanvas_application_input.h` does not exist.

- [x] **Step 3: Implement the router and separate target**

```cpp
enum class GCanvasApplicationInputErrorCode {
  None,
  NormalizationFailed,
  ApplicationFailed,
};

struct GCanvasApplicationInputResult {
  bool processed = false;
  GCanvasApplicationInputError error;
  explicit operator bool() const noexcept { return !error; }
};
```

Create `FlexUI::GCanvasApplicationInput`, linking `FlexUI::Controller` and `FlexUI::GCanvasInput`. Every public route first rejects a non-owner thread, then normalizes, then dispatches or applies viewport state. Do not add a fallback or log at this intermediate boundary.

- [x] **Step 4: Run focused and adjacent regression tests**

Run `test_gcanvas_application_input`, `test_gcanvas_input`, `test_desktop_application`, `test_event_dispatcher`, and `test_host_bridge` under `win-dev-user`; run the application tests under `win-dev-turboscript-user` and the same adjacent set under `win-release-user`.

### Task 2: Document the completed boundary and remaining Window prerequisites

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: the tested router ownership and error contract.
- Produces: an explicit prerequisite list for `GCanvasWindowHost` instead of implying that native focus/capture/lifetime are complete.

- [x] **Step 1: Record facts and unchecked prerequisites**

Document that application routing is implemented while real Window binding still requires listener-scoped removal, focus events, native capture synchronization, close handling, and GPU smoke coverage. Keep those P5 entries unchecked.

- [x] **Step 2: Verify formatting, affected files, and the plan checklist**

Run `clang-format --dry-run --Werror` on new C++ files, `git diff --check`, `codegraph sync .`, and `codegraph affected` for the router source. Confirm no implementation step remains unchecked before reporting completion.
