# FlexUI Application Service Requests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect controller application commands to an application-owned bounded request table so the host can dispatch typed service requests and map worker completions back to the originating script request ID.

**Architecture:** A private owner-thread `ApplicationServiceRequestQueue` implements `IApplicationCommandQueue` with preallocated request slots and a fixed index ring. Reservation copies untrusted command data into inactive slots; publication only changes slot states and ring indices, so it remains allocation-free and cannot fail after UI mutation commit. `DesktopApplication` exposes value-semantic request/completion polling, while workers continue to use the existing MPSC `ApplicationCompletionMailbox` and never access the pending table.

**Tech Stack:** C++17, FlexUI Controller, fixed-capacity `std::vector` storage, TinyTest, CMake Presets.

**Spec:** `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md` sections 9.2, 13.2, 13.3, 15, and 16; `flexUI/docs/FLEXUI_DESKTOP_PLAN.md` P4 Service registry.

## Global Constraints

- The application owner thread exclusively reserves, publishes, receives, resolves, advances, closes, and destroys service request state.
- Service workers receive an owning request copy and may only post an owning `ApplicationCompletion` to the existing MPSC mailbox.
- Capacity counts all reserved, published, and in-flight requests; it is fixed at construction and never grows.
- Full capacity returns `QueueFull` before UI mutation commit; a failed reservation publishes no request and retains no pending mapping.
- `IPreparedApplicationCommands::publish()` performs no allocation, blocking, service callback, or fallible operation.
- The pending slot table is the sole fact source for `{host token -> script request ID}`. The mailbox does not duplicate request state.
- Host token IDs are non-zero and monotonically issued; discarded reservations may leave gaps and IDs are never reused within a process.
- Request order is global FIFO in controller publication order. Completion order is service-defined and may differ.
- Reload drains old mailbox completions, cancels all old request slots, then installs the new generation before publishing the detached controller candidate.
- Close rejects new commands, cancels queued/in-flight request slots, closes the completion mailbox, and remains idempotent.
- This phase does not execute DLL functions, freeze the plugin C ABI, stop/join plugin workers, or inject completion events into TurboScript.

## Public API Decision Requiring Approval

Create `flexUI/include/flexUI/application_service.h` with additive value types:

```cpp
struct ApplicationServiceRequest {
  ApplicationRequestToken token;
  std::uint64_t script_request_id = 0;
  std::string capability;
  std::string operation;
  std::string payload;
};

struct ApplicationServiceCompletion {
  std::uint64_t script_request_id = 0;
  ApplicationCompletionStatus status = ApplicationCompletionStatus::Succeeded;
  std::string payload;
  std::string error_code;
  std::string error_message;
};

struct ApplicationServiceRequestLimits {
  static constexpr std::size_t kDefaultCapacity = 256;
  std::size_t capacity = kDefaultCapacity;
};

enum class ApplicationServicePollStatus { Ready, Empty, Closed };
enum class ApplicationServiceErrorCode {
  None,
  InvalidCapacity,
  InvalidGeneration,
  InvalidRequest,
  DuplicateRequestId,
  QueueFull,
  UnknownToken,
  InvalidState,
  Closed,
  WrongThread,
  GenerationExhausted,
  AllocationFailed,
  CompletionMailboxFailed,
  InternalInvariant,
};

struct ApplicationServiceError {
  ApplicationServiceErrorCode code = ApplicationServiceErrorCode::None;
  std::string message;
  ApplicationCompletionError completion_error;

  explicit operator bool() const noexcept {
    return code != ApplicationServiceErrorCode::None;
  }
};

struct ApplicationServiceStatistics {
  std::size_t current_pending = 0;
  std::size_t peak_pending = 0;
  std::uint64_t published = 0;
  std::uint64_t dispatched = 0;
  std::uint64_t completed = 0;
  std::uint64_t cancelled = 0;
  std::uint64_t discarded = 0;
  std::uint64_t queue_full = 0;
  std::uint64_t rejected_duplicate = 0;
  std::uint64_t rejected_unknown = 0;
};

struct ApplicationServiceRequestResult {
  ApplicationServicePollStatus status = ApplicationServicePollStatus::Empty;
  std::optional<ApplicationServiceRequest> request;
  ApplicationServiceError error;
};

struct ApplicationServiceCompletionResult {
  ApplicationServicePollStatus status = ApplicationServicePollStatus::Empty;
  std::optional<ApplicationServiceCompletion> completion;
  ApplicationServiceError error;
};
```

Extend `DesktopApplicationLimits` and `DesktopApplication` additively:

```cpp
struct DesktopApplicationLimits {
  UiDocumentLimits document;
  ControllerLimits controller;
  MutationLimits mutation;
  ApplicationCompletionLimits completion;
  ApplicationServiceRequestLimits service_requests;
};

ApplicationServiceRequestResult try_receive_service_request();
ApplicationServiceCompletionResult try_receive_service_completion();
ApplicationServiceStatistics service_statistics() const noexcept;
```

