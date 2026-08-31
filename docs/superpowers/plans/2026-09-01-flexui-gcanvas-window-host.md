# FlexUI gCanvas Window Host Implementation Plan

> **For Codex:** Execute this plan inline. Keep the host in the desktop adapter layer so FlexUI Core remains independent of GLFW and native window APIs.

**Goal:** Add a production-shaped desktop host that owns a gCanvas window and GPU context, drives one `DesktopApplication`, routes native input, synchronizes pointer capture, and performs deterministic close/shutdown.

**Architecture:** `GCanvasWindowHost` is a facade/Pimpl in `FlexUI::GCanvasWindowHost`. Its implementation owns resources in dependency order: `Window -> Context(borrowed) -> Flex renderer -> DesktopApplication -> input router -> listener subscriptions`. The application remains the sole lifecycle source of truth; native callbacks only route input or request close, while `pump_once()`/`run()` perform frame and shutdown transitions on the owner thread.

**Tech Stack:** C++17, FlexUI controller/core, Flex gCanvas render engine, gCanvas Window/OpenGL/Vulkan, TinyTest, CMake presets.

---

### Task 1: Add the application frame boundary and renderer injection

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

- [x] Add `DesktopApplication::frame(double)` with owner-thread, lifecycle, finite/non-negative delta checks and nested controller errors.
- [x] Add additive `DesktopApplicationBuilder::renderer(flex::Renderer*)` injection so a host can bind the renderer after creating its native context.
- [x] Test successful, invalid, closed-state, failure, and wrong-thread frame calls.

### Task 2: Implement the concrete gCanvas desktop host

**Files:**
- Create: `flexUI/include/flexUI/gcanvas_window_host.h`
- Create: `flexUI/modules/desktop/gcanvas_window_host.cpp`
- Modify: `flexUI/modules/desktop/CMakeLists.txt`

- [x] Define bounded frame modes/configuration and structured build/runtime errors.
- [x] Create and own window, context, renderer, application, router, and scoped subscriptions with dependency-safe teardown.
- [x] Route resize, pointer, keyboard, text, scroll, focus, and close callbacks without allowing exceptions to cross the GLFW boundary.
- [x] Synchronize native pointer capture after pointer input and every frame.
- [x] Implement deterministic `pump_once()`, event-driven/continuous `run()`, close request, and application shutdown.

### Task 3: Add real GPU host tests and build wiring

**Files:**
- Create: `flexUI/tests/test_gcanvas_window_host.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

- [x] Exercise hidden OpenGL and Vulkan host creation, initial viewport, first frame, close, and shutdown.
- [x] Exercise pointer-capture synchronization and owner-thread rejection.
- [x] Exercise invalid host configuration without opening a window.
- [x] Build and run the smallest relevant TinyTest targets, then adjacent Debug ASan/Release suites.

### Task 4: Synchronize desktop design documentation

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

- [x] Document ownership, frame modes, error/lifecycle semantics, and the Core/GLFW dependency boundary.
- [x] Mark only implemented and verified P5 items complete; retain later IME/clipboard/dialog/DPI work as open.
