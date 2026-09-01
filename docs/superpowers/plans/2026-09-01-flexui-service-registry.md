# FlexUI Service Registry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one immutable, application-scoped ServiceRegistry that validates authorized service commands before UI commit and resolves published requests to lifetime-safe C++ endpoints for the host.

**Architecture:** A mutable builder validates service descriptors and produces an immutable registry snapshot containing host-owned metadata plus `shared_ptr` endpoint references. `DesktopApplication` stores the registry and a validated capability manifest; its request queue uses the same snapshot for fail-fast command validation, while the host resolves an owning request after controller return. DLL ABI, worker execution, completion retry, and plugin stop/join remain separate phases.

**Tech Stack:** C++17, FlexUI Controller, `std::shared_ptr` immutable snapshots, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 8.5, 9.1, 9.4, 12, 13.2, and 13.4; `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P4 Service registry and P6 Service calls.

## Global Constraints

- A capability is canonical `<namespace>/<major>`, for example `document.storage/1`; namespace segments use lowercase ASCII letters, digits, `_` or `-`, and major is a non-zero decimal `uint32_t` without leading zeroes.
- An operation is non-empty lowercase ASCII and may additionally contain digits, `.`, `_`, or `-`.
- Registry growth is bounded: defaults are 64 services, 64 operations per service, 256 bytes per capability/operation identifier, and 64 KiB as the largest configurable per-operation payload; configured limits must be non-zero.
- The registry snapshot is immutable and safe to share; registration/build belongs to one host owner thread.
- Duplicate capability or duplicate operation registration fails without replacing an existing entry.
- The application manifest has explicit `allowed` and `required` capability sets; `required` must be a subset of `allowed` and every required service must exist at application build.
- An absent registry behaves as an empty registry. A script that emits a command then receives `UnknownCapability` before any same-callback UI mutation commits.
- Request validation checks authorization, service existence, operation membership, and descriptor payload byte limit without calling an endpoint.
- The request queue owns the immutable registry `shared_ptr` and one validated manifest copy; this pair is the sole application fact source for authorization and descriptor-to-endpoint routing.
- Endpoint objects use C++ `shared_ptr` only inside the host. No STL type crosses the future DLL ABI.
- This phase does not load DLLs, automatically execute endpoint code, retry mailbox backpressure, or implement PluginHost stop/join. Fake and built-in endpoints may use the submission contract directly in tests/host code.

---

## Public API Decision Requiring Approval

Create `flexUI/include/flexUI/service_registry.h` with these additive contracts:

```cpp
class ApplicationServiceRegistry;
class IApplicationServiceCompletionSink;
struct ApplicationServiceSubmitResult;

struct ApplicationServiceOperationDescriptor {
  static constexpr std::size_t kDefaultMaxPayloadBytes =
      ApplicationCommandLimits::kDefaultMaxStringBytes;
  std::string name;
  std::size_t max_payload_bytes = kDefaultMaxPayloadBytes;
};

struct ApplicationServiceRegistryLimits {
  static constexpr std::size_t kDefaultMaxServices = 64;
  static constexpr std::size_t kDefaultMaxOperationsPerService = 64;
  static constexpr std::size_t kDefaultMaxIdentifierBytes = 256;
  static constexpr std::size_t kDefaultMaxPayloadBytes =
      ApplicationCommandLimits::kDefaultMaxTotalStringBytes;
  std::size_t max_services = kDefaultMaxServices;
  std::size_t max_operations_per_service =
      kDefaultMaxOperationsPerService;
  std::size_t max_identifier_bytes = kDefaultMaxIdentifierBytes;
  std::size_t max_payload_bytes = kDefaultMaxPayloadBytes;
};

struct ApplicationServiceDescriptor {
  std::string capability;
  std::vector<ApplicationServiceOperationDescriptor> operations;
};

struct ApplicationCapabilityManifest {
  std::vector<std::string> allowed;
  std::vector<std::string> required;
};

class IApplicationServiceEndpoint {
public:
  virtual ~IApplicationServiceEndpoint() = default;
  virtual ApplicationServiceSubmitResult try_submit(
      const ApplicationServiceRequest &request,
      IApplicationServiceCompletionSink &completion_sink) = 0;
};

class IApplicationServiceCompletionSink {
public:
  virtual ~IApplicationServiceCompletionSink() = default;
  virtual ApplicationCompletionPostResult
  try_post(const ApplicationCompletion &completion) = 0;
};

