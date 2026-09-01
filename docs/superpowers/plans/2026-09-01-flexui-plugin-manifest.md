# FlexUI Plugin Manifest P4c Implementation Plan

> Status: implementation plan for the trusted in-process PluginHost manifest phase.

**Goal:** Add a bounded `plugin.toml` v1 loader, dependency ordering, descriptor matching and
default-deny permission admission without changing the frozen DLL ABI 1.0 or the behavior of the
existing explicit DLL loading path.

**Architecture:** `PluginManifestParser` owns file/TOML adaptation and produces an immutable
host-owned model. `PluginDependencyResolver` validates unique IDs, exact semantic versions,
required/optional dependencies and cycles, then returns deterministic dependency-first indices.
`PluginHostBuilder` stages manifests and performs one build transaction: resolve -> load/create ->
validate manifest against descriptor -> build registry -> validate required capabilities -> start
in dependency order -> publish. Failure publishes neither host nor registry and existing RAII
unloads every unstarted module; already-started modules stop in reverse order.

**Libraries:** C++17, `TurboUtils::Core`, the installed `TurboParser::Parser` TOML facade, existing
`FlexUI::Services`, existing native library adapter, TinyTest. The configured root TurboUtils SDK
does not export `TurboUtils::TomlParser`, so this phase uses the already-required facade rather than
adding a package-root fallback.

## Public contract

### Manifest v1

```toml
manifest_version = 1
name = "document.storage"
version = "1.2.0"
library = "flexui_document_storage.dll"
services = ["document.storage/1"]
capabilities = ["settings.read/1"]
permissions = ["file"]

[abi]
major = 1
minor = 0

[[dependencies]]
name = "settings.core"
version = "1.0.0"
optional = false
```

- `manifest_version` must equal `1`.
- `name` uses the same canonical lowercase identifier syntax as the DLL `plugin_id`.
- `version` and dependency `version` are canonical SemVer 2.0.0 strings. Manifest v1 dependency
  matching is exact, including prerelease and build metadata; range operators are rejected.
- `library` is a relative path. After canonicalization it must remain inside the canonical manifest
  directory and identify a regular file. Absolute paths and traversal outside the directory fail.
- `abi.major` must equal host ABI major. `abi.minor` is the minimum host minor required by the
  package and must not exceed the host minor.
- `services` lists every capability exported by the DLL descriptor exactly once. The set must match
  the loaded descriptor exactly.
- `capabilities` lists required services that must exist in the final immutable registry. This is a
  load-time contract only; ABI 1.0 still does not expose cross-plugin service lookup.
- `permissions` accepts only `file`, `network`, `process`, and `device`. Every requested permission
  must be granted by `PluginHostPolicy`; the default policy grants none.
- Every dependency requires `name`, exact `version`, and explicit `optional`. A missing required
  dependency or version mismatch fails. A missing optional dependency is allowed; if present, its
  version must still match and its graph edge participates in ordering.
- Unknown keys fail so spelling mistakes cannot silently weaken policy or dependency checks.

### Compatibility and migration

- `flexui_plugin_api_v1` and entry symbol remain unchanged.
- Existing `PluginHostBuilder::load_plugin(absolute_path)` remains source-compatible and continues
  to load/create immediately in caller order. It is explicitly a trusted legacy/embedding path and
  has no manifest permissions or dependencies.
- The additive `PluginHostBuilder(PluginHostLimits, PluginHostPolicy)` overload and
  `load_manifest(absolute_manifest_path)` are additive. Manifest candidates are not loaded until
  `build()` has validated the complete graph and policy.
- Applications migrate one DLL at a time by placing `plugin.toml` beside the DLL and switching the
  builder call. A failure can be rolled back by restoring `load_plugin`; no stored data changes.
- Permission admission is not an OS sandbox. Trusted in-process native code can call platform APIs
  directly. Untrusted plugins still require the future process/IPC host.

## Ownership and failure semantics

- The builder owns parsed manifests, loaded modules and built-in registrations until successful
  `build()` transfers modules and registry to `PluginHost`.
- Parsed TOML strings are copied before `turbo_free_toml`; no parser-owned pointer escapes.
- The manifest is bounded before parsing. All lists are bounded by host/plugin registry limits.
- The final registry remains the sole capability fact source. Manifest capabilities are validated
  against that snapshot; no parallel mutable capability registry is introduced.
