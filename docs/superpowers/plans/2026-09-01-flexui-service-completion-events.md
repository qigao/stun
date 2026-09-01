# FlexUI Service Completion Events Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver resolved application-service completions to an optional TurboScript `on_service_completion` export through the existing transactional controller effect pipeline.

**Architecture:** `ApplicationServiceCompletion` remains the owning value restored by the application request table. `ScriptController` pre-resolves one optional lifecycle-adjacent export, validates a borrowed immutable completion snapshot, and applies returned UI mutations/application commands with the same reserve-before-commit protocol as native events. `DesktopApplication` exposes an owner-thread single-record dispatch operation; existing raw polling remains available for non-script hosts, but the two APIs consume the same mailbox and must not be mixed for one application.

**Tech Stack:** C++17, FlexUI Controller, TurboScript C ABI, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 8.4, 13.2-13.4, 15-16 and `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P6 Service 调用.

## Global Constraints

- Completion consumption and controller dispatch run only on the application owner thread.
- `on_service_completion` is optional, resolved once during controller load, accepts exactly one record, and is never looked up on the hot path.
- The script record is `{request_id, status, payload, error_code, error_message}`; status is exactly `succeeded`, `failed`, or `cancelled`.
- Strings remain owned by `ApplicationServiceCompletion`; the adapter exposes borrowed explicit-length views only for the synchronous TurboScript call.
- A successful or failed service status is data, not a host fallback trigger. Only adapter/controller failures fault the controller.
- The callback may return ordinary bounded mutations and commands; commands are reserved before UI mutation preparation and published only after UI commit.
- Reload/close keep the existing generation and mailbox fact source. Stale/closed completions never reach a replacement or closed controller.
- One dispatch call consumes at most one record; callers bound work by choosing how often to call it.
- No plugin, `Element`, renderer, gCanvas, or TurboScript private type crosses the public completion value boundary.

---

### Task 1: Controller completion callback contract

**Files:**
- Modify: `flexUI/include/flexUI/controller.h`
- Modify: `flexUI/modules/controller/controller.cpp`
- Test: `flexUI/tests/test_script_controller.cpp`

**Interfaces:**
- Consumes: `ApplicationServiceCompletion`, `IScriptModule::resolve_export()`, `IScriptModule::call()`, and `ScriptController::Impl::apply_effects()`.
- Produces: `ScriptCallbackKind::ServiceCompletion`, `ScriptCallContext::service_completion`, `ControllerStage::ServiceCompletion`, `ScriptController::dispatch_service_completion(const ApplicationServiceCompletion&)`, and `has_service_completion_handler()`.

- [x] **Step 1: Add failing TinyTest cases** proving load resolves optional `on_service_completion` once, absence is a successful no-op, success/failure/cancelled records remain immutable during dispatch, oversized fields and zero request IDs fail before the module call, callback failure faults the controller, and callback effects use the existing transaction order.
- [x] **Step 2: Build and run `test_script_controller`** with `win-dev-user`; expect compilation failure for the missing callback kind/API.
- [x] **Step 3: Implement the controller contract** by including `application_service.h`, adding completion byte limits derived from `ApplicationCompletionLimits`, resolving `on_service_completion` during `load()`, clearing its handle during `clear()`, validating the snapshot, synchronously calling it with a borrowed pointer, and routing its effects through `apply_effects()`.
- [x] **Step 4: Run `test_script_controller`** under Debug/ASan and Release and require all cases to pass.

### Task 2: TurboScript value adapter

**Files:**
- Modify: `flexUI/modules/controller/controller_turboscript.cpp`
- Test: `flexUI/tests/test_turboscript_controller.cpp`

**Interfaces:**
- Consumes: `ScriptCallbackKind::ServiceCompletion` and `ScriptCallContext::service_completion` from Task 1.
- Produces: a one-argument TurboScript record with fields `request_id`, `status`, `payload`, `error_code`, and `error_message`.

- [x] **Step 1: Add failing interpreter and JIT tests** where `on_service_completion(value)` verifies every success/failure field and returns a mutation; add an arity-mismatch case and an out-of-range `request_id` adapter failure.
- [x] **Step 2: Build and run `test_turboscript_controller`** with `win-dev-turboscript-user`; expect the new tests to fail before the adapter implementation.
- [x] **Step 3: Extend `make_arguments()`** with dedicated fixed-size completion record storage, exact status mapping, int64 range validation, one-argument arity, and explicit errors for a missing snapshot or invalid enum.
- [ ] **Step 4: Run `test_turboscript_controller`** under interpreter/JIT Debug/ASan and Release. Debug/ASan interpreter/JIT passes; Release remains blocked because the installed Release TurboScript header lacks the value/result/module ABI declarations present in the Debug package.

### Task 3: DesktopApplication single-completion dispatch

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Test: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `try_receive_service_completion()` and `ScriptController::dispatch_service_completion()`.
- Produces: `DesktopApplicationCompletionDispatchResult` and `DesktopApplication::try_dispatch_service_completion()`.

- [x] **Step 1: Add failing application tests** proving one call consumes one resolved completion, invokes the optional handler once, returns the owning completion, commits handler mutations, exposes failed status to the module without fallback, reports `Empty` without a call, reports `Closed` after close, rejects non-owner calls, and never delivers stale pre-reload completions.
- [x] **Step 2: Build and run `test_desktop_application`** with `win-dev-user`; expect compilation failure for the missing dispatch result/API.
- [x] **Step 3: Implement owner-thread dispatch** by preserving raw poll errors, returning `Ready/Empty/Closed`, determining `dispatched` from the pre-resolved optional handler, and nesting controller failure in `DesktopApplicationError` with a distinct service-completion stage/code. Do not retry, fallback, or requeue a consumed completion.
- [ ] **Step 4: Run application, service request, completion mailbox, controller, TurboScript application, and gCanvas host regressions in Debug/ASan and Release.** Core and gCanvas application-input regressions pass in both configurations, and TurboScript application/controller pass in Debug/ASan; the standard preset does not generate the GPU-gated gCanvas window-host target, while Release TurboScript is blocked by the installed header mismatch above.

### Task 4: Protocol documentation and verification

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`
- Modify: `docs/superpowers/plans/2026-09-01-flexui-service-completion-events.md`

