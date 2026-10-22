# Headless XML runtime qualification

This profile builds the normal production `flexUI`, `flexUI_controller`, Flex
Runtime/Compiler/Expression/Render/Animation, Markdown and gCanvas Core targets.
It executes the original XML admission, XML compiler, UI document, widget registry,
content model and desktop application suites with ASan/UBSan. Whole-tree schema
rejection, factory ordering, failed application publication and failed reload
preservation run against the actual implementation.

The only extra include roots compensate for existing repository-root-relative
paths in the production CMake modules. No source substitutes, missing-symbol
stubs, section garbage collection or sanitizer exclusions are used.

Install the published Salts and SaltsUtils SDKs used by runtime-conformance CI,
plus re2c, nlohmann-json, FreeType, stb and pugixml. Build the three small upstream
source dependencies into a separate prefix:

```sh
cmake -S flexUI/tests/xml-runtime-deps -B build/xml-deps \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/xml-deps-install"
cmake --build build/xml-deps --parallel 2
cmake --install build/xml-deps
cmake -S flexUI/tests/xml-runtime-standalone -B build/xml-runtime \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$SALTS_ROOT;$PWD/build/xml-deps-install" \
  -DSTB_INCLUDE_DIRS=/usr/include/stb
cmake --build build/xml-runtime --target stun_xml_runtime_qualification --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/xml-runtime --no-tests=error --verbose --output-on-failure
```

The dependency profile checks the exact commits behind Lexbor 3.0.0, Box2D 3.1.1
and tree-sitter 0.25.10. Its tree-sitter package exports the real upstream library
under the package spelling consumed by FlexUI. CI caches only these installed
third-party dependencies; first-party production targets are rebuilt with
sanitizers for each applicable head.

This profile excludes window/GPU/Mesa, native plugin hosts and TurboScript JIT.
Existing ScriptModuleFactory fixtures exercise the real controller/application
boundary, but do not qualify a published TurboScript runtime or installed editor
(#31). Content-model checks cover registered tags, children and declared text;
complete property/event/binding schemas, stable internal reconciliation identity
and natural XML text remain separate #16 work. Anonymous nodes stay outside
the named index; event and binding targets require an explicit id. The no-registry instantiation overload remains an
explicit structural compatibility path.
