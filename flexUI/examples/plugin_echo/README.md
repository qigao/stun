# FlexUI installed PluginHost example

This standalone project consumes only an installed FlexUI package. It builds a
pure C echo plugin module and a C++ host executable, then verifies the complete
load, start, service request, completion, stop, join and unload path.

The [Unicode runtime workflow](../../../.github/workflows/unicode-runtime.yml)
configures and tests the actual production FlexUI targets on Linux with ASan and
UBSan, then directly installs and consumes the package. Its producer profile
enables `FLEXUI_ENABLE_PLUGINS` and disables the window/GPU backends. After
configuring the producer, run these commands from the repository root. Set
`FLEXUI_INSTALL_ROOT` to the destination and export `SALTS_ROOT` for the installed
Salts/SaltsUtils profile used by the producer; use the same OpenSSL package profile.

```bash
cmake --build build/flexui-production --target flexUI flexUI_services flexUI_plugin_host
cmake --install build/flexui-production --config Debug \
  --prefix "$FLEXUI_INSTALL_ROOT" --component FlexUIPlugin
cmake -S flexUI/examples/plugin_echo -B build/flexui-installed-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_PREFIX_PATH=$FLEXUI_INSTALL_ROOT;$SALTS_ROOT" \
  -DFLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER=ON \
  -DFLEXUI_EXAMPLE_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build build/flexui-installed-host
ctest --test-dir build/flexui-installed-host --no-tests=error --output-on-failure
```

`PluginSDK`, `Services`, and `PluginHost` have separate export files within the
same `FlexUIPlugin` install component. Requesting only `PluginSDK` or `Services`
loads no Salts packages; requesting `PluginHost` also loads both components and
its Salts dependencies. A request without components loads all components built
into the package. Unknown required components and an unavailable PluginHost fail
with explicit diagnostics.

The workflow separately configures the C header consumer and the C++ registry
consumer with Salts discovery disabled. The latter calls the installed Services
implementation; the C plugin/host lifecycle is exercised by the example above.
The subshell below preserves the caller's package environment:

```bash
(
  unset SALTS_ROOT SALTS_UTILS_ROOT LD_LIBRARY_PATH LD_PRELOAD
  for component in PluginSDK Services; do
    consumer_build="build/flexui-installed-$component"
    cmake -S flexUI/tests/plugin_install_consumer -B "$consumer_build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug "-DFLEXUI_CONSUMER_COMPONENT=$component" \
      "-DCMAKE_PREFIX_PATH=$FLEXUI_INSTALL_ROOT" \
      -DCMAKE_DISABLE_FIND_PACKAGE_Salts=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_SaltsUtils=TRUE \
      '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
      '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
    cmake --build "$consumer_build"
    ctest --test-dir "$consumer_build" --no-tests=error --output-on-failure
  done
)
```

GCC/Clang sanitizer options apply to both the C plugin and C++ host; instrumented
producer archives require compatible sanitizer flags in their consumers. MSVC
continues to support the address sanitizer option with the undefined sanitizer
option off. On Windows, put the selected shared dependency DLL directories in
`PATH` or beside the executable. The Linux workflow clears `LD_LIBRARY_PATH` and
`LD_PRELOAD` and checks the actual loaded Salts provider. Setting
`FLEXUI_EXAMPLE_BUILD_HOST=OFF` builds only the C module and requires only PluginSDK.
