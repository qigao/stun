# Flex DSL Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the old `.flex` language and compiler after UI, animation and physics consumers have migrated, without deleting the native Flex runtime.

**Architecture:** Migrate every source-format consumer to XML/CSS/TBS or direct native runtime APIs, then remove parser/compiler targets from the leaves inward. Timeline, curves, physics, MIR execution, renderer and runtime remain independent libraries.

**Tech Stack:** C++20, CMake, FlexUI XML/CSS/TBS frontend, Flex Timeline/Physics/MIR runtime, TinyTest/CTest.

**Spec:** `flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md`

## Global Constraints

- Do not delete the DSL while any shipped target, example, tool or install-tree consumer requires it.
- Preserve native Timeline/Track/Curve/Physics/MIR public behavior and tests.
- No parser fallback after the compatibility feature is removed.
- Record every removed or replaced public target/header and provide one-release migration notes.

---

## Task 1: Build a complete consumer inventory

- [ ] Enumerate `.flex` files and `Flex::Compiler`/`Flex::DSL` references with `rg.exe` and `fd.exe`.
- [ ] Classify each consumer as UI, animation, physics, tooling, example or obsolete ThorVG artifact.
- [ ] Map each retained behavior to XML/CSS/TBS or a native runtime API and name its parity test.
- [ ] Identify installed public headers and exported CMake targets that form compatibility obligations.

## Task 2: Migrate UI and application consumers

- [ ] Convert FlexPlayer and FlexUI examples to XML + CSS + TBS.
- [ ] Convert FlexChart UI consumers or document an explicit native API replacement.
- [ ] Keep golden semantic tests comparing legacy and XML compiled programs during migration.
- [ ] Remove the legacy UI loader only after all application entry points default to XML.

## Task 3: Migrate animation and physics source features

- [ ] Define CSS keyframe/transition lowering for common UI animation behavior.
- [ ] Define a bounded declarative resource schema for native Timeline, curve and physics assets not expressible as CSS.
- [ ] Add typed TBS controller commands for starting, pausing, seeking and parameterizing native programs.
- [ ] Port DSL runtime fixtures to direct IR/resource fixtures and verify numerical parity.

## Task 4: Retire compiler-facing tools and formats

- [ ] Replace or remove `flex-compiler` and `.flexb` workflows based on remaining deployment needs.
- [ ] Remove imports, parser diagnostics and DSL binary serialization only after their consumers are gone.
- [ ] Publish migration notes for removed syntax, headers, targets and file extensions.

## Task 5: Remove DSL implementation and exports

- [ ] Remove parser/lexer/AST sources and re2c/Lemon generation from `flex/modules/dsl`.
- [ ] Remove `flex_compiler`, `flex_dsl`, `Flex::Compiler` and `Flex::DSL` exports.
- [ ] Remove DSL includes and source-loading APIs from `flex/include/flex.h` and engine code.
- [ ] Remove obsolete `.flex` fixtures and dependency declarations.
- [ ] Confirm no remaining reference with `rg.exe` and `fd.exe`.

## Task 6: Release-gate verification

- [ ] Configure and build supported Debug+ASan and Release preset matrices.
- [ ] Run Flex runtime, animation, physics, render, FlexUI and install-tree consumer tests.
- [ ] Compare retained animation/physics numerical fixtures before and after removal.
- [ ] Verify package configs contain no parser dependencies or removed targets.
- [ ] Verify XML + CSS + TBS desktop example loads, handles events and runs native animation.
