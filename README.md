# Stun

Stun is a native C/C++ declarative UI and charting stack built around **FlexUI**, **Flex**, and **gCanvas**.

The desktop application model is intentionally Web-familiar without embedding a browser runtime:

```text
XML UI + CSS / Tailwind classes + TurboScript + assets/services
                         |
                         v
                    FlexUI runtime
                         |
                  Flex / gCanvas
                         |
                  OpenGL / Vulkan
```

## Application authoring

The target authoring surface is:

- **XML** for typed UI structure and widget composition.
- **CSS** for the normal cascade, design tokens, state styles, transitions, and animation declarations.
- **Tailwind-compatible classes** for modern utility-first styling using the standard `class="..."` surface.
- **TurboScript (TBS)** for controller-local state, event handling, bounded UI mutations, and typed application-service commands.
- **gCanvas** for native window/GPU resources and drawing; it is not the UI semantic layer.

A representative application is intended to look like:

```xml
<ui name="Settings"
    xmlns:on="urn:flexui:event"
    xmlns:bind="urn:flexui:binding">
  <column class="min-h-screen bg-background p-6 gap-4">
    <label class="text-2xl font-semibold">Settings</label>

    <card class="rounded-xl border bg-card p-6 shadow-sm">
      <row class="items-center justify-between gap-4">
        <label>Wi-Fi</label>
        <switch id="wifi" bind:checked="wifiEnabled"
                on:change="set_wifi" />
      </row>
    </card>
  </column>
</ui>
```

TurboScript does **not** receive a browser DOM or renderer pointers. It consumes immutable typed event snapshots and returns bounded mutation/command effects. FlexUI remains the owner of UI state.

## Runtime ownership

The primary runtime rules are:

1. **Element tree + computed style are the single UI truth.**
2. **XML compiles to an immutable `CompiledUiProgram` before instantiation.**
3. **Containers, widgets, text, and reusable components have typed schemas/content models.**
4. **CSS and Tailwind utilities share one StyleEngine cascade.**
5. **Salts::Unicode is the Unicode/UTF-8 data boundary; FlexUI must not maintain a second Unicode implementation.**
6. **Text layout/shaping stays above Unicode and below widget rendering.**
7. **TurboScript can request UI mutations or typed services but cannot own `Element*`, renderer, gCanvas context, or GPU resources.**
8. **Application load/reload is transactional: a complete detached candidate is validated before publication.**
9. **There is no silent compatibility fallback between source formats, style semantics, scripts, or services.**

See:

- [FlexUI architecture](flexUI/docs/ARCHITECTURE.md)
- [XML / CSS / TurboScript architecture](flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md)
- [FlexUI Desktop design](flexUI/docs/FLEXUI_DESKTOP_DESIGN.md)
- [TurboScript controller design](flexUI/docs/TURBOSCRIPT_CONTROLLER_DESIGN.md)
- [Tailwind JIT current implementation](flexUI/docs/TAILWIND_JIT_DEFAULT_DESIGN.md)

The active umbrella is [issue #1](https://github.com/qigao/stun/issues/1).

## Major modules

- `flexUI/` — declarative UI, widgets, CSS, layout, text, events, controller/application runtime.
- `flex/` — scene/runtime/animation/render abstractions and gCanvas rendering integration.
- `vendor/gCanvas/` — native canvas/window/GPU substrate.
- `charts/` — chart engines and FlexUI adapters, including Mermaid support.
- `TextEdit/` — text-editor integration.
- `meta_editor/` — diagram/editor tooling.
- `flexui_designer/` — visual designer tooling.
- `tango/` — terminal UI support.

## Current convergence work

The current architecture work is tracked independently:

- [#8 — Unicode/text boundary](https://github.com/qigao/stun/issues/8)
- [#9 — typed XML container/widget/text/component schema](https://github.com/qigao/stun/issues/9)
- [#10 — standard-compatible Tailwind semantics](https://github.com/qigao/stun/issues/10)
- [salts-utils #101 — Unicode grapheme/word/line-break/bidi primitives](https://github.com/qigao/salts-utils/issues/101)

The legacy `.flex` UI source path is migration-only. Native Flex runtime capabilities such as layout, timeline, curve, animation, and physics are independent of that source DSL and remain runtime capabilities.

## Charts

Chart engines are under `charts/`. Mermaid chart engines are documented in:

- [charts/mermaid/README.md](charts/mermaid/README.md)
