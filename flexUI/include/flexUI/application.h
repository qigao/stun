#pragma once

#include "flexUI/application_completion.h"
#include "flexUI/application_service.h"
#include "flexUI/service_registry.h"
#include "flexUI/box.h"
#include "flexUI/controller.h"
#include "flexUI/widget_registry.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace flexUI {

class DesktopApplication;

struct ScriptModuleFactoryResult {
  std::unique_ptr<IScriptModule> module;
  ScriptModuleError error;

  explicit operator bool() const noexcept { return module != nullptr && !static_cast<bool>(error); }
};

/// Creates one isolated script module from owned application source.
///
/// The factory is called on the application owner thread. Implementations must
/// copy source and module_name before returning if they retain either value.
using ScriptModuleFactory =
    std::function<ScriptModuleFactoryResult(std::string_view source, std::string_view module_name)>;

struct DesktopApplicationSources {
  std::string ui;
  std::string stylesheet;
  std::string script;
  std::string script_module_name = "flexui-application";
};

/// Registered before the UI is built. The component's XML is compiled
/// in the same detached candidate as the base application; event/binding
/// IDs become `scope::local_id`, but all callbacks still use ONE script
/// module and the existing ordered Mutation/Command engines.
struct DesktopScopedXmlComponent {
  std::string scope;
  std::string host_id;
  std::string source;
};

/// Strict initial application-global input declaration for compiled XML
/// bindings. This is NOT a per-component namespace; duplicate names or
/// conflicting types are rejected before Box publication. Reload constructs
/// a new candidate from these immutable seed values.
struct DesktopBindingInput {
  std::string name;
  std::variant<double, bool, std::string> value;
};

/// Explicit static component-local-name to declared application input mapping.
struct DesktopScopedBindingAlias {
  std::string scope;
  std::string local_name;
  std::string application_name;
};

struct DesktopApplicationLimits {
  UiDocumentLimits document;
  ControllerLimits controller;
  MutationLimits mutation;
  ApplicationCompletionLimits completion;
  ApplicationServiceRequestLimits service_requests;
};

enum class DesktopApplicationStage {
  None,
  Configuration,
  UiCompile,
  CssLoad,
  UiInstantiation,
  ScriptModuleCreation,
  ControllerLoad,
  ControllerMount,
  ControllerEvent,
  CandidateBuild,
  Lifecycle,
  ControllerUnmount,
  ControllerFrame,
  CompletionMailbox,
  ServiceRegistry,
  ServiceRequests,
  ControllerServiceCompletion,
  ControllerEventRetirement,
};

enum class DesktopApplicationErrorCode {
  None,
  InvalidConfiguration,
  WrongThread,
  UiCompileFailed,
  CssLoadFailed,
  UiInstantiationFailed,
  ScriptRequired,
  ScriptModuleCreationFailed,
  ControllerLoadFailed,
  ControllerMountFailed,
  ControllerDispatchFailed,
  EventDispatchFailed,
  CandidateBuildFailed,
  InvalidState,
  ControllerUnmountFailed,
  InvalidArgument,
  ControllerFrameFailed,
  CompletionMailboxFailed,
  ServiceRegistryFailed,
  ServiceRequestFailed,
  GenerationExhausted,
  ControllerServiceCompletionFailed,
  ControllerEventRetirementFailed,
};

struct DesktopApplicationError {
  DesktopApplicationErrorCode code = DesktopApplicationErrorCode::None;
  DesktopApplicationStage stage = DesktopApplicationStage::None;
  std::string message;
  UiDocumentError ui_error;
  std::vector<CssDiagnostic> css_diagnostics;
  ScriptModuleError script_error;
  ControllerError controller_error;
  ApplicationCompletionError completion_error;
  ApplicationServiceError service_error;
  ApplicationServiceRegistryError registry_error;

  explicit operator bool() const noexcept { return code != DesktopApplicationErrorCode::None; }
};

