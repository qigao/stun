# Mesa module-lifetime regression (#31)

This fixture fixes and tests the Linux Mesa cache leaks isolated in Stun issue #31. It is not part of the Stun ABI or gCanvas implementation.

## Source and patch scope

The workflow downloads the official Mesa 25.2.8 archive and checks SHA-256 `097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93`, published at https://docs.mesa3d.org/relnotes/25.2.8.html. It also checks the two unchanged Git blob IDs before building. Original Mesa copyright/license notices are preserved in the downloaded source. `module-cleanup.patch` is a downstream patch, not an upstream-accepted change.

The two cache owners are `src/util/u_cpu_detect.c` (one affinity-mask allocation shared with the published caps snapshot) and `src/gallium/auxiliary/rtasm/rtasm_execmem.c` (the executable mapping and u_mm metadata). Linux ELF finalizers at priority 101 run after ordinary finalizers; this keeps the caches alive for other contexts and for Mesa's queue exit handlers. The rtasm finalizer uses the existing allocator mutex and u_mmDestroy. There is no application-callable cleanup, per-context global teardown, provider fallback, retained-library workaround, SIMD override, or sanitizer suppression.

This patch is scoped to Linux ELF/GCC-compatible compilers. It does not establish Windows/macOS cleanup behavior. Remove it after an upstream version fixes both lifetimes and passes the same controls; verify before changing the driver pin.

## Validation

`mesa-driver.yml` builds the real upstream driver and uses the same ASan/UBSan-instrumented GLFW/GLAD-only executable before and after patching/rebuilding those two source files. Mesa itself uses its normal debugoptimized configuration, as the original distribution driver was not sanitizer-instrumented; the control's allocator interception and exit leak checks remain enabled throughout. No claim is made of sanitizer coverage of every Mesa instruction.

Each variant executes ten independent processes: init, window, draw, overlapping shared contexts, and two rendering threads, with one and four full GLFW lifecycles each. Pixel counts, completed teardown, actual loaded driver paths, and complete module unload are mandatory. The baseline must reproduce drawing leaks; all patched processes must exit zero without leak/UB/memory errors. The overlapping-context case draws after each sibling is destroyed. Rendering workers join before their windows are destroyed. No check can pass by keeping Mesa mapped.

The workflow retains raw exits, maps, pixel counts, source/patch/compiler provenance, build options, and binary hashes. It packages the installed patched driver only after the comparison succeeds. The full Stun desktop gate remains separate and is not made green by these driver-only tests.

Local entry point after installing the pinned dependencies:

```sh
cmake -S cmake/ci/mesa -B build/mesa-control -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_INSTALLED_DIR="$CONTROL_PORTS"
cmake --build build/mesa-control
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
export LIBGL_DRIVERS_PATH="$MESA_ROOT/lib/dri"
export MESA_EXPECTED_DRIVER="$(readlink -f "$LIBGL_DRIVERS_PATH/swrast_dri.so")"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xvfb-run -a build/mesa-control/mesa_driver_control overlap 4
```

The explicit driver path follows Mesa's local-install model: https://docs.mesa3d.org/install.html. Tests fail on a missing or unintended driver instead of selecting another provider.
