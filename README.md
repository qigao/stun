# Stun

Stun is a native C/C++ UI and charting stack built around **FlexUI**, **Flex**,
and **gCanvas**, with D2D and Tango rendering adapters. Its adopted application
design is **XML + native Tailwind + TurboScript**, with explicit target profiles
for pixel and terminal output and no CSS authoring or generated-CSS stage.

The [native Tailwind and backend design](flexUI/docs/TAILWIND_NATIVE_BACKENDS_DESIGN.md)
defines that target. **Runtime migration is not complete:** the current utility
catalog still generates CSS for StyleEngine. This design update does not replace
that implementation or claim new shader/backend support.

The first [native paint compiler slice](flexUI/docs/TAILWIND_NATIVE_PAINT_PROFILE.md)
provides an independent typed compiler and a property-registry bridge for a
limited opacity/color/state profile. The default Box/XML path and backend
effect admission remain migration work.

## Application authoring and ownership

- **XML** describes UI structure and static properties; FlexUI compiles it into an
  immutable `CompiledUiProgram` before instantiation.
- **Tailwind** is the sole target styling input. The native compiler will produce
  typed declarations, conditions, theme references and effect requirements;
  handwritten CSS, CSS resources and CSS fallback are retired by the migration.
  Full profile support is not a property of the current exact-token catalog.
- **TurboScript** controllers receive typed event snapshots and request bounded
  UI mutations and service commands. The architecture keeps Element pointers,
  renderer access, and GPU resources in the native host.
- **FlexUI** owns the Element tree, computed style, widgets, layout, and application
  candidate publication. A failed reload must leave the active instance intact.
- **Salts::Unicode** supplies strict UTF-8 scanning, versioned Unicode properties,
  and text boundaries. FlexUI owns typography and editing policy above them.
- **Target renderer owners** will consume validated drawing/effect plans. D2D
  primitives and new effect graphs will be qualified separately; gCanvas
  GL/GLES/Vulkan will have separate program profiles. Tango uses terminal cells
  and has no GPU shaders. Admission, budgets and generation contracts remain
  implementation work.

These ownership rules are the design contract. Current scope matters:

| Area | Implemented and focused qualification | Remaining work |
| --- | --- | --- |
| Unicode/text | Strict scalar scanning, emoji property/presentation separation, grapheme-based approximate layout, and XML UTF-8 admission | Real shaping/measurement integration, editable grapheme geometry, complete application boundary coverage ([#15](https://github.com/qigao/stun/issues/15)) |
| XML | Registered content preflight, anonymous nodes, natural text, strict UTF-8 admission, and real headless load/reload regressions | Complete property/event/binding schemas, stable internal identity, precise decoded-text source maps, and ordered mixed content ([#16](https://github.com/qigao/stun/issues/16)) |
| Tailwind | Catalog-based utility-to-CSS compilation into the existing StyleEngine | Native Tailwind-only compiler, typed Theme/Preset, complete behavior migration and CSS removal ([#17](https://github.com/qigao/stun/issues/17)) |
| Backend effects | D2D primitives, Tango cell adapter and gCanvas GPU implementations with different current limits | Effect-domain admission, separate shader artifacts, strict unsupported behavior and resource retirement; the design matrix is not a current support claim ([#17](https://github.com/qigao/stun/issues/17)) |
| Desktop delivery | Native application/controller code and focused component checks | Compatible TurboScript SDK integration, supported installed distributions/platforms, and physical-GPU qualification ([#31](https://github.com/qigao/stun/issues/31)) |

See the [architecture](flexUI/docs/ARCHITECTURE.md),
[native Tailwind and backend decision](flexUI/docs/TAILWIND_NATIVE_BACKENDS_DESIGN.md),
[XML/controller ownership history](flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md),
[desktop design](flexUI/docs/FLEXUI_DESKTOP_DESIGN.md),
[controller design](flexUI/docs/TURBOSCRIPT_CONTROLLER_DESIGN.md), and
[current utility JIT implementation history](flexUI/docs/TAILWIND_JIT_DEFAULT_DESIGN.md).
The application umbrella is [#1](https://github.com/qigao/stun/issues/1).

## Major modules

- `flexUI/`: declarative UI, widgets, styles, text, events, and application/controller runtime.
- `flex/`: expression, animation, scene/runtime, and rendering capabilities. Native
  capabilities remain useful independently of the legacy `.flex` source DSL.
- `shared/gcanvas/`: native canvas/window/GPU substrate.
- `shared/layout/`: independently consumable graph layout algorithms.
- `shared/markdown/`: canonical Markdown parsing shared by its consumers.
- `charts/`: domain chart languages/renderers and reusable XML ChartFrame shells via `UiDocument` Fragment/Slot composition; plot/widgets remain native. See the [Mermaid documentation](charts/mermaid/README.md) and [chart composition design](flexUI/docs/CHART_UI_DOCUMENT_COMPOSITION_DESIGN.md).
- `tango/`: terminal UI support.

The legacy `.flex` source path remains during migration. Its deletion requires
consumer and install-tree validation; it is separate from retaining native Flex
animation, numerical MIR, layout, and physics capabilities.

## Flex/FlexUI core development on Windows

`win-core-release-user` builds the Flex/FlexUI graph with plugin support enabled
and bundled applications/charts disabled. It uses the repository's vcpkg
manifest and requires x64 Release Salts and SaltsUtils SDK installations.
The full application build remains available as `win-release-user`.

Run in an x64 Visual Studio developer shell, setting the SDK roots to your
installed packages (the following paths match this workspace):

```powershell
$env:SALTS_ROOT = 'C:/projects/cpp/external/pkgs/salts/release'
$env:SALTS_UTILS_ROOT = 'C:/projects/cpp/external/pkgs/salts-utils/release'
cmake --preset win-core-release-user
cmake --build --preset win-core-release-user --target test_plugin_create_unwind test_plugin_host test_plugin_manifest test_plugin_abi test_service_registry test_property_cmeta test_style_property_registry test_style_engine test_css_typed_ir test_css_typed_apply test_typed_transition test_typed_animation --parallel 2
ctest --preset win-core-release-user -R '^(test_plugin_create_unwind|test_plugin_host|test_plugin_manifest|test_plugin_abi|test_service_registry|test_property_cmeta|test_style_property_registry|test_style_engine|test_css_typed_ir|test_css_typed_apply|test_typed_transition|test_typed_animation)$'
```

This profile's install entry is `install-win-core-release-user`; its destination
is `build/Msvc-Core-Release/install`, separate from both SDK roots.