struct DesktopApplicationResult {
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Outcome of a queue-only quiescence after the same component's SCRIPT
/// event routes have already been retired. This does NOT stop worker execution
/// or detach Elements/Bindings/Widgets/resources.
struct DesktopApplicationScopedServiceQuiesceResult {
  std::size_t cancelled_queued = 0;
  /// Newly marked InFlight tokens on this call, NOT total outstanding host
  /// work. The worker continues until its real completion is observed.
  std::size_t newly_suppressed_in_flight = 0;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Owner-thread view of the sole queue's outstanding exact-origin request
/// leases AND the one Controller's scoped event route retirement marker.
/// This contains no worker join or plugin DLL resource-lifetime evidence.
struct DesktopApplicationScopedServiceDrainSnapshot {
  bool event_routes_retired = false;
  bool admission_quiesced = false;
  std::size_t reserved = 0;
  std::size_t published = 0;
  std::size_t in_flight = 0;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
  std::size_t pending() const noexcept { return reserved + published + in_flight; }
  /// Ready for the NEXT resource-lifetime preflight, not for unmount!
  /// A worker can retain the cancellation lease after completion was posted;
  /// an endpoint must still provide independent release/join proof.
  bool queue_drained() const noexcept {
    return !static_cast<bool>(error) && event_routes_retired &&
           admission_quiesced && pending() == 0;
  }
};

/// Independent resource join proof for the current application generation.
/// Does not imply Widget/Binding release or physical unmount.
struct DesktopApplicationScopedResourceJoinResult {
  std::uint64_t generation = 0;
  std::size_t joined_endpoints = 0;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// A bounded, owner-thread-only validation of compiler-attributed nodes against
/// the CURRENT live Box. No Element* is retained or returned. This proves only
/// topology/ID/Widget factory presence; it does NOT prove focus/capture,
/// Widget/Binding observer quiescence, worker join, or safe-to-unmount.
struct DesktopScopedComponentTopologySnapshot {
  std::size_t resolved_compiled_nodes = 0;
  std::size_t compiled_widget_hosts = 0;
  std::size_t compiled_event_nodes = 0;
  std::size_t compiled_binding_nodes = 0;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Application-validated scoped root plus the sole Box's read-only borrower
/// snapshot. No resource is freed, no worker joins, and no unmount is granted.
struct DesktopScopedComponentBorrowSnapshot {
  BoxSubtreeBorrowSnapshot borrowers;
  DesktopApplicationError error;
  explicit operator bool() const noexcept {
    return !static_cast<bool>(error) && static_cast<bool>(borrowers);
  }
};

/// Generation-local READ-ONLY composition of the three independent
/// retirement observations. It is expressly NOT a safe-unmount certificate:
/// it cannot join native workers, disconnect callback closures/observers,
/// stage compiled-program or Widget destruction, or authorize deletion.
struct DesktopScopedRetirementDiagnostics {
  std::uint64_t generation = 0;
  DesktopScopedComponentTopologySnapshot topology;
  DesktopScopedComponentBorrowSnapshot element_borrowers;
  DesktopApplicationScopedServiceDrainSnapshot service_drain;
  DesktopApplicationError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error) &&
           static_cast<bool>(topology) &&
           static_cast<bool>(element_borrowers) &&
           static_cast<bool>(service_drain);
  }
};

struct DesktopApplicationBuildResult {
  std::unique_ptr<DesktopApplication> application;
  DesktopApplicationError error;

  explicit operator bool() const noexcept {
    return application != nullptr && !static_cast<bool>(error);
  }
};

/// Result of consuming and optionally dispatching one resolved service
/// completion to the active script controller.
struct DesktopApplicationCompletionDispatchResult {
  ApplicationServicePollStatus status = ApplicationServicePollStatus::Empty;
  bool dispatched = false;
  std::optional<ApplicationServiceCompletion> completion;
  DesktopApplicationError error;