`try_receive_service_request()` transfers an owning command envelope to the host and retains only token/script identity in the pending slot. `try_receive_service_completion()` polls the mailbox, validates that the token names an in-flight slot, removes the pending mapping exactly once, and returns an owning script-facing result.

---

### Task 1: Fixed-capacity request table and command reservation

**Files:**
- Create: `flexUI/include/flexUI/application_service.h`
- Create: `flexUI/modules/controller/application_service_requests.hpp`
- Create: `flexUI/modules/controller/application_service_requests.cpp`
- Modify: `flexUI/modules/controller/CMakeLists.txt`
- Create: `flexUI/tests/test_application_service_requests.cpp`
- Modify: `flexUI/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `IApplicationCommandQueue::reserve()`, `IPreparedApplicationCommands::publish()`, `ApplicationCommandBatch`, and `ApplicationRequestToken`.
- Produces: private `ApplicationServiceRequestQueue`, the approved public value/result types, and fixed request statistics.

- [x] **Step 1: Write TinyTest cases for zero capacity, exact saturation, duplicate script IDs, discarded reservations, publish-once behavior, FIFO request transfer, unknown/duplicate completion, close cancellation, and generation reset.**
- [x] **Step 2: Configure/build `test_application_service_requests` and verify the missing service request contract fails compilation.**
- [x] **Step 3: Implement preallocated slots with `Free -> Reserved -> Published -> InFlight -> Free` transitions and a fixed ring of slot indices.**
- [x] **Step 4: Ensure reservation copies all strings before returning; destruction releases reserved slots; publication only commits slot state and ring indices.**
- [x] **Step 5: Run the focused test under Debug/ASan and Release presets.**

### Task 2: DesktopApplication command/completion integration

**Files:**
- Modify: `flexUI/include/flexUI/application.h`
- Modify: `flexUI/modules/controller/application.cpp`
- Modify: `flexUI/tests/test_desktop_application.cpp`

**Interfaces:**
- Consumes: `ControllerEffects{mutations, commands}`, private request queue polling/resolution, and `ApplicationCompletionMailbox`.
- Produces: the approved `DesktopApplication` polling methods and application-level structured service errors.

- [x] **Step 1: Add a scripted application test whose event callback publishes a command only after its UI mutation commits.**
- [x] **Step 2: Add tests proving reservation failure leaves UI unchanged, request polling returns host token plus script ID, and completion polling restores the script ID exactly once.**
- [x] **Step 3: Create the request queue and command engine before candidate construction, inject both mutation and command engines into every controller candidate, and preserve their addresses across reload.**
- [x] **Step 4: Coordinate reload as detached candidate build, mailbox generation advance/drain, no-fail request-table generation reset, then active-state swap.**
- [x] **Step 5: Coordinate close as request-table close/cancel followed by mailbox close before publishing `CloseRequested`.**
- [x] **Step 6: Run controller, command, completion, desktop application, and gCanvas host regressions.**

### Task 3: Protocol documentation and verification

**Files:**
- Modify: `flexUI/docs/FLEXUI_DESKTOP_DESIGN.md`
- Modify: `flexUI/docs/FLEXUI_DESKTOP_PLAN.md`

**Interfaces:**
- Consumes: verified slot ownership, FIFO, backpressure, token mapping, reload, and close behavior.
- Produces: the host-side contract used later by ServiceRegistry, PluginHost, and the TurboScript completion-event adapter.

- [x] **Step 1: Document data unit, fact source, owner thread, slot lifecycle, capacity, backpressure, token issuance, completion reordering, close/reload cancellation, errors, and statistics.**
- [x] **Step 2: Mark only long-task request ID mapping and queue error semantics complete; retain DLL ABI, PluginHost stop/join, capability authorization, and TurboScript event delivery as pending.**
- [x] **Step 3: Run placeholder scan, `git diff --check`, CodeGraph sync/affected analysis, focused Debug/ASan tests, and focused Release tests.**

## Compatibility, Migration, and Rollback

- Existing applications that emit no commands retain the same controller, rendering, XML/CSS, and mailbox behavior; the new queue allocates fixed-capacity slot and index storage during application build.
- Script command `request_id` remains script-owned and unchanged. The host token is additive and never crosses back into script-visible data until completion resolution.
- Invalid service-request capacity becomes an application build error. Queue saturation becomes a controller command error before UI mutation commit.
- Host code migrates by polling an owning `ApplicationServiceRequest`, dispatching it to an authorized service, and echoing its token in `ApplicationCompletion`.
- Rollback removes the additive service header/methods and command-engine injection; the existing standalone command and completion primitives remain valid.