- Duplicate manifest IDs, duplicate IDs across legacy and manifest candidates, invalid manifest,
  denied permission, missing capability/dependency, version mismatch and cycle all fail fast.
- Optional dependency absence is the only allowed fallback and is explicit in the manifest. It does
  not select another implementation or change service routing.

## Task 1: Freeze public types and errors

**Files:**
- Modify: `flexUI/include/flexUI/plugin_host.h`
- Test: `flexUI/tests/test_plugin_manifest.cpp`

- Add bounded manifest limits to `PluginHostLimits`.
- Add `PluginPermission`, bit operations and `PluginHostPolicy`.
- Add manifest/dependency/policy error codes with stable stage and path context.
- Add documented `load_manifest()` while preserving `load_plugin()`.

## Task 2: Parse and validate manifest v1

**Files:**
- Create: `flexUI/modules/plugin_host/plugin_manifest.hpp`
- Create: `flexUI/modules/plugin_host/plugin_manifest.cpp`
- Modify: `flexUI/modules/plugin_host/CMakeLists.txt`
- Test: `flexUI/tests/test_plugin_manifest.cpp`
- Test fixtures are generated in a temporary package directory so each configured build can use
  the platform- and configuration-specific plugin library filename without checking binaries or
  build-tree paths into the repository.

- Read through `turbo_fs_open/read` into a fixed `max_manifest_bytes + 1` buffer so concurrent file
  growth cannot trigger an unbounded allocation; parse with the explicit-length `turbo_parse_toml`
  facade from `TurboParser::Parser`.
- Copy all values into an internal model and free each TOML string with its owning parser API.
- Validate required/unknown keys, types, UTF-8/string/list/count bounds, canonical identifiers,
  canonical SemVer and duplicate list entries.
- Canonicalize manifest/library paths and reject escape from the manifest directory.

## Task 3: Resolve dependency graph

**Files:**
- Create: `flexUI/modules/plugin_host/plugin_dependency_resolver.hpp`
- Create: `flexUI/modules/plugin_host/plugin_dependency_resolver.cpp`
- Test: `flexUI/tests/test_plugin_manifest.cpp`

- Validate duplicate IDs and exact dependency versions before loading any DLL.
- Use deterministic Kahn topological sorting with manifest staging order as the tie breaker.
- Include present optional dependencies as edges; permit only explicitly optional missing nodes.
- Report the dependent plugin and dependency ID for missing, mismatched and cyclic graphs.

## Task 4: Integrate the build transaction

**Files:**
- Modify: `flexUI/modules/plugin_host/plugin_host.cpp`
- Modify: `flexUI/modules/plugin_host/plugin_module.hpp`
- Modify: `flexUI/modules/plugin_host/plugin_module.cpp`
- Test: `flexUI/tests/test_plugin_host.cpp`
- Test DLLs: `flexUI/tests/plugin_test_echo.cpp`

- Stage manifest models in the builder and resolve before library loading.
- Load/create manifest DLLs in dependency order and check name/version/ABI/services against the
  copied descriptor.
- Reject IDs duplicated between legacy and manifest candidates.
- Build the immutable registry, then validate every manifest capability against it.
- Start in dependency order; reuse existing reverse rollback and shutdown ordering.

## Task 5: Document and verify

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`
- Modify: `flexUI/tests/CMakeLists.txt`

- Document schema, trusted boundary, permission admission limitation and migration.
- Run focused manifest/host/ABI/service tests in Debug, ASan when available, and Release.
- Re-run PluginHost lifecycle tests repeatedly.
- Configure with `FLEXUI_ENABLE_PLUGINS=OFF` and verify the host/TOML parser targets and manifest
  tests are absent while `FlexUI::PluginSDK` remains available.
- Run `git diff --check`, placeholder scan and CodeGraph affected-test analysis.

## Explicitly deferred

- Version ranges, multiple installed versions of one plugin ID and dependency solving.
- OS sandbox/process isolation, signature verification and trust-store policy.
- Hot reload/state migration.
- ABI host service lookup and typed service payload/schema.
- Directory discovery or implicit recursive scanning.