  explicit operator bool() const noexcept {
    return status == ApplicationServicePollStatus::Ready && completion.has_value() &&
           !static_cast<bool>(error);
  }
};

/// Monotonic owner-thread lifecycle of one published desktop application.
enum class DesktopApplicationState {
  Ready,
  CloseRequested,
  Shutdown,
};

/// Published owner-thread application state for one UI/CSS/script bundle.
///
/// The application owns Box, compiled program, mutation adapter and optional
/// mounted controller. The renderer passed to the builder is borrowed and must
/// outlive the application. Reload builds a complete detached candidate and
/// publishes it with one no-fail owner swap. All access must remain on the
/// thread that called DesktopApplicationBuilder::build().
class DesktopApplication final {
public:
  ~DesktopApplication();

  DesktopApplication(const DesktopApplication &) = delete;
  DesktopApplication &operator=(const DesktopApplication &) = delete;
  DesktopApplication(DesktopApplication &&) = delete;
  DesktopApplication &operator=(DesktopApplication &&) = delete;

  Box &box() noexcept;
  const Box &box() const noexcept;
  const CompiledUiProgram &program() const noexcept;
  /// Compiler-produced structural provenance MOVED with this published
  /// generation. Exact root ordinals/parent indices include anonymous Widget
  /// hosts, but are NOT live resource leases or approval to detach subtrees.
  /// Call on the owner thread, like program().
  const std::vector<UiScopedComponentOwnership> &
  scoped_component_ownership() const noexcept;
  /// Verify that each compiled per-scope node resolves by its parent ordinal
  /// and ID/tag/Widget kind in this application's live Box. Reject on owner
  /// mismatch, missing anonymous nodes, or structural drift WITHOUT mutating
  /// the Box or publishing a new runtime owner table. This is only a necessary
  /// stage of the future unmount preflight, NOT resource release evidence.
  DesktopScopedComponentTopologySnapshot
  inspect_scoped_component_topology(std::string_view scope) const;
  /// Read current Box borrowers of the exact compiled component root after
  /// validating topology. The root is located by host ID and compiler ordinal,
  /// never a possibly missing/root-local Element ID. This does not validate
  /// plugin join, Binding observer release, or safe unmount.
  DesktopScopedComponentBorrowSnapshot
  inspect_scoped_component_borrowers(std::string_view scope) const;
  /// Combine compiler topology, current Box borrowers (including external
  /// closure owners), and exact-scope service drain under one generation.
  /// Read-only, no join/detach/retire, and intentionally NO ready-to-unmount
  /// predicate: joined Endpoint/Widget/Binding lifetimes are not proved here.
  DesktopScopedRetirementDiagnostics
  inspect_scoped_retirement_diagnostics(std::string_view scope) const;
  ScriptController *controller() noexcept;
  const ScriptController *controller() const noexcept;
  const DesktopApplicationSources &sources() const noexcept;
  bool uses_legacy_flex_compatibility() const noexcept;
  DesktopApplicationState state() const noexcept;
  /// Current application incarnation used in host-issued service tokens. This
  /// atomic query is safe from producer threads.
  std::uint64_t generation() const noexcept;
  /// The returned object is application-owned. Only try_post() may be called
  /// outside the application owner thread.
  ApplicationCompletionMailbox &completion_mailbox() noexcept;
  const ApplicationCompletionMailbox &completion_mailbox() const noexcept;

  /// Transfers at most one published command to the owner-thread host. The
  /// returned request owns its strings; its token remains pending until a
  /// matching completion is consumed or the application reloads/closes.
  /// @return `WrongThread` outside the owner thread, `Closed` after close, or
  ///         `Ready`/`Empty` without blocking while the application is ready.
  ApplicationServiceRequestResult try_receive_service_request();

  /// Resolves a received owning request against this application's immutable
  /// registry and capability policy without invoking the endpoint.
  ApplicationServiceResolveResult resolve_service_request(
      const ApplicationServiceRequest &request) const;

  /// Consumes at most one worker completion and restores its script request
  /// identity. Unknown or out-of-order tokens fail explicitly.
  /// @return `WrongThread` outside the owner thread, `Closed` after close, or
  ///         a structured mailbox/token result without blocking.
  ApplicationServiceCompletionResult try_receive_service_completion();