enum class ApplicationServiceSubmitErrorCode {
  None,
  Busy,
  Closed,
  InvalidRequest,
  Rejected,
  InternalFailure,
};

struct ApplicationServiceSubmitError {
  ApplicationServiceSubmitErrorCode code =
      ApplicationServiceSubmitErrorCode::None;
  std::string message;
  explicit operator bool() const noexcept;
};

struct ApplicationServiceSubmitResult {
  ApplicationServiceSubmitError error;
  explicit operator bool() const noexcept;
};

enum class ApplicationServiceRegistryErrorCode {
  None,
  InvalidLimits,
  InvalidCapability,
  InvalidOperation,
  InvalidPayloadLimit,
  ServiceLimitExceeded,
  OperationLimitExceeded,
  StringLimitExceeded,
  DuplicateCapability,
  DuplicateOperation,
  MissingEndpoint,
  InvalidManifest,
  MissingRequiredCapability,
  UnauthorizedCapability,
  UnknownCapability,
  UnsupportedOperation,
  InvalidPayload,
  InvalidRequest,
  InvalidState,
  WrongThread,
  AllocationFailed,
  InternalInvariant,
};

struct ApplicationServiceRegistryError {
  ApplicationServiceRegistryErrorCode code =
      ApplicationServiceRegistryErrorCode::None;
  std::string capability;
  std::string operation;
  std::string message;
  explicit operator bool() const noexcept;
};

struct ApplicationServiceRegistryResult {
  ApplicationServiceRegistryError error;
  explicit operator bool() const noexcept;
};

struct ApplicationServiceRegistryBuildResult {
  std::shared_ptr<const ApplicationServiceRegistry> registry;
  ApplicationServiceRegistryError error;
  explicit operator bool() const noexcept;
};

struct ApplicationServiceResolveResult {
  std::shared_ptr<IApplicationServiceEndpoint> endpoint;
  ApplicationServiceRegistryError error;
  explicit operator bool() const noexcept;
};

class ApplicationServiceRegistryBuilder final {
public:
  explicit ApplicationServiceRegistryBuilder(
      ApplicationServiceRegistryLimits limits = {});
  ApplicationServiceRegistryResult register_service(
      ApplicationServiceDescriptor descriptor,
      std::shared_ptr<IApplicationServiceEndpoint> endpoint);
  ApplicationServiceRegistryBuildResult build() const;
};

class ApplicationServiceRegistry final {
public:
  ApplicationServiceRegistryResult
  validate_manifest(const ApplicationCapabilityManifest &manifest) const;
  ApplicationCommandError validate_command(
      const ApplicationCommand &command,
      const ApplicationCapabilityManifest &manifest) const;
  ApplicationServiceResolveResult resolve(
      const ApplicationServiceRequest &request,
      const ApplicationCapabilityManifest &manifest) const;
  bool contains(std::string_view capability) const noexcept;
  std::size_t size() const noexcept;
};
```

`ApplicationServiceResolveResult` owns a `shared_ptr<IApplicationServiceEndpoint>` and a structured registry error. `try_submit()` receives the request by const reference: success means the endpoint copied all retained data and accepted responsibility for exactly one completion attempt with the same token; failure means it retained no request/sink reference and posted nothing. A worker may retain the non-owning sink only while its PluginHost instance is started; stop/join must finish before application/mailbox destruction. Mailbox backpressure retry remains the endpoint/PluginHost responsibility and is not implemented by the registry.

`ApplicationCompletionMailbox` additively implements `IApplicationServiceCompletionSink`; its existing `try_post()` semantics do not change.

Extend `DesktopApplicationBuilder` and `DesktopApplication` additively:

```cpp
DesktopApplicationBuilder &services(
    std::shared_ptr<const ApplicationServiceRegistry> registry,
    ApplicationCapabilityManifest manifest);

ApplicationServiceResolveResult resolve_service_request(
    const ApplicationServiceRequest &request) const;
