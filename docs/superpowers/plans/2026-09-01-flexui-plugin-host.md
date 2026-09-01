# FlexUI PluginHost Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 FlexUI 增加默认关闭、可显式启用的版本化纯 C DLL 插件 ABI，以及能安全完成 load/create/start/submit/stop/join/destroy/unload 的 PluginHost。

**Architecture:** `FlexUI::PluginSDK` 只公开稳定 C ABI；`FlexUI::PluginHost` 是 C++ Facade，动态库细节隔离在私有 native adapter。插件 service 经现有不可变 `ApplicationServiceRegistry` 暴露，请求使用固定容量活动槽，completion 可由多个插件线程回调，但所有外部回调均在锁外发生。

**Tech Stack:** C11 ABI、C++17、CMake、TurboUtils UTF-8 校验、TinyTest、Windows `LoadLibraryExW` / Unix `dlopen`

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`

## Global Constraints

- 首版只加载调用方显式指定的可信、进程内插件；不提供 manifest discovery、热重载、权限沙箱或崩溃隔离。
- ABI major/minor 为 `1.0`，入口符号固定为 `flexui_plugin_get_api_v1`；major 必须完全相等，插件声明的 required host minor 不得高于 host minor。
- ABI 仅传 C POD、定宽整数、opaque handle、函数指针和 borrowed byte view；禁止跨 CRT 分配或释放内存。
- 插件必须在成功 `submit()` 前复制其需要保留的 request；completion view 只在 `post_completion()` 调用期间有效。
- 每个公开 ABI struct 必须携带 `struct_size`，可扩展表保留 reserved slots；禁止改变默认平台 packing。
- 活动 request slot 在 host 构建时一次性预分配；满时返回 `QueueFull`，不动态扩容、不阻塞、不静默丢弃。
- `stop()` 先拒绝新请求，再通知插件；只有 `join()` 成功且所有 ABI 调用退出后才能 destroy 和 unload。
- `FLEXUI_ENABLE_PLUGINS` 默认 `OFF`；关闭时不得编译或链接平台动态库加载代码。
- 当前用户已选择 inline execution；本计划在当前隔离 worktree 内逐项执行，不派发子代理。

---

## File Structure

- `flexUI/include/flexUI/plugin_abi.h`：唯一公开纯 C ABI、状态码、descriptor、host/plugin 函数表与所有权契约。
- `flexUI/include/flexUI/plugin_host.h`：C++ builder、host、limits、error、state 和 stats Facade。
- `flexUI/modules/plugin_host/native_plugin_library.hpp/.cpp`：唯一直接调用平台动态库 API 的 RAII adapter。
- `flexUI/modules/plugin_host/plugin_module.hpp/.cpp`：单个插件的 ABI 校验、生命周期、endpoint 与固定 request slot 表。
- `flexUI/modules/plugin_host/plugin_host.cpp`：多插件事务构建、registry 发布和逆序关闭。
- `flexUI/modules/plugin_host/CMakeLists.txt`：`FlexUI::PluginSDK` 和可选 `FlexUI::PluginHost` targets。
- `flexUI/tests/plugin_test_*.c/.cpp`：真实动态库正常与错误 fixture。
- `flexUI/tests/test_plugin_abi.c`：C11 可包含性与 ABI 常量检查。
- `flexUI/tests/test_plugin_host.cpp`：加载、路由、背压、同步 completion、超时重试与逆序清理。

### Task 1: Freeze the pure C ABI and PluginSDK target

**Files:**
- Create: `flexUI/include/flexUI/plugin_abi.h`
- Create: `flexUI/tests/test_plugin_abi.c`
- Modify: `flexUI/modules/plugin_host/CMakeLists.txt`
- Modify: `flexUI/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `flexui_plugin_get_api_v1_fn`, `flexui_host_api_v1`, `flexui_plugin_api_v1`, `flexui_plugin_descriptor_v1`, `flexui_plugin_request_v1`, `flexui_plugin_completion_v1`.
- Status values: `FLEXUI_PLUGIN_STATUS_OK`, `INVALID_ARGUMENT`, `INVALID_STATE`, `UNSUPPORTED_ABI`, `RESOURCE_LIMIT`, `BUSY`, `CLOSED`, `REJECTED`, `TIMED_OUT`, `INTERNAL`, `QUEUE_FULL`, `STALE`.

