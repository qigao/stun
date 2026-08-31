# FlexUI XML Frontend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make XML the FlexUI structure format while preserving one immutable UI IR and existing runtime behavior.

**Architecture:** Add a thin XML adapter above parser-independent definitions, then instantiate typed widgets through a registry. The legacy `.flex` parser remains a temporary producer of the same IR until consumer migration finishes.

**Tech Stack:** C++20, FlexUI, pugixml, Flex native MIR/runtime, TinyTest, CMake presets.

**Spec:** `flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md`

## Global Constraints

- Keep Box/widget state owned by the UI thread.
- Parse and validate before publishing any tree or program.
- Reuse `UiDocumentDefinition` and `CompiledUiProgram`; do not create XML-specific runtime state.
- Preserve old `.flex` behavior until the retirement plan's deletion gate is met.
- Keep TurboScript out of XML parsing, layout, style cascade and per-frame animation evaluation.

---

## Task 1: Extract the parser-independent semantic compiler

**Files:**

- Modify: `flexUI/include/flexUI/ui_document.h`
- Modify: `flexUI/src/ui_document.cpp`
- Test: `flexUI/tests/test_ui_document.cpp`

- [x] Add `compile_ui_definition(UiDocumentDefinition, resources, limits)` with value ownership.
- [x] Validate document name, node tree, properties, limits and resources before lowering.
- [x] Make legacy `compile_ui_document` parse, then delegate to the new compiler.
- [x] Remove `AstNode` use from definition validation.
- [x] Test event, binding and resource lowering from a manually constructed definition.
- [x] Test ownership by mutating the caller's source object after compilation.
- [x] Run `test_ui_document` in Debug+ASan and Release presets.

## Task 2: Add an XML source adapter

**Files:**

- Create: `flexUI/include/flexUI/ui_xml.h`
- Create: `flexUI/src/ui_xml.cpp`
- Modify: `flexUI/CMakeLists.txt`
- Create: `flexUI/tests/test_ui_xml.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

- [x] Confirm the installed pugixml imported target and use it through `find_package`.
- [x] Parse exactly one document root plus an optional bounded resource section.
- [x] Convert XML namespaced event/binding attributes to canonical definition keys.
- [x] Reject unknown structural nodes, duplicate IDs/attributes, invalid scalar types and embedded NUL.
- [x] Preserve source locations where pugixml exposes enough information; otherwise return the closest byte offset and document the limitation.
- [x] Compile via `compile_ui_definition` and prove XML and legacy sources lower to equivalent programs.
- [x] Run focused XML and UI document tests in Debug+ASan and Release.

## Task 3: Introduce a typed WidgetRegistry

**Files:**

- Create: `flexUI/include/flexUI/widget_registry.h`
- Create: `flexUI/src/widget_registry.cpp`
- Modify: `flexUI/include/flexUI/ui_document.h`
- Modify: `flexUI/src/ui_document.cpp`
- Create: `flexUI/tests/test_widget_registry.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

- [x] Define a narrow factory interface returning detached, host-owned widgets.
- [x] Register built-in tags explicitly and reject duplicate registrations.
- [x] Inject the registry into instantiation; do not use a service locator or global mutable singleton.
- [x] Preserve transactional detached-tree construction and Box ownership transfer.
- [x] Test concrete widget types, unknown tags, factory failure, duplicate registration and unchanged Box on failure.

## Task 4: Make XML the desktop application entry format

**Files:**

- Create: `flexUI/include/flexUI/application.h`
- Create: `flexUI/include/flexUI/application_turboscript.h`
- Create: `flexUI/modules/controller/application.cpp`
- Create: `flexUI/modules/controller/application_turboscript.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`
- Test: `flexUI/tests/test_desktop_application.cpp`
- Test: `flexUI/tests/test_turboscript_application.cpp`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

- [x] Add `DesktopApplicationBuilder::xml_entry()` as the default entry API and keep
      `legacy_flex_entry_compatibility()` as the only legacy opt-in.
- [x] Keep `FlexUI::Core <- FlexUI::Controller <- FlexUI::ControllerTurboScript` dependency
      direction: the generic application accepts an injected `ScriptModuleFactory`; the
      TurboScript target only supplies a factory adapter.
- [x] Build a detached candidate containing compiled UI, strict CSS, typed widgets, mutation
      host/engine and a mounted controller; publish only after all stages succeed.
- [x] Validate XML semantics, typed widget tags, strict CSS diagnostics and controller exports
      before publication; preserve nested structured errors and source locations.
- [x] Make reload atomic by building a complete replacement state and publishing it with one
      no-fail owner swap; a failed reload leaves the active Box, program and controller intact.
- [x] Test typed XML construction, strict CSS rejection, missing exports, mount failure, static
      UI validation, failed/successful reload and explicit legacy compatibility.
- [x] Test the real TurboScript adapter with XML + CSS + TBS when the feature is enabled.
- [x] Update design/plan documentation and verify feature-on and feature-off build matrices.

### Task 4b: Connect native events to the application controller

- [x] Preserve the existing Box application callback and add a separate framework observer.
- [x] Route only unconsumed events, with target-to-ancestor bubbling and synthesized click.
- [x] Carry pointer-free `target` and `current_target` handles through the controller and TBS ABI.
- [x] Return owner-thread, native callback and controller failures from
      `DesktopApplication::dispatch_event()` as structured application errors.
- [x] Test click ordering, bubbling, widget consumption, controller fault, feature-off behavior and
      real TurboScript mutation through the application facade.

## Task 5: Verify installation and performance boundaries

**Files:**

- Modify: install/export CMake files identified by target inspection.
- Add: install-tree consumer fixture under the repository's existing test convention.
- Add: load/instantiate benchmark under `flexUI/tests/`.

- [ ] Verify public headers do not expose pugixml or TurboScript implementation types.
- [ ] Verify installed targets resolve all transitive dependencies.
- [ ] Benchmark XML parse, semantic compile and typed instantiation separately at typical and maximum supported sizes.
- [ ] Run adjacent FlexUI regression tests, install-tree consumer tests and full relevant CTest presets.
