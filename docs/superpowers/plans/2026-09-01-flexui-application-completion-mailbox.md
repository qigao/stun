# FlexUI Application Completion Mailbox Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded worker-to-UI completion mailbox whose generation and shutdown rules make service completion safe across application close and reload.

**Architecture:** `ApplicationCompletionMailbox` owns a TurboUtils Disruptor configured as a bounded multi-producer/single-consumer queue. Producers copy or move complete value-semantic records into queue-owned heap objects, while the application owner thread is the only consumer and lifecycle controller. `DesktopApplication` owns one mailbox, closes it before entering `CloseRequested`, and advances its generation before publishing a reloaded controller.

**Tech Stack:** C++17, TurboUtils Disruptor, FlexUI Controller, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 9.2, 13.2, and 15; `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P4-P6.

## Global Constraints

- The topology is multiple service-worker producers and exactly one application-owner-thread consumer.
- Queue capacity is fixed, non-zero, and a power of two; queue-full returns an explicit error and never blocks, drops, overwrites, or grows.
- `try_post()` takes ownership only after successful publication; all caller buffers are copied or moved into a host-owned record before the ABI call returns.
- Request tokens contain a non-zero host request ID and non-zero application generation; stale generations never reach a replacement controller.
- `close()` stops acceptance, waits only for already-entered non-blocking publishers, cancels every published record, and is idempotent.
- `advance_generation()` stops acceptance, quiesces publishers, cancels the old generation, installs a strictly newer generation, and resumes acceptance.
- Mailbox destruction requires the PluginHost to have stopped and joined workers; no caller may retain or invoke a mailbox pointer during destruction.
- This phase does not convert completions into TurboScript events and does not freeze the DLL C ABI.

---

### Task 1: Bounded completion mailbox contract and implementation

**Files:**
- Create: `flexUI/include/flexUI/application_completion.h`
- Create: `flexUI/modules/controller/application_completion.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Create: `flexUI/tests/test_application_completion.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `disruptor_create`, publisher try-claim/publish, worker try-claim/release, and `TurboUtils::Core`.
- Produces: `ApplicationRequestToken`, `ApplicationCompletion`, `ApplicationCompletionLimits`, `ApplicationCompletionMailbox`, structured post/poll/control results, and mailbox statistics.

- [x] **Step 1: Add TinyTest coverage for invalid construction, validation, FIFO delivery, exact-capacity saturation, wrong-thread polling, close cancellation, generation rollover, stale-token rejection, and concurrent close/post.**
- [x] **Step 2: Build `test_application_completion` and confirm it fails because the public contract is absent.**
- [x] **Step 3: Implement the mailbox with pointer-sized Disruptor entries, an active-publisher quiescence gate, explicit ownership transfer, checked string budgets, and atomic statistics.**
- [x] **Step 4: Run the focused mailbox test under the configured Debug/ASan and Release build trees.**

### Task 2: DesktopApplication lifecycle integration

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `ApplicationCompletionMailbox::create()`, `close()`, and `advance_generation()`.
- Produces: `DesktopApplication::completion_mailbox()`, `DesktopApplication::generation()`, completion limits in `DesktopApplicationLimits`, and nested application errors for mailbox initialization or generation failure.

- [x] **Step 1: Add tests proving close drains queued completions before `CloseRequested`, late completions return `Closed`, reload increments generation, and old-generation completions return `StaleGeneration`.**
- [x] **Step 2: Build `test_desktop_application` and confirm the missing integration API fails compilation.**
- [x] **Step 3: Create the mailbox during detached application publication, close it before the lifecycle state changes, and advance it before the no-fail candidate swap.**
- [x] **Step 4: Run application, command, controller, input-router, and window-host regression tests.**

### Task 3: Protocol documentation and verification gate

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: verified mailbox ownership, backpressure, generation, and close behavior.
- Produces: the exact C++ contract that PluginHost and the future C ABI adapter may rely on.

- [x] **Step 1: Document data unit, fact source, ownership, topology, FIFO scope, capacity, backpressure, state transitions, cancellation, destruction preconditions, errors, and statistics.**
- [x] **Step 2: Mark only the completed mailbox and close/reload safety checklist items; retain PluginHost, ServiceRegistry, DLL ABI, and TurboScript-event work as pending.**
- [x] **Step 3: Run `git diff --check`, CodeGraph sync/affected analysis, focused Debug/ASan tests, and focused Release tests.**

## Compatibility, Migration, and Rollback

- Existing XML, CSS, TurboScript, command envelopes, rendering, and native window behavior remain unchanged.
- `DesktopApplication` gains a small fixed-capacity mailbox allocation at build time; invalid limits or allocation failure now fail application publication with a structured error.
- Service adapters migrate by copying borrowed DLL buffers into `ApplicationCompletion`, posting with the host-issued token, and handling `QueueFull`, `Closed`, and `StaleGeneration` explicitly.
- Rollback removes the additive completion header/source, application accessors/limits, tests, and documentation without changing persisted data or existing script syntax.