- [ ] **Step 1: Add a C11 compile test before the header exists**

```c
#include <flexUI/plugin_abi.h>

_Static_assert(FLEXUI_PLUGIN_ABI_MAJOR == 1u, "ABI major");
_Static_assert(FLEXUI_PLUGIN_ABI_MINOR == 0u, "ABI minor");
_Static_assert(sizeof(flexui_plugin_bytes_view) == 16u, "64-bit ABI layout");

int main(void) {
    flexui_plugin_api_v1 api = {0};
    api.struct_size = sizeof(api);
    return api.struct_size == sizeof(api) ? 0 : 1;
}
```

- [ ] **Step 2: Configure/build the compile test and observe the missing-header failure**

Run: `cmake --build --preset win-dev-user --target test_plugin_abi`

Expected: compilation fails because `flexUI/plugin_abi.h` does not exist.

- [ ] **Step 3: Implement the ABI header and always-available SDK target**

The header defines `FLEXUI_PLUGIN_CALL`, `FLEXUI_PLUGIN_EXPORT`, `FLEXUI_PLUGIN_ENTRY_SYMBOL_V1`, fixed-width status types, byte/error buffers, service/operation descriptors, request/completion records, host callback table, opaque instance, plugin lifecycle table, and the entry function typedef. Each view is documented as borrowed for the call only; `destroy` is valid only after successful join or create unwind.

```cmake
add_library(FlexUIPluginSDK INTERFACE)
add_library(FlexUI::PluginSDK ALIAS FlexUIPluginSDK)
target_include_directories(FlexUIPluginSDK INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/flexUI/include>
    $<INSTALL_INTERFACE:include>)
```

- [ ] **Step 4: Build and run the C test**

Run: `cmake --build --preset win-dev-user --target test_plugin_abi`

Expected: target builds and exits with status 0.

### Task 2: Implement the fail-fast native dynamic-library adapter

**Files:**
- Create: `flexUI/modules/plugin_host/native_plugin_library.hpp`
- Create: `flexUI/modules/plugin_host/native_plugin_library.cpp`
- Create: `flexUI/tests/plugin_test_missing_entry.c`
- Create: `flexUI/tests/plugin_test_bad_abi.c`
- Test: `flexUI/tests/test_plugin_host.cpp`

**Interfaces:**
- Produces: `NativePluginLibrary::open(const std::filesystem::path&)`, `symbol(const char*)`, `close()`.
- Consumes: absolute canonical plugin path supplied explicitly by the builder.

- [ ] **Step 1: Write loader rejection tests**

Tests require rejection of a relative path, a missing file, a library without `flexui_plugin_get_api_v1`, and a table whose ABI major is not 1. Each assertion checks the typed `PluginHostErrorCode`, path, and stage.

- [ ] **Step 2: Build the tests and observe missing PluginHost API failures**

Run: `cmake --build --preset win-dev-user --target test_plugin_host`

Expected: compilation fails because `flexUI/plugin_host.h` and the adapter implementation are absent.

- [ ] **Step 3: Implement the private adapter**

On Windows, canonicalize and require an absolute regular-file path, then call:

```cpp
LoadLibraryExW(path.c_str(), nullptr,
               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
               LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
```

On Unix, call `dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL)`. Resolve only the fixed entry symbol. Move operations transfer ownership; destruction closes a still-open handle. Errors retain stage, path and the platform diagnostic without logging at lower layers.

- [ ] **Step 4: Run the negative loader tests**

Run: `ctest --preset win-dev-user -R ^test_plugin_host$ --output-on-failure`

Expected: every invalid library is rejected before create/start.

### Task 3: Implement PluginModule lifecycle and bounded request slots

**Files:**
- Create: `flexUI/include/flexUI/plugin_host.h`
- Create: `flexUI/modules/plugin_host/plugin_module.hpp`
- Create: `flexUI/modules/plugin_host/plugin_module.cpp`
- Create: `flexUI/tests/plugin_test_echo.cpp`
- Modify: `flexUI/tests/test_plugin_host.cpp`

**Interfaces:**
- Produces: `PluginHostLimits`, `PluginHostError`, `PluginHostState`, `PluginHostStats`, and a private `PluginModule` implementing `IApplicationServiceEndpoint`.
- Consumes: `ApplicationServiceRequest`, `IApplicationServiceCompletionSink`, plugin ABI tables.

