# FlexUI Editor Controller Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the real desktop editor TurboScript controller enforce one active save, recover after terminal completions, and reject mismatched or duplicate completions.

**Architecture:** `editor.tbs` remains the single owner of its save target and in-flight gate. Because the script permits only one pending save, it may reuse script request ID 1 after a terminal completion; the host's monotonic `ApplicationRequestToken` remains the cross-request and cross-reload identity. A focused TinyTest loads the shipped script asset through the production TurboScript adapter, so the example behavior is tested without duplicating the script in C++.

**Tech Stack:** TurboScript, FlexUI controller adapter, C++17, TinyTest, CMake, CTest.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`

## Global Constraints

- Do not change the public event snapshot, command envelope, completion ABI, or service contract.
- A duplicate save while one request is active returns no effects.
- Script request ID 1 is reusable only after the active request reaches a terminal completion.
- A completion must have an active request and a matching script ID; rejection must not expose partial effects.

---

## Task 1: Add the real-script contract test

**Files:**

- Create: `flexUI/tests/test_desktop_editor_controller.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

- [x] Load `flexUI/examples/desktop_editor/editor.tbs` as bounded test data and create a JIT controller module.
- [x] Resolve `save_document` and `on_service_completion` with their production callback kinds.
- [x] Verify the first save emits `Saving...` and request ID 1, while a concurrent save emits no effects.
- [x] Verify failed completion text, recovery with the released ID, mismatched completion rejection, successful text, and duplicate completion rejection.

The core red expectation is:

```cpp
const auto duplicate = call_completion(module, completion_export, 1);
check_false(static_cast<bool>(duplicate));
```

## Task 2: Enforce the active-completion precondition

**Files:**

- Modify: `flexUI/examples/desktop_editor/editor.tbs`

- [x] Keep the single in-flight script identity aligned with the existing host-token design.
- [x] Require an active request before accepting a completion ID.
- [x] Clear the in-flight gate only after the active completion passes validation.

The intended state transition is:

```text
idle --save(id=1)--> in-flight(1) --failed/succeeded(1)--> idle
in-flight(1) --save--> in-flight(1)
in-flight(1) --completion(id!=1)--> error, state unchanged
idle --completion(id=1)--> error, state unchanged
```

## Task 3: Document and verify the contract

**Files:**

- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

- [x] Record the example controller's state ownership and reusable single-ID rule.
- [x] Run the focused controller contract test, editor smoke test, and adjacent TurboScript application test.
- [x] Reconfigure the feature-disabled preset/build tree and confirm the optional target remains absent.
- [x] Run `git diff --check` and preserve all unrelated user changes.