  /// Consumes at most one resolved completion and synchronously calls the
  /// optional `on_service_completion` export. This and raw completion polling
  /// share one authoritative mailbox consumer and must not be mixed.
  /// @return `Ready` with an owning completion and `dispatched == false` when
  ///         no script handler exists; `Suppressed` with no script callback for
  ///         an in-flight request whose scoped origin was quiesced;
  ///         `Empty`/`Closed` without a callback; or a nested error.
  DesktopApplicationCompletionDispatchResult try_dispatch_service_completion();

  /// Owner-thread snapshot of bounded request-table counters.
  ApplicationServiceStatistics service_statistics() const noexcept;

  /// Reports whether the caller may access owner-thread application state.
  /// This query does not read the active Box and is safe from any thread.
  bool is_owner_thread() const noexcept;

  /// Stops future event dispatch and reload work without destroying the
  /// published UI state. Repeated owner-thread calls are idempotent.
  DesktopApplicationResult request_close();

  /// Unmounts the optional controller after close was requested. Published
  /// Box, program and source state remain readable until destruction.
  /// Repeated calls after completed cleanup are idempotent.
  DesktopApplicationResult shutdown();

  /// Routes one native event through widgets and the optional script
  /// controller. Script callbacks run only for unconsumed compiled bindings.
  /// This operation must run on the application owner thread.
  DesktopApplicationResult dispatch_event(Event &event);

  /// Calls the optional script on_frame callback with a finite, non-negative
  /// delta. This operation does not advance Box animation time or render; the
  /// desktop host owns those frame stages.
  DesktopApplicationResult frame(double delta_seconds);

  /// Builds and validates a replacement before changing active state. This
  /// operation must run on the application owner thread.
  /// @return Success after atomic publication, or a structured candidate error
  ///         while the current Box, program and controller remain unchanged.
  DesktopApplicationResult reload(DesktopApplicationSources sources);

  /// Atomically replace the complete set of statically compiled XML
  /// components at the next application generation boundary. The supplied
  /// vector is the full desired set (including an empty set for removal);
  /// input aliases are also supplied in full and must refer to explicitly
  /// declared application inputs. Candidate compile, registry, typed binding,
  /// controller and service preflight complete before publication. Failure
  /// preserves the active generation, Box, program and stored composition.
  ///
  /// This is a FULL generation-scoped recomposition, not in-place fragment
  /// insertion/removal, live handler registration, component-private state or
  /// an independently owned script context. Current app source, CSS, script,
  /// input initial seeds and capability manifest are reused.
  /// Requires owner thread and Ready state; successful publication resets
  /// completion/service generations exactly like reload().
  DesktopApplicationResult recompose_scoped_xml_components(
      std::vector<DesktopScopedXmlComponent> components,
      std::vector<DesktopScopedBindingAlias> input_aliases);

  /// Retire only pre-resolved script event callbacks for a registered XML
  /// component scope in the current generation. No Element, Binding, Widget,
  /// resource or service request is removed/cancelled. Reentrant native
  /// bubbling is rejected, and complete reload/recompose resets the table.
  /// This is not a component unmount operation.
  DesktopApplicationResult retire_scoped_event_handlers(
      std::string_view scope);

  /// Quiesce request admission only AFTER the controller has retired every
  /// event route in this registered scope. Rejects a live event route and any
  /// callback/native bubbling reentrance; does not unmount component state.
  /// Published unsubmitted commands are cancelled; in-flight host tokens are
  /// retained until real completion and suppressed from script delivery.
  DesktopApplicationScopedServiceQuiesceResult
  quiesce_retired_scoped_service_requests(std::string_view scope);