- [ ] **Step 1: Add lifecycle and data-path tests**

The real test DLL exposes `test.echo/1` with `echo` and `block`. Cover synchronous completion during `submit`, payload/error copying, unknown token, duplicate completion, exact slot capacity, capacity+1 rejection, plugin submit failure retaining no slot, queue-full completion retry, stop rejecting new work, 1 ms join timeout, later successful join, and no callback after unload.

- [ ] **Step 2: Run tests and observe missing implementation failures**

Run: `cmake --build --preset win-dev-user --target test_plugin_host`

Expected: link or compile failures identify the unimplemented module API.

- [ ] **Step 3: Implement descriptor validation and copying**

Validate non-null function pointers, table/struct size prefixes, ABI version, UTF-8 identifiers, identifier grammar, unique capabilities/operations, configured counts and payload limits. Copy all descriptors into host-owned C++ storage before registry construction; no plugin pointer escapes an ABI call except the stable function table and instance handle, both kept only until unload.

- [ ] **Step 4: Implement the fixed-capacity slot state machine**

Each preallocated slot is `Free`, `Active`, or `Posting` and stores token, request id and a non-owning completion sink. Under one mutex, reserve/release slots and update current/peak counters. Never allocate, invoke plugin code, or call a sink while holding the mutex.

```text
submit: Free -> Active -> plugin submit
sync/async completion: Active -> Posting -> sink call
sink QueueFull: Posting -> Active (plugin may retry same token)
sink accepted/terminal: Posting -> Free
plugin submit failure: Active -> Free
stop: accepting=false; join success -> remaining Active slots abandoned -> Free
```

- [ ] **Step 5: Implement stop/join and retryable timeout**

`stop(timeout)` changes `Started -> Stopping`, invokes plugin `stop` once, and calls join with the remaining deadline. `TimedOut` leaves the module in `Stopping` with library and slots alive so a later call can retry. Only successful join permits slot cancellation, destroy and unload. Destructor uses an infinite join safety net and never unloads live code.

- [ ] **Step 6: Run lifecycle tests repeatedly**

Run: `ctest --preset win-dev-user -R ^test_plugin_host$ --repeat until-fail:100 --output-on-failure`

Expected: 100 consecutive passes, including synchronous completion and timeout retry.

### Task 4: Build the transactional PluginHost Facade and registry bridge

**Files:**
- Create: `flexUI/modules/plugin_host/plugin_host.cpp`
- Modify: `flexUI/include/flexUI/plugin_host.h`
- Modify: `flexUI/tests/test_plugin_host.cpp`

**Interfaces:**
- Produces: `PluginHostBuilder::load_plugin(path)`, `register_builtin(descriptor, endpoint)`, `build()`, `PluginHost::stop(timeout)`, `registry()`, `state()`, `stats()`.
- Build result owns `std::unique_ptr<PluginHost>`, shares `std::shared_ptr<const ApplicationServiceRegistry>`, and carries a typed error on failure.

- [ ] **Step 1: Add transaction and registry tests**

Cover mixed built-in/plugin registration, duplicate capability rejection, successful resolution through the immutable registry, builder owner-thread enforcement, failed second build, partial create/start rollback in reverse order, maximum plugin/service limits, and registry snapshot lifetime.

- [ ] **Step 2: Run tests and observe missing Facade behavior**

Run: `ctest --preset win-dev-user -R ^test_plugin_host$ --output-on-failure`

Expected: new assertions fail until builder transaction and rollback are implemented.

- [ ] **Step 3: Implement candidate build and atomic publication**

The builder loads, validates and creates candidate modules without publishing them. It registers copied service descriptors plus built-ins into `ApplicationServiceRegistryBuilder`, builds the immutable snapshot, then starts plugins in load order. Any failure stops/joins/destroys already-started candidates in reverse order and returns no registry or host.

- [ ] **Step 4: Implement host-wide reverse shutdown and stats aggregation**

`PluginHost::stop` first disables every endpoint, then stops and joins modules in reverse order using one absolute deadline. A timeout returns the responsible plugin id/path and preserves retryable state. Stats aggregate fixed slot counts and monotonic submitted/completed/rejected/abandoned counters.

- [ ] **Step 5: Run PluginHost and service-registry tests**

