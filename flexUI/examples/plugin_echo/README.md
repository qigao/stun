# FlexUI installed PluginHost example

This standalone project consumes only an installed FlexUI package. It builds a
pure C echo plugin DLL and a C++ host executable, then verifies the complete
load, start, service request, completion, stop, join and unload path.

Install the producer's `FlexUIPlugin` component, then configure this directory
with exact package locations:

```powershell
cmake --install <producer-build> --config Release `
  --prefix <flexui-prefix> --component FlexUIPlugin
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DFlexUI_DIR=<flexui-prefix>/lib/cmake/FlexUI `
  -DTurboUtils_DIR=<turboutils-prefix>/lib/cmake/TurboUtils `
  -DTurboParser_DIR=<turboparser-prefix>/lib/cmake/TurboParser
cmake --build build
ctest --test-dir build --output-on-failure
```

At runtime, the TurboUtils and TurboParser `bin` directories must be present in
`PATH` when those packages were built as shared libraries. The repository test
`test_plugin_install_consumer` supplies that runtime environment explicitly.
It also configures the same source with `FLEXUI_EXAMPLE_BUILD_HOST=OFF` and no
TurboUtils/TurboParser package locations, proving that `PluginSDK` remains a
standalone pure C component.