  /// Nonmutating, generation-local snapshot of the current compiled XML
  /// scope's retired event routes, admission tombstone, and pending service
  /// token leases. Even queue_drained()==true does NOT prove host worker exit,
  /// plugin join, Box/Widget/Binding ownership release, or safe unmount.
  /// No endpoint callbacks or completion mailbox consumption occur here.
  DesktopApplicationScopedServiceDrainSnapshot
  inspect_scoped_service_drain(std::string_view scope) const;

  /// Requires exact-scope routes retired, service admission quiesced and all
  /// real terminals consumed. Delegates to the one immutable ServiceRegistry;
  /// unsupported native providers fail closed. Never detaches Elements.
  DesktopApplicationScopedResourceJoinResult join_scoped_service_resources(
      std::string_view scope, std::chrono::milliseconds timeout);

private:
  struct Impl;
  explicit DesktopApplication(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;

  friend class DesktopApplicationBuilder;
};

/// Fluent candidate builder. XML is the default and recommended entry format.
class DesktopApplicationBuilder final {
public:
  /// @param renderer Borrowed renderer; null is valid for headless operation.
  explicit DesktopApplicationBuilder(flex::Renderer *renderer = nullptr);
  ~DesktopApplicationBuilder();

  DesktopApplicationBuilder(const DesktopApplicationBuilder &) = delete;
  DesktopApplicationBuilder &operator=(const DesktopApplicationBuilder &) = delete;
  DesktopApplicationBuilder(DesktopApplicationBuilder &&) noexcept;
  DesktopApplicationBuilder &operator=(DesktopApplicationBuilder &&) noexcept;

  DesktopApplicationBuilder &xml_entry(std::string source);

  /// Statically compose an XML fragment below a structural host element ID.
  /// Scopes must be unique; local element IDs are requalified before
  /// event/binding compilation and share the one application ScriptController.
  /// Components are recompiled for each atomic application reload.
  /// Dynamic per-component script registration/unmount is intentionally
  /// unsupported until it has its own ownership and cancellation contract.
  DesktopApplicationBuilder &scoped_xml_component(
      std::string scope, std::string host_id, std::string source);

  /// Redirect one static component template's binding input source to an
  /// explicitly declared typed application-global input. No implicit
  /// namespace, runtime per-component owner or dynamic unregister is created.
  DesktopApplicationBuilder &scoped_binding_alias(
      std::string scope, std::string local_name, std::string application_name);

  /// Declare a bounded application-global input before XML bindings mount.
  /// Input names are shared explicitly across components. Every candidate
  /// and reload starts with the supplied typed initial value.
  DesktopApplicationBuilder &binding_input_string(std::string name,
                                                   std::string value);
  DesktopApplicationBuilder &binding_input_bool(std::string name, bool value);
  DesktopApplicationBuilder &binding_input_number(std::string name, double value);

  /// Explicit migration-only entry for the retired `.flex` UI syntax.
  DesktopApplicationBuilder &legacy_flex_entry_compatibility(std::string source);

  DesktopApplicationBuilder &stylesheet(std::string source);
  DesktopApplicationBuilder &script(std::string source, ScriptModuleFactory factory,
                                    std::string module_name = "flexui-application");
  /// Replaces the borrowed renderer used by subsequently built candidates.
  DesktopApplicationBuilder &renderer(flex::Renderer *renderer) noexcept;
  DesktopApplicationBuilder &box_options(BoxOptions options);
  DesktopApplicationBuilder &limits(DesktopApplicationLimits limits);
  /// Installs a borrowed allocation-free callback invoked after successful
  /// completion publication. Its context must outlive the built application
  /// and all producers; request_close() quiesces callbacks before returning.
  DesktopApplicationBuilder &
  completion_wakeup(ApplicationCompletionWakeup wakeup) noexcept;
  DesktopApplicationBuilder &widget_registry(WidgetRegistry registry);
  DesktopApplicationBuilder &services(
      std::shared_ptr<const ApplicationServiceRegistry> registry,
      ApplicationCapabilityManifest manifest);

  /// Validates and mounts a detached candidate before returning ownership.
  DesktopApplicationBuildResult build() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