Run: `ctest --preset win-dev-user -R "^test_(plugin_host|service_registry|application_service_requests)$" --output-on-failure`

Expected: all tests pass.

### Task 5: Gate the module in CMake and synchronize design documentation

**Files:**
- Modify: `CMakeOptions.cmake`
- Modify: `flexUI/CMakeLists.txt`
- Modify: `flexUI/modules/plugin_host/CMakeLists.txt`
- Modify: `flexUI/tests/CMakeLists.txt`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Produces: default-OFF `FLEXUI_ENABLE_PLUGINS`, always present `FlexUI::PluginSDK`, conditional `FlexUI::PluginHost`.

- [ ] **Step 1: Add the feature option and target gates**

```cmake
option(FLEXUI_ENABLE_PLUGINS "Build the optional FlexUI trusted DLL PluginHost?" OFF)
```

`FlexUI::PluginSDK` remains usable when OFF. `FlexUI::PluginHost`, `${CMAKE_DL_LIBS}` and plugin runtime tests exist only when ON.

- [ ] **Step 2: Verify the OFF configuration contains no loader target**

Run: `cmake --fresh --preset win-dev-user -DFLEXUI_ENABLE_PLUGINS=OFF`

Run: `cmake --build --preset win-dev-user --target test_plugin_abi`

Expected: configure/build succeeds, and `FlexUIPluginHost` is absent from the generated target list.

- [ ] **Step 3: Update the design and milestone checklist**

Document the frozen ABI compatibility rule, borrowed/copy boundaries, trusted-plugin scope, fixed request capacity, completion retry semantics, owner-thread control plane, retryable join timeout, destructor safety net and required shutdown order (`PluginHost` before application/mailbox). Mark only implemented P4 checklist items complete; keep manifest, dependency resolution, signature verification, permission enforcement and process isolation explicitly future work.

- [ ] **Step 4: Verify ON configuration and focused tests**

Run: `cmake --fresh --preset win-dev-user -DFLEXUI_ENABLE_PLUGINS=ON`

Run: `cmake --build --preset win-dev-user --target test_plugin_host`

Run: `ctest --preset win-dev-user -R ^test_plugin_host$ --output-on-failure`

Expected: plugin fixtures build as real dynamic libraries and all host tests pass.

### Task 6: Verification and compatibility audit

**Files:**
- Verify only; fix only files implicated by evidence.

**Interfaces:**
- Consumes: all P4a/P4b targets and tests.

- [ ] **Step 1: Run minimal and adjacent Debug tests**

Run: `ctest --preset win-dev-user -R "^test_(plugin_host|service_registry|application_service_requests|desktop_application)$" --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 2: Run Release tests**

Run: `cmake --fresh --preset win-release-user -DFLEXUI_ENABLE_PLUGINS=ON`

Run: `cmake --build --preset win-release-user --target test_plugin_host test_plugin_abi test_service_registry test_application_service_requests test_desktop_application`

Run: `ctest --preset win-release-user -R "^test_(plugin_abi|plugin_host|service_registry|application_service_requests|desktop_application)$" --output-on-failure`

Expected: all selected tests pass.

- [ ] **Step 3: Run sanitizer coverage available on this toolchain**

Run the repository's FlexUI ASan preset with `FLEXUI_ENABLE_PLUGINS=ON`, then the same focused test regex. If Windows dynamic-library instrumentation is unavailable, record the exact configure/build output and retain Debug repeated tests as evidence without claiming race or memory-safety proof.

- [ ] **Step 4: Audit diffs and forbidden placeholders**

Run: `git diff --check`

Run: `rg.exe -n "TO[D]O|FIXM[E]|HAC[K]|Not implemente[d]|assert\(false\)" flexUI/include/flexUI/plugin_abi.h flexUI/include/flexUI/plugin_host.h flexUI/modules/plugin_host flexUI/tests/test_plugin_host.cpp`

Expected: `git diff --check` succeeds and the placeholder scan prints no matches.

- [ ] **Step 5: Inspect the final impact surface**

Run: `codegraph affected -p . flexUI/include/flexUI/plugin_abi.h flexUI/include/flexUI/plugin_host.h flexUI/modules/plugin_host/plugin_host.cpp`

Expected: affected callers are limited to the optional module, its tests, service registry and documented future DesktopApplication integration boundary.