**Interfaces:**
- Consumes: verified callback, adapter, transaction, owner-thread, close, and reload behavior from Tasks 1-3.
- Produces: the documented host pump contract and updated P4/P6 evidence checklist.

- [x] **Step 1: Document the exact handler name/schema**, single-consumer rule, optional-handler result, owner thread, limits, fault behavior, no-fallback rule, same-callback transaction order, and event-loop wakeup responsibility.
- [x] **Step 2: Mark verified checklist entries** for required capability build validation, completion event conversion, stale/closed cancellation, and explicit service-error handling; keep tagged payload/schema and event-driven native wakeup work unchecked.
- [x] **Step 3: Run self-review** with `rg.exe -n "T[D]B|T[O]DO|implement[ ]later|appropriat[e]|similar[ ]to"` on this plan, confirm type names match implementation, and remove every matched placeholder.
- [x] **Step 4: Run final verification**: `git diff --check`, CodeGraph sync/affected analysis, focused Debug/ASan and Release tests, plus feature-off controller/application tests.

## Compatibility, Migration, and Rollback

- Existing event, frame, mount, unmount, raw completion polling, no-script, and no-plugin behavior remains available. Applications opt into script delivery by exporting `on_service_completion` and calling `try_dispatch_service_completion()` from their owner-thread host loop.
- Raw `try_receive_service_completion()` and scripted dispatch consume the same mailbox; documentation forbids mixing them for one application because there is one authoritative consumer.
- The optional callback adds one export lookup during controller load and no per-frame call. When no completion is dispatched, no script callback occurs.
- Rollback removes the callback kind/context field, optional handle, adapter record conversion, application dispatch API, tests, and documentation without changing the plugin ABI, mailbox record, request token, manifest, or persisted format.
