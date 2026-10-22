# Stun

Stun is a native C/C++ UI and charting stack built around **FlexUI**, **Flex**,
and **gCanvas**. Its desktop application design uses XML, CSS/Tailwind utilities,
and TurboScript without requiring a browser runtime in the installed application.

## Application authoring and ownership

- **XML** describes UI structure and static properties; FlexUI compiles it into an
  immutable `CompiledUiProgram` before instantiation.
- **CSS** owns the style cascade and animation declarations. The current utility
  catalog generates CSS into that same cascade. Full Tailwind v4 compatibility
  is a separate compiler goal, not a property of the current whitelist.
- **TurboScript** controllers receive typed event snapshots and request bounded
  UI mutations and service commands. The architecture keeps Element pointers,
  renderer access, and GPU resources in the native host.
- **FlexUI** owns the Element tree, computed style, widgets, layout, and application
  candidate publication. A failed reload must leave the active instance intact.
- **Salts::Unicode** supplies strict UTF-8 scanning, versioned Unicode properties,
  and text boundaries. FlexUI owns typography and editing policy above them.
- **gCanvas** owns canvas/window/GPU drawing resources and consumes rendering
  commands; application semantics remain in FlexUI.

These ownership rules are the design contract. Current scope matters:

| Area | Implemented and focused qualification | Remaining work |
| --- | --- | --- |
| Unicode/text | Strict scalar scanning, emoji property/presentation separation, grapheme-based approximate layout, and XML UTF-8 admission | Real shaping/measurement integration, editable grapheme geometry, complete application boundary coverage ([#15](https://github.com/qigao/stun/issues/15)) |
| XML | Registry-backed factories, explicit IDs, attribute-based text, and strict source admission | Typed content descriptors, anonymous nodes, and natural text ([#16](https://github.com/qigao/stun/issues/16)) |
| CSS/Tailwind | Catalog-based utility compilation into the existing StyleEngine cascade | Fixed Tailwind v4 profile, broader syntax, and differential conformance ([#17](https://github.com/qigao/stun/issues/17)) |
| Desktop delivery | Native application/controller code and focused component checks | Compatible TurboScript SDK integration, supported installed distributions/platforms, and physical-GPU qualification ([#31](https://github.com/qigao/stun/issues/31)) |

See the [architecture](flexUI/docs/ARCHITECTURE.md),
[XML/CSS/TurboScript decision](flexUI/docs/XML_CSS_TBS_ARCHITECTURE.md),
[desktop design](flexUI/docs/FLEXUI_DESKTOP_DESIGN.md),
[controller design](flexUI/docs/TURBOSCRIPT_CONTROLLER_DESIGN.md), and
[current utility JIT design](flexUI/docs/TAILWIND_JIT_DEFAULT_DESIGN.md).
The application umbrella is [#1](https://github.com/qigao/stun/issues/1).

## Major modules

- `flexUI/`: declarative UI, widgets, CSS, text, events, and application/controller runtime.
- `flex/`: expression, animation, scene/runtime, and rendering capabilities. Native
  capabilities remain useful independently of the legacy `.flex` source DSL.
- `shared/gcanvas/`: native canvas/window/GPU substrate.
- `shared/layout/`: independently consumable graph layout algorithms.
- `shared/markdown/`: canonical Markdown parsing shared by its consumers.
- `charts/`: chart engines and UI adapters; see the [Mermaid documentation](charts/mermaid/README.md).
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