```

`services()` stores an immutable snapshot. `build()` validates the manifest before publishing any application state. `resolve_service_request()` is owner-thread-only and defensively repeats registry/policy validation for the owning request returned by `try_receive_service_request()`.

---

### Task 1: Immutable descriptor registry

**Files:**
- Modify: `flexUI/include/flexUI/application_completion.h`
- Create: `flexUI/include/flexUI/service_registry.h`
- Create: `flexUI/modules/services/CMakeLists.txt`
- Create: `flexUI/modules/services/service_registry.cpp`
- Modify: `flexUI/CMakeLists.txt`
- Create: `flexUI/tests/test_service_registry.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ApplicationCommand`, `ApplicationServiceRequest`, and standard owning values.
- Produces: `ApplicationServiceRegistryBuilder`, immutable `ApplicationServiceRegistry`, descriptor validation, manifest validation, and endpoint resolution.

- [x] **Step 1: Write TinyTest cases for invalid registry limits, exact service/operation capacity, canonical capability grammar, operation grammar, identifier/payload limits, duplicate capabilities/operations, and missing endpoints.**
- [x] **Step 2: Build the focused test and verify it fails because `flexUI/service_registry.h` does not exist.**
- [x] **Step 3: Implement bounded contiguous builder storage and an immutable registry snapshot; failed registration must leave prior entries unchanged and lookups must not allocate.**
- [x] **Step 4: Test allowed/required validation, duplicate manifest entries, required-not-allowed, missing required service, unauthorized service, unknown service, unsupported operation, and payload limit failures.**
- [x] **Step 5: Implement `validate_command()` so its errors map exactly to existing `ApplicationCommandErrorCode::{UnknownCapability,UnsupportedOperation,InvalidPayload}` where applicable.**
- [x] **Step 6: Verify `resolve()` returns the same shared endpoint identity without exposing mutable registry storage.**
- [x] **Step 7: Run `test_service_registry` under Debug/ASan and Release presets.**

### Task 2: Request-queue and DesktopApplication integration

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application_service_requests.hpp`
- Modify: `flexUI/modules/controller/application_service_requests.cpp`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Modify: `flexUI/tests/test_application_service_requests.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: immutable registry, validated manifest, `ApplicationCommandEngine`, and the existing fixed request table.
- Produces: fail-fast policy validation before reservation plus owner-thread endpoint resolution after publication.

- [x] **Step 1: Add a request-queue test proving unauthorized, unknown, unsupported, and oversized commands reserve no slots.**
- [x] **Step 2: Give the application-owned request queue the immutable registry `shared_ptr` and validated manifest value; validate the entire batch before allocating reservation staging.**
- [x] **Step 3: Add builder tests proving malformed policy and missing required capabilities fail before candidate publication with nested registry errors.**
- [x] **Step 4: Add a same-callback test proving registry rejection prevents UI mutation commit and command publication.**
- [x] **Step 5: Implement owner-thread `resolve_service_request()` through the request queue's registry/policy fact source, and test endpoint identity across successful reload.**
- [x] **Step 6: Preserve compatibility for applications that emit no commands; update existing service-command fixtures to install an explicit `storage/1` registry and policy.**
- [x] **Step 7: Run command, service request, desktop application, controller, and gCanvas host regressions under Debug/ASan and Release.**

### Task 3: Protocol documentation and boundary review

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: verified immutable routing, policy, queue validation, and application resolution behavior.
- Produces: the stable C++ boundary used by the later PluginHost C ABI adapter.

- [x] **Step 1: Document registry fact source, application policy ownership, validation order, endpoint lifetime, thread model, errors, reload behavior, and migration cost.**
- [x] **Step 2: Mark service namespace uniqueness, capability authorization, required build validation, and host resolution complete; keep tagged schema, DLL ABI, endpoint execution, completion retry, and PluginHost lifecycle pending.**
- [x] **Step 3: Run placeholder scan, `git diff --check`, CodeGraph sync/affected analysis, and focused Debug/ASan plus Release verification.**

## Compatibility, Migration, and Rollback

- XML/CSS/rendering and applications that emit no service commands retain existing behavior.
- Scripts that emit commands must migrate from unversioned capability strings such as `storage` to canonical names such as `storage/1`, and the builder must install an explicit allowlist. This is an intentional fail-fast security change.
- The new Services target remains below Controller and contains no Box, renderer, TurboScript, native-window, or DLL-loader dependency.
- Existing host polling and completion APIs remain source-compatible. Endpoint execution is not implied by successful resolution.
- Rollback removes the Services target and registry injection; the fixed request table and completion mailbox remain independently usable.
