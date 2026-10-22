# Project Overview

This repository contains a C/C++ UI + charting stack (Flex + chart engines).

## Charts

Chart engines are under `charts/`. Mermaid chart engines live here:

- `charts/mermaid/README.md`

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

