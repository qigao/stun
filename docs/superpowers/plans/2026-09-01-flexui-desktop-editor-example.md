# FlexUI Desktop Editor Example Implementation Plan

> **For Codex:** Execute this plan incrementally with the repository's CMake presets and TinyTest conventions. Keep the example optional behind its existing feature targets.

**Goal:** Deliver one runnable desktop editor example whose XML view, CSS theme, TurboScript controller, native document-service DLL, PluginHost, and gCanvas OpenGL window execute as one lifecycle-safe application.

**Architecture:** The C++ executable owns startup configuration and source loading. `PluginHostBuilder` validates and starts the document-service DLL, then `GCanvasPluginWindowHost` injects the resulting immutable service registry into `DesktopApplicationBuilder`. TurboScript only returns typed UI mutations and service commands; the DLL owns the service implementation and posts completion through the host ABI. A bounded `--smoke` path creates an invisible window, dispatches a real compiled click binding, pumps the request/completion pipeline, verifies the resulting UI state, and shuts down in application-before-plugin order.

**Tech Stack:** C++17, C11 plugin ABI, FlexUI XML/CSS/controller, TurboScript JIT/interpreter adapter, gCanvas OpenGL window host, TurboUtils filesystem, CMake, CTest.

---

## Task 1: Add external editor assets

**Files:**

- Create: `flexUI/examples/desktop_editor/editor.xml`
- Create: `flexUI/examples/desktop_editor/editor.css`
- Create: `flexUI/examples/desktop_editor/editor.tbs`

- [x] Define a bounded desktop layout with stable element IDs and a compiled `on:click` binding.
- [x] Style the shell, toolbar, editor surface, and status element using supported CSS properties.
- [x] Return a typed `document.save/1` service command from the save handler and update status from `on_service_completion`.

## Task 2: Add the native document-service plugin

**Files:**

- Create: `flexUI/examples/desktop_editor/document_service.c`

- [x] Implement the complete C ABI lifecycle: create, start, submit, stop, join, destroy.
- [x] Publish exactly one bounded `document.save/1` operation and preserve request identity in completion.
- [x] Reject invalid lifecycle state, operation, or oversized payload explicitly without cross-boundary allocation.

## Task 3: Compose the full desktop runtime

**Files:**

- Create: `flexUI/examples/desktop_editor/main.cpp`

- [x] Load XML/CSS/TBS assets through TurboUtils and report the failing path/stage.
- [x] Build the plugin host, TurboScript factory, capability policy, and gCanvas window as one composition.
- [x] Support interactive visible execution and a bounded invisible `--smoke` execution.
- [x] Verify smoke-mode service completion changes the status text before orderly shutdown.

## Task 4: Wire CMake and automated verification

**Files:**

- Create: `flexUI/examples/desktop_editor/CMakeLists.txt`
- Modify: `flexUI/examples/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`

- [x] Build the example only when TurboScript, PluginHost, and gCanvas composition targets all exist.
- [x] Pass relocatable asset-directory and plugin-file names through target-scoped compile definitions.
- [x] Register the smoke executable only under the existing real GPU-test gate and configure matching runtime paths.

## Task 5: Document and validate

**Files:**

- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

- [x] Document the example ownership chain, interaction boundary, and command/completion flow.
- [x] Build the smallest new targets with the matching Windows preset and Visual Studio environment.
- [x] Run the editor smoke test, adjacent TurboScript application test, plugin composition test, and feature-boundary checks.
- [x] Review the resulting diff for placeholders, unbounded loops, ABI ownership leaks, and unrelated edits.
