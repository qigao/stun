# Stun

Stun is a native C/C++ declarative application and GUI stack built around **FlexUI** and **gCanvas**.

The application authoring model intentionally follows familiar modern Web UI concepts without embedding a browser, DOM, JavaScript engine, Node.js, or npm runtime:

```text
XML UI + CSS / Tailwind CSS + TurboScript
                  |
                  v
          native FlexUI runtime
                  |
          RenderCommandList
                  |
                  v
               gCanvas
          OpenGL / Vulkan
```

## Application model

- **XML** is the canonical declarative UI source format.
- **CSS** owns normal style, cascade, state styling, transitions, and animation declarations.
- **Tailwind CSS compatibility** lets applications reuse modern utility-first design patterns through standard `class="..."` markup. Stun compiles supported Tailwind semantics natively; installed applications do not require Node/npm.
- **TurboScript** is the typed controller language for events, application state, bounded UI mutations, and capability-gated service commands. It does not expose an unrestricted DOM or renderer API.
- **Salts::Unicode** is the target shared UTF-8/Unicode semantic foundation. FlexUI text layout builds grapheme, word, line, bidi, shaping, cursor, and selection behavior above that versioned Unicode boundary.
- **gCanvas** owns native window/GPU drawing resources. UI semantics, layout, widgets, styling, and application state remain in FlexUI.

## Runtime ownership

FlexUI keeps one Element tree plus computed style as the UI source of truth. XML compiles into an immutable `CompiledUiProgram`; widget/container semantics come from `WidgetRegistry`; CSS and Tailwind utilities feed the same cascade; TurboScript receives immutable typed snapshots and returns transactional effects.

Application load and reload are candidate-based: XML, style, script, widgets, resources, and services are validated before an application instance is published. Failed candidates do not partially mutate the active application.

## Current architecture work

- [#15 — Unicode/Text convergence on Salts::Unicode](https://github.com/qigao/stun/issues/15)
- [#16 — typed XML container/widget/text schema](https://github.com/qigao/stun/issues/16)
- [#17 — Tailwind v4-compatible native utility compiler](https://github.com/qigao/stun/issues/17)
- [#1 — FlexUI Desktop umbrella](https://github.com/qigao/stun/issues/1)

See `flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md` and `flexUI/docs/ARCHITECTURE.md` for the runtime boundaries.

## Charts

Chart engines live under `charts/`. Mermaid chart engines are documented in `charts/mermaid/README.md`.
