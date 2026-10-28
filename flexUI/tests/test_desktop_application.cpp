#include <flexUI/application.h>
#include <flexUI/application_host_bridge.h>
#include <flexUI/render_command.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>

#include <tinytest.hpp>

#include <atomic>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void count_completion_wakeup(void *context) noexcept {
  static_cast<std::atomic<std::uint64_t> *>(context)->fetch_add(
      1, std::memory_order_release);
}

class JoinedStorageEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &,
      flexUI::IApplicationServiceCompletionSink &) override { return {}; }

  flexUI::ApplicationEndpointScopeJoinCode
  join_scoped_resources(std::string_view scope,
                        std::chrono::milliseconds) override {
    ++join_count;
    last_scope.assign(scope);
    return may_join ? flexUI::ApplicationEndpointScopeJoinCode::Joined
                    : flexUI::ApplicationEndpointScopeJoinCode::Busy;
  }

  bool may_join = false;
  std::size_t join_count = 0;
  std::string last_scope;
};

class TestStorageEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &,
      flexUI::IApplicationServiceCompletionSink &) override {
    return {};
  }
};

std::shared_ptr<TestStorageEndpoint> configure_storage_service(
    flexUI::DesktopApplicationBuilder &application_builder) {
  auto endpoint = std::make_shared<TestStorageEndpoint>();
  flexUI::ApplicationServiceRegistryBuilder registry_builder;
  check(registry_builder.register_service(
      {"storage/1", {{"write", 1024}}}, endpoint));
  auto registry = registry_builder.build();
  check(registry);
  application_builder.services(std::move(registry.registry),
                               {{"storage/1"}, {"storage/1"}});
  return endpoint;
}

struct ModuleProbe {
  std::vector<std::string> created_sources;
  std::vector<std::string> created_names;
  std::vector<std::string> calls;
  std::vector<std::string> destroyed;
  std::vector<flexUI::ScriptEventSnapshot> events;
  std::vector<flexUI::ApplicationServiceCompletion> service_completions;
  std::vector<double> frame_deltas;
  std::vector<std::string> sequence;
  flexUI::DesktopApplication *application = nullptr;
  flexUI::DesktopApplicationResult reentrant_shutdown;
  bool shutdown_during_event = false;
  bool emit_service_command = false;
  bool emit_valid_mutation = false;
  bool emit_invalid_mutation = false;
  std::uint64_t service_request_id = 71;
};

class FakeApplicationModule final : public flexUI::IScriptModule {
public:
  FakeApplicationModule(std::vector<std::string> exports, std::shared_ptr<ModuleProbe> probe,
                        bool fail_mount, bool fail_event, bool fail_frame, bool fail_unmount,
                        bool fail_service_completion)
      : probe_(std::move(probe)), fail_mount_(fail_mount), fail_event_(fail_event),
        fail_frame_(fail_frame), fail_unmount_(fail_unmount),
        fail_service_completion_(fail_service_completion) {
    std::uint64_t next_handle = 1;
    for (auto &name : exports) {
      const flexUI::ScriptExportHandle handle{next_handle++};
      names_.emplace(handle.value, name);
      handles_.emplace(std::move(name), handle);
    }
  }

  ~FakeApplicationModule() override { probe_->destroyed.push_back("module"); }

  flexUI::ScriptResolveResult resolve_export(std::string_view name,
                                             flexUI::ScriptCallbackKind) override {
    const auto found = handles_.find(std::string(name));
    return found == handles_.end() ? flexUI::ScriptResolveResult{}
                                   : flexUI::ScriptResolveResult{found->second, {}};
  }

  flexUI::ScriptCallResult call(flexUI::ScriptExportHandle handle,
                                const flexUI::ScriptCallContext &context) override {
    const auto found = names_.find(handle.value);
    if (found == names_.end()) {
      return {{flexUI::ScriptModuleErrorCode::InvalidExport, "unknown fake application export"}};
    }
    probe_->calls.push_back(found->second);
    probe_->sequence.push_back("script:" + found->second);
    if (context.event != nullptr) {
      probe_->events.push_back(*context.event);
    }
    if (context.service_completion != nullptr) {
      probe_->service_completions.push_back(*context.service_completion);
    }
    if (fail_mount_ && found->second == "on_mount") {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected mount failure"}};
    }
    if (fail_event_ && context.callback == flexUI::ScriptCallbackKind::Event) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected event failure"}};
    }
    if (fail_service_completion_ &&
        context.callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
               "injected service completion failure"}};
    }
    if (context.callback == flexUI::ScriptCallbackKind::Frame) {
      probe_->frame_deltas.push_back(context.delta_seconds);
    }
    if (fail_frame_ && context.callback == flexUI::ScriptCallbackKind::Frame) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected frame failure"}};
    }
    if (fail_unmount_ && context.callback == flexUI::ScriptCallbackKind::Unmount) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure, "injected unmount failure"}};
    }
    if (probe_->shutdown_during_event && context.callback == flexUI::ScriptCallbackKind::Event &&
        probe_->application != nullptr) {
      static_cast<void>(probe_->application->request_close());
      probe_->reentrant_shutdown = probe_->application->shutdown();
    }
    flexUI::ScriptCallResult result;
    if (context.callback == flexUI::ScriptCallbackKind::Event && probe_->emit_service_command) {
      const auto appended =
          result.commands.append(
              {probe_->service_request_id, "storage/1", "write", "document"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
    }
    if (context.callback == flexUI::ScriptCallbackKind::Event &&
        (probe_->emit_valid_mutation || probe_->emit_invalid_mutation)) {
      const flexUI::UiHandle target =
          probe_->emit_invalid_mutation
              ? flexUI::UiHandle{"missing", context.event->current_target.generation}
              : context.event->current_target;
      const auto appended = result.mutations.append(flexUI::SetTextMutation{target, "Saved"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
    }
    if (context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        probe_->emit_valid_mutation) {
      const auto appended = result.mutations.append(
          flexUI::SetTextMutation{{"save", 1}, "Completion applied"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
    }
    return result;
  }

private:
  std::shared_ptr<ModuleProbe> probe_;
  bool fail_mount_ = false;
  bool fail_event_ = false;
  bool fail_frame_ = false;
  bool fail_unmount_ = false;
  bool fail_service_completion_ = false;
  std::unordered_map<std::string, flexUI::ScriptExportHandle> handles_;
  std::unordered_map<std::uint64_t, std::string> names_;
};

flexUI::ScriptModuleFactory fake_factory(const std::shared_ptr<ModuleProbe> &probe) {
  return [probe](std::string_view source, std::string_view module_name) {
    probe->created_sources.emplace_back(source);
    probe->created_names.emplace_back(module_name);

    std::vector<std::string> exports{"on_mount", "on_frame", "on_unmount", "parent_handler",
                                     "consumed_handler"};
    if (source != "missing-handler") {
      exports.emplace_back("save_document");
    }
    if (source == "service-completion-controller" ||
        source == "service-completion-failure") {
      exports.emplace_back("on_service_completion");
    }
    return flexUI::ScriptModuleFactoryResult{
        std::make_unique<FakeApplicationModule>(
            std::move(exports), probe, source == "mount-failure", source == "event-failure",
            source == "frame-failure", source == "unmount-failure",
            source == "service-completion-failure"),
        {}};
  };
}

std::string xml_entry(std::string_view id = "save") {
  return "<ui name=\"Desktop\" xmlns:on=\"urn:flexui:event\">"
         "<button id=\"" +
         std::string(id) +
         "\" text=\"Save\" on:click=\"save_document\"/>"
         "</ui>";
}

void check_single_value(const std::vector<std::string> &actual, std::string_view expected) {
  check_equal(actual.size(), std::size_t{1});
  if (!actual.empty()) {
    check_equal(actual.front(), std::string(expected));
  }
}

flexUI::DesktopApplicationResult dispatch_click(flexUI::DesktopApplication &application,
                                                float x = 10.0F, float y = 10.0F) {
  auto down = flexUI::Event::mouse_down(x, y);
  auto result = application.dispatch_event(down);
  if (!result) {
    return result;
  }
  auto up = flexUI::Event::mouse_up(x, y);
  return application.dispatch_event(up);
}

} // namespace

spec("FlexUI desktop scoped service quiescence facade") {
  it("requires retired event routes and suppresses only the retired scope completion") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    const std::string root =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<button id=\"action\" on:key_down=\"save_document\">Save</button></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left", panel)
        .scoped_xml_component("sales", "right", panel)
        .script("service-completion-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    // Zero pending tokens BEFORE route/admission retirement must not be
    // misrepresented as a completed component lifetime.
    const auto initial_drain = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(initial_drain));
    check_false(initial_drain.event_routes_retired);
    check_false(initial_drain.admission_quiesced);
    check(initial_drain.pending() == std::size_t{0});
    check_false(initial_drain.queue_drained());
    auto *left = app.box().get_by_id("sale::action");
    auto *right = app.box().get_by_id("sales::action");
    check_not_null(left);
    check_not_null(right);
    if (!left || !right) return;
    auto run_key = [&](flexUI::Element *button, std::uint64_t id) {
      probe->service_request_id = id;
      app.box().set_focus(button);
      auto native = flexUI::Event::key_down(flexUI::KeyCode::A);
      return app.dispatch_event(native);
    };

    check(run_key(left, 71));
    auto first = app.try_receive_service_request();
    check(first);
    if (!first) return;
    check_equal(first.request->script_request_id, std::uint64_t{71});
    check(run_key(right, 72));
    check(run_key(left, 73));
    check_equal(app.service_statistics().current_pending, std::size_t{3});

    // Closing service admission before the Controller event routes would
    // fault the ONE shared controller when a later scope event emits commands.
    auto premature = app.quiesce_retired_scoped_service_requests("sale");
    check_false(static_cast<bool>(premature));
    check(premature.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check_equal(app.service_statistics().current_pending, std::size_t{3});
    const auto before_drain = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(before_drain));
    check_false(before_drain.event_routes_retired);
    check_false(before_drain.admission_quiesced);
    check(before_drain.in_flight == std::size_t{1});
    check(before_drain.published == std::size_t{1});
    check(before_drain.pending() == std::size_t{2});
    check_false(before_drain.queue_drained());

    flexUI::DesktopApplicationErrorCode wrong_thread =
        flexUI::DesktopApplicationErrorCode::None;
    std::thread foreign([&] {
      wrong_thread = app.quiesce_retired_scoped_service_requests("sale").error.code;
    });
    foreign.join();
    check(wrong_thread == flexUI::DesktopApplicationErrorCode::WrongThread);
    flexUI::DesktopApplicationScopedServiceDrainSnapshot foreign_drain;
    std::thread foreign_inspector([&] {
      foreign_drain = app.inspect_scoped_service_drain("sale");
    });
    foreign_inspector.join();
    check(foreign_drain.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_false(foreign_drain.queue_drained());
    const auto missing_drain = app.inspect_scoped_service_drain("other");
    check(missing_drain.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    auto unknown = app.quiesce_retired_scoped_service_requests("other");
    check_false(static_cast<bool>(unknown));
    check(unknown.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    check(app.retire_scoped_event_handlers("sale"));
    const auto routes_only = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(routes_only));
    check(routes_only.event_routes_retired);
    check_false(routes_only.admission_quiesced);
    check(routes_only.pending() == std::size_t{2});
    check_false(routes_only.queue_drained());
    // Even after route retirement, do not allow the queue gate to change in
    // the middle of a different scope's native bubbling dispatch.
    bool native_reentry_refused = false;
    app.box().set_event_callback(
        [&](flexUI::Element &node, const flexUI::Event &native) {
          if (&node == right && native.type == flexUI::EventType::KeyDown) {
            const auto refused = app.quiesce_retired_scoped_service_requests("sale");
            native_reentry_refused = !static_cast<bool>(refused) &&
                refused.error.code == flexUI::DesktopApplicationErrorCode::InvalidState;
            const auto inspect = app.inspect_scoped_service_drain("sale");
            native_reentry_refused = native_reentry_refused &&
                inspect.error.code == flexUI::DesktopApplicationErrorCode::InvalidState;
          }
        });
    probe->emit_service_command = false;
    app.box().set_focus(right);
    auto nested = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(nested));
    probe->emit_service_command = true;
    app.box().set_event_callback({});
    check(native_reentry_refused);
    check_equal(app.service_statistics().current_pending, std::size_t{3});
    check(app.controller()->is_scoped_event_route_retired("sale"));
    check_false(app.controller()->is_scoped_event_route_retired("sales"));
    const auto generation = app.generation();
    const auto quiesced = app.quiesce_retired_scoped_service_requests("sale");
    check(quiesced);
    check_equal(quiesced.cancelled_queued, std::size_t{1});
    check_equal(quiesced.newly_suppressed_in_flight, std::size_t{1});
    // Cancellation has been REQUESTED, but the worker has not posted/retired
    // its accepted token. The sibling scope must remain independent.
    const auto in_flight_drain = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(in_flight_drain));
    check(in_flight_drain.event_routes_retired);
    check(in_flight_drain.admission_quiesced);
    check(in_flight_drain.reserved == std::size_t{0});
    check(in_flight_drain.published == std::size_t{0});
    check(in_flight_drain.in_flight == std::size_t{1});
    check_false(in_flight_drain.queue_drained());
    const auto sibling_drain = app.inspect_scoped_service_drain("sales");
    check(static_cast<bool>(sibling_drain));
    check_false(sibling_drain.event_routes_retired);
    check_false(sibling_drain.admission_quiesced);
    check(sibling_drain.published == std::size_t{1});
    check_false(sibling_drain.queue_drained());
    const auto repeated = app.quiesce_retired_scoped_service_requests("sale");
    check(repeated);
    check_equal(repeated.cancelled_queued, std::size_t{0});
    check_equal(repeated.newly_suppressed_in_flight, std::size_t{0});
    check_equal(app.generation(), generation);
    check_not_null(app.box().get_by_id("sale::action"));
    check(app.controller()->has_active_event_binding(
        "sales::action", flexUI::UiEventKind::KeyDown));
    check_false(app.controller()->has_active_event_binding(
        "sale::action", flexUI::UiEventKind::KeyDown));
    check_equal(app.service_statistics().current_pending, std::size_t{2});

    auto sibling = app.try_receive_service_request();
    check(sibling);
    if (!sibling) return;
    check_equal(sibling.request->script_request_id, std::uint64_t{72});
    check(app.try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    flexUI::ApplicationCompletion gone;
    gone.token = first.request->token;
    gone.payload = "stale-scope";
    flexUI::ApplicationCompletion live;
    live.token = sibling.request->token;
    live.payload = "active-scope";
    check(app.completion_mailbox().try_post(gone));
    check(app.completion_mailbox().try_post(live));
    // Posted in the mailbox != consumed by the owner: the original slot
    // remains InFlight until this application resolves the real record.
    const auto posted_drain = app.inspect_scoped_service_drain("sale");
    check(posted_drain.in_flight == std::size_t{1});
    check_false(posted_drain.queue_drained());

    auto suppressed = app.try_dispatch_service_completion();
    check_false(static_cast<bool>(suppressed.error));
    check(suppressed.status == flexUI::ApplicationServicePollStatus::Suppressed);
    check_false(suppressed.dispatched);
    check_false(suppressed.completion.has_value());
    check(probe->service_completions.empty());
    const auto drained_scope = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(drained_scope));
    check(drained_scope.queue_drained());
    check(drained_scope.pending() == std::size_t{0});
    // IMPORTANT: queue_drained here proves only queue identity retirement.
    // It does not prove an endpoint thread has exited or PluginHost joined.
    const auto sibling_still_live = app.inspect_scoped_service_drain("sales");
    check(sibling_still_live.in_flight == std::size_t{1});
    check_false(sibling_still_live.queue_drained());
    auto dispatched = app.try_dispatch_service_completion();
    check(static_cast<bool>(dispatched));
    check_true(dispatched.dispatched);
    check_equal(probe->service_completions.size(), std::size_t{1});
    check_equal(probe->service_completions.front().script_request_id,
                std::uint64_t{72});
    check_equal(probe->service_completions.front().payload,
                std::string("active-scope"));
    check_equal(app.service_statistics().suppressed_completions, std::uint64_t{1});
    check_equal(app.service_statistics().current_pending, std::size_t{0});
    check(app.controller()->state() == flexUI::ControllerState::Mounted);

    check(app.reload({root, "", "service-completion-controller", "fresh-scope"}));
    check_equal(app.generation(), generation + 1);
    check_false(app.controller()->is_scoped_event_route_retired("sale"));
    const auto next_generation_drain = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(next_generation_drain));
    check_false(next_generation_drain.admission_quiesced);
    check_false(next_generation_drain.event_routes_retired);
    check_false(next_generation_drain.queue_drained());
    auto *fresh = app.box().get_by_id("sale::action");
    check_not_null(fresh);
    if (!fresh) return;
    check(run_key(fresh, 74));
    auto new_request = app.try_receive_service_request();
    check(new_request);
    if (!new_request) return;
    check_equal(new_request.request->script_request_id, std::uint64_t{74});
    check_equal(new_request.request->token.generation, generation + 1);
  }
}

spec("FlexUI scoped Endpoint resource join admission") {
  it("requires route retirement, queue drain and independent native join") {
    const std::string root =
        "<ui name=\"ScopedResource\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    flexUI::ApplicationServiceRegistryBuilder services;
    auto endpoint = std::make_shared<JoinedStorageEndpoint>();
    check(services.register_service({"storage/1", {{"write", 1024}}}, endpoint));
    auto registry = services.build();
    check(static_cast<bool>(registry));
    if (!registry) return;

    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left",
            "<ui name=\"Sale\"><div><button id=\"send\">Send</button></div></ui>")
        .scoped_xml_component("sales", "right",
            "<ui name=\"Sales\"><div id=\"label\">Keep</div></ui>")
        .services(std::move(registry.registry), {{"storage/1"}, {"storage/1"}});
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto *sibling = app.box().get_by_id("sales::label");
    check_not_null(sibling);
    if (!sibling) return;
    const auto sibling_handle = app.box().handle_for(*sibling);
    const auto generation = app.generation();

    auto premature = app.join_scoped_service_resources(
        "sale", std::chrono::milliseconds{0});
    check_false(static_cast<bool>(premature));
    check(premature.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check_equal(endpoint->join_count, std::size_t{0});
    check(app.retire_scoped_event_handlers("sale"));
    check(app.quiesce_retired_scoped_service_requests("sale"));
    check(app.inspect_scoped_service_drain("sale").queue_drained());

    auto busy = app.join_scoped_service_resources(
        "sale", std::chrono::milliseconds{0});
    check_false(static_cast<bool>(busy));
    check(busy.error.code == flexUI::DesktopApplicationErrorCode::ServiceRegistryFailed);
    check(busy.error.registry_error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::EndpointJoinBusy);
    check_equal(endpoint->join_count, std::size_t{1});

    endpoint->may_join = true;
    auto joined = app.join_scoped_service_resources(
        "sale", std::chrono::milliseconds{10});
    check(static_cast<bool>(joined));
    check_equal(joined.generation, generation);
    check_equal(joined.joined_endpoints, std::size_t{1});
    check_equal(endpoint->last_scope, std::string("sale"));
    check_equal(app.box().resolve_handle(sibling_handle), sibling);
    check_not_null(app.box().get_by_id("sale::send"));

    flexUI::DesktopApplicationScopedResourceJoinResult foreign_thread;
    std::thread worker([&] {
      foreign_thread = app.join_scoped_service_resources(
          "sale", std::chrono::milliseconds{0});
    });
    worker.join();
    check(foreign_thread.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    auto missing = app.join_scoped_service_resources(
        "missing", std::chrono::milliseconds{0});
    check_false(static_cast<bool>(missing));
    check(missing.error.code == flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    check(app.reload({root, "", "", "new-generation"}));
    auto stale = app.join_scoped_service_resources(
        "sale", std::chrono::milliseconds{0});
    check_false(static_cast<bool>(stale));
    check(stale.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check_not_null(app.box().get_by_id("sales::label"));
  }

  it("refuses unsupported Endpoint despite the scoped queue being drained") {
    const std::string root =
        "<ui name=\"Unsupported\"><main id=\"root\">"
        "<section id=\"host\"/></main></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root).scoped_xml_component(
        "sale", "host",
        "<ui name=\"Passive\"><div id=\"label\">NoEvents</div></ui>");
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check(app.retire_scoped_event_handlers("sale"));
    check(app.quiesce_retired_scoped_service_requests("sale"));
    check(app.inspect_scoped_service_drain("sale").queue_drained());
    auto unsupported = app.join_scoped_service_resources(
        "sale", std::chrono::milliseconds{0});
    check_false(static_cast<bool>(unsupported));
    check(unsupported.error.registry_error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::EndpointJoinUnsupported);
    check_not_null(app.box().get_by_id("sale::label"));
  }
}

spec("FlexUI eventless scoped retirement admission") {
  it("retires zero routes using the compiled ledger without inventing a handler") {
    auto probe = std::make_shared<ModuleProbe>();
    const std::string root =
        "<ui name=\"ScopedEventless\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string passive =
        "<ui name=\"Passive\"><div><button id=\"action\">Passive</button>"
        "<span>Anonymous</span></div></ui>";
    const std::string active =
        "<ui name=\"Active\" xmlns:on=\"urn:flexui:event\">"
        "<button id=\"action\" on:key_down=\"save_document\">Run</button></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left", passive)
        .scoped_xml_component("sales", "right", active)
        .script("eventless-scopes", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    const auto topology = app.inspect_scoped_component_topology("sale");
    check(static_cast<bool>(topology));
    check(topology.compiled_event_nodes == std::size_t{0});
    check(topology.compiled_widget_hosts == std::size_t{1});
    check(app.inspect_scoped_component_borrowers("sale"));
    const auto prior = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(prior));
    check_false(prior.event_routes_retired);
    check_false(prior.queue_drained());

    // The Controller alone lacks the compiler ownership proof and must keep
    // rejecting zero handlers. Only the application may attest this scope.
    const auto raw = app.controller()->retire_scoped_event_handlers("sale");
    check_false(static_cast<bool>(raw));
    check(raw.error.code == flexUI::ControllerErrorCode::InvalidArgument);
    auto foreign = app.retire_scoped_event_handlers("unknown");
    check_false(static_cast<bool>(foreign));
    check(foreign.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    check(app.retire_scoped_event_handlers("sale"));
    check(app.retire_scoped_event_handlers("sale"));
    check(app.controller()->is_scoped_event_route_retired("sale"));
    check_false(app.controller()->is_scoped_event_route_retired("sales"));
    auto quiesced = app.quiesce_retired_scoped_service_requests("sale");
    check(static_cast<bool>(quiesced));
    const auto drained = app.inspect_scoped_service_drain("sale");
    check(drained.queue_drained());
    check(drained.pending() == std::size_t{0});
    check_not_null(app.box().get_by_id("sale::action"));
    check_not_null(app.box().get_by_id("sales::action"));

    auto *sibling = app.box().get_by_id("sales::action");
    if (sibling) {
      app.box().set_focus(sibling);
      auto key = flexUI::Event::key_down(flexUI::KeyCode::A);
      check(app.dispatch_event(key));
      check_equal(probe->events.size(), std::size_t{1});
    }
    check(app.reload({root, "", "eventless-scopes", "generation-two"}));
    const auto reloaded = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(reloaded));
    check_false(reloaded.event_routes_retired);
    check_false(reloaded.admission_quiesced);
  }

  it("admits a scriptless anonymous Widget subtree without a phantom controller") {
    const std::string root =
        "<ui name=\"Scriptless\"><main id=\"root\">"
        "<section id=\"host\"/><section id=\"sibling\"/></main></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component(
            "sale", "host",
            "<ui name=\"Passive\"><div><button id=\"action\">Go</button>"
            "<span>Untargeted</span></div></ui>")
        .scoped_xml_component(
            "sales", "sibling",
            "<ui name=\"Other\"><div id=\"label\">Keep</div></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check_null(app.controller());
    const auto before = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(before));
    check(before.event_routes_retired);
    check_false(before.admission_quiesced);
    check_false(before.queue_drained());
    const auto borrowed = app.inspect_scoped_component_borrowers("sale");
    check(static_cast<bool>(borrowed));
    check(borrowed.borrowers.widget_hosts == std::size_t{1});
    auto *sibling = app.box().get_by_id("sales::label");
    check_not_null(sibling);
    if (!sibling) return;
    const auto handle = app.box().handle_for(*sibling);

    check(app.retire_scoped_event_handlers("sale"));
    const auto quiesced = app.quiesce_retired_scoped_service_requests("sale");
    check(static_cast<bool>(quiesced));
    check(app.inspect_scoped_service_drain("sale").queue_drained());
    check_equal(app.box().resolve_handle(handle), sibling);
    check_not_null(app.box().get_by_id("sale::action"));

    // Eventless admission is NOT permission to bypass compiled Widget ownership.
    check_false(app.box().retire_bound_structural_subtree("sale::action"));
    check_equal(app.box().resolve_handle(handle), sibling);
    check(app.reload({root, "", "", "new-generation"}));
    const auto refreshed = app.inspect_scoped_service_drain("sale");
    check(static_cast<bool>(refreshed));
    check(refreshed.event_routes_retired);
    check_false(refreshed.admission_quiesced);
  }
}

spec("FlexUI scoped live Box topology preflight") {
  it("resolves eventless anonymous roots and Widget hosts without fake script events") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    const std::string root =
        "<ui name=\"Topology\"><main id=\"root\">"
        "<section id=\"host\"><div id=\"base\"/></section>"
        "<section id=\"sibling\"/></main></ui>";
    builder.xml_entry(root)
        .scoped_xml_component(
            "eventless", "host",
            "<ui name=\"Panel\"><div>"
            "<button id=\"action\">Send</button><span>Plain</span>"
            "</div></ui>")
        .scoped_xml_component(
            "other", "sibling",
            "<ui name=\"Other\"><div id=\"frame\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check_null(app.controller());
    const auto &owners = app.scoped_component_ownership();
    check(owners.size() == std::size_t{2});
    if (owners.size() != 2) return;
    check(owners[0].nodes[0].qualified_id.empty());
    check(owners[1].scope == "other");
    check(owners[1].nodes[0].qualified_id == "other::frame");
    const auto checked = app.inspect_scoped_component_topology("eventless");
    check(static_cast<bool>(checked));
    const auto borrowers = app.inspect_scoped_component_borrowers("eventless");
    check(static_cast<bool>(borrowers));
    check(borrowers.borrowers.mounted_elements >= std::size_t{3});
    check(borrowers.borrowers.widget_hosts >= std::size_t{1});
    const auto sibling_borrow = app.inspect_scoped_component_borrowers("missing");
    check(sibling_borrow.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    check(checked.resolved_compiled_nodes == std::size_t{3});
    check(checked.compiled_widget_hosts == std::size_t{1});
    check(checked.compiled_event_nodes == std::size_t{0});
    check(checked.compiled_binding_nodes == std::size_t{0});

    const auto *anonymous = app.box().get_by_id("host");
    check_not_null(anonymous);
    if (!anonymous || anonymous->children().size() < 2) return;
    const auto *root_element = dynamic_cast<const flexUI::Element *>(
        anonymous->children()[1]);
    check_not_null(root_element);
    if (!root_element) return;
    check(root_element->id().empty());
    auto *action = app.box().get_by_id("eventless::action");
    check_not_null(action);
    if (!action) return;

    // Runtime borrower inspection includes Box-owned structural nodes and
    // actual Widget-owned parts; it NEVER authorizes a compiled subtree
    // retirement or claims the native plugin worker has joined.
    const auto baseline = app.box().inspect_subtree_borrowers(root_element);
    check(static_cast<bool>(baseline));
    check(baseline.mounted_elements >= std::size_t{3});
    check(baseline.widget_hosts >= std::size_t{1});
    check_false(baseline.focused_descendant);
    check_false(baseline.captured_descendant);
    check_false(baseline.native_event_dispatching);
    check_false(baseline.layout_or_widget_traversal_active);
    const auto invalid_root =
        app.box().inspect_subtree_borrowers(app.box().root());
    check(invalid_root.error == flexUI::BoxSubtreeBorrowErrorCode::InvalidRoot);
    flexUI::BoxSubtreeBorrowErrorCode foreign_error =
        flexUI::BoxSubtreeBorrowErrorCode::None;
    std::thread foreign([&] {
      foreign_error =
          app.box().inspect_subtree_borrowers(root_element).error;
    });
    foreign.join();
    check(foreign_error == flexUI::BoxSubtreeBorrowErrorCode::WrongThread);

    app.box().set_focus(action);
    auto focused = app.box().inspect_subtree_borrowers(root_element);
    check(static_cast<bool>(focused));
    check(focused.focused_descendant);
    check_false(focused.captured_descendant);
    app.box().set_mouse_capture(action);
    auto captured = app.box().inspect_subtree_borrowers(root_element);
    check(captured.focused_descendant);
    check(captured.captured_descendant);
    // A sibling component in the SAME Box must not inherit the focus or
    // capture lease from its adjacent scoped instance.
    auto *sibling_root = app.box().get_by_id("other::frame");
    check_not_null(sibling_root);
    if (!sibling_root) return;
    const auto sibling_borrows =
        app.box().inspect_subtree_borrowers(sibling_root);
    check(static_cast<bool>(sibling_borrows));
    check(sibling_borrows.mounted_elements == std::size_t{1});
    check_false(sibling_borrows.focused_descendant);
    check_false(sibling_borrows.captured_descendant);
    app.box().release_mouse_capture(action);
    app.box().set_focus(nullptr);
    const auto cleared = app.box().inspect_subtree_borrowers(root_element);
    check(static_cast<bool>(cleared));
    check_false(cleared.focused_descendant);
    check_false(cleared.captured_descendant);

    check(app.reload({root, "", "", "refreshed-eventless"}));
    const auto refreshed = app.inspect_scoped_component_topology("eventless");
    check(static_cast<bool>(refreshed));
    check(refreshed.resolved_compiled_nodes == std::size_t{3});
    check(refreshed.compiled_widget_hosts == std::size_t{1});
  }
}

spec("FlexUI desktop application publishes complete XML candidates") {
  it("builds typed XML strict CSS and a mounted script controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 123px; }")
        .script("valid-controller", fake_factory(probe), "desktop-test");

    auto built = builder.build();
    check(static_cast<bool>(built));
    check_not_null(built.application.get());
    if (!built) {
      return;
    }

    auto *save = built.application->box().get_by_id("save");
    check_not_null(save);
    check_not_null(dynamic_cast<flexUI::ButtonWidget *>(save->widget));
    check_not_null(built.application->controller());
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
    check_equal(built.application->program().name(), "Desktop");
    check_single_value(probe->created_sources, "valid-controller");
    check_single_value(probe->created_names, "desktop-test");
    check_single_value(probe->calls, "on_mount");

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check_within(save->style_.width, 123.0F, 0.001F);
  }

  it("instantiates widget text from natural XML content") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Natural\"><button id=\"save\">Save</button></ui>");

    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *save = built.application->box().get_by_id("save");
    auto *button = save ? dynamic_cast<flexUI::ButtonWidget *>(save->widget) : nullptr;
    check_not_null(save);
    check_not_null(button);
    if (save && button) {
      check_equal(save->text(), std::string("Save"));
      check_equal(button->text(), std::string("Save"));
    }
  }

  it("publishes anonymous structural nodes without public handles") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(R"(
      <ui name="Anonymous">
        <main class="page">
          <div class="panel">
            <label id="title" text="Settings"/>
          </div>
        </main>
      </ui>
    )");

    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *root = built.application->box().root();
    auto *title = built.application->box().get_by_id("title");
    check_not_null(root);
    check_not_null(title);
    check(root->id().empty());
    check_false(static_cast<bool>(built.application->box().handle_for(*root)));
    check_null(built.application->box().get_by_id(""));
    check(static_cast<bool>(built.application->box().handle_for(*title)));
  }

  it("rejects strict CSS diagnostics before publishing an application") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>")
        .stylesheet("#root { widht: 90px; width: 12wat; }");

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::CssLoadFailed);
    check_false(built.error.css_diagnostics.empty());
  }


  it("rejects invalid UTF-8 before publishing an application") {
    std::string invalid =
        "<ui name=\"Bad\">\n"
        "  <div id=\"root\" text=\"";
    invalid.push_back(static_cast<char>(0xFF));
    invalid += "\"/>\n</ui>";

    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(invalid);

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(built.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidUtf8);
    check_equal(built.error.ui_error.line, 2);
    check_equal(built.error.ui_error.column, 24);
  }

  it("keeps the active candidate unchanged when reload XML is invalid UTF-8") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Stable\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *active_box = &built.application->box();
    const std::string active_ui = built.application->sources().ui;

    std::string invalid =
        "<ui name=\"Replacement\">\n"
        "  <div id=\"replacement\" text=\"";
    invalid.push_back(static_cast<char>(0xFF));
    invalid += "\"/>\n</ui>";

    const auto failed =
        built.application->reload({invalid, "", "", "reload-invalid-utf8"});
    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(failed.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(failed.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidUtf8);
    check_equal(&built.application->box(), active_box);
    check_not_null(built.application->box().get_by_id("root"));
    check_null(built.application->box().get_by_id("replacement"));
    check_equal(built.application->sources().ui, active_ui);
  }

  it("rejects missing controller exports before publication") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("missing-handler", fake_factory(probe));

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ControllerLoadFailed);
    check(built.error.controller_error.code == flexUI::ControllerErrorCode::MissingHandlerExport);
    check_equal(built.error.controller_error.element_id, "save");
    check_equal(built.error.controller_error.handler, "save_document");
  }

  it("rejects a failing mount without publishing candidate state") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("mount-failure", fake_factory(probe));

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ControllerMountFailed);
    check(built.error.controller_error.stage == flexUI::ControllerStage::Mount);
  }

  it("requires a script factory when XML declares event handlers") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry());

    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ScriptRequired);
  }

  it("keeps the active application unchanged when reload fails") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 40px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *active_box = &built.application->box();
    const auto failed =
        built.application->reload({xml_entry("replacement"), "#replacement { width: 80px; }",
                                   "missing-handler", "reload-test"});

    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::DesktopApplicationErrorCode::ControllerLoadFailed);
    check_equal(&built.application->box(), active_box);
    check_not_null(built.application->box().get_by_id("save"));
    check_null(built.application->box().get_by_id("replacement"));
    check_equal(built.application->sources().script, "valid-controller");
  }

  it("publishes UI CSS and controller together after successful reload") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *previous_box = &built.application->box();
    const auto reloaded =
        built.application->reload({xml_entry("replacement"), "#replacement { width: 88px; }",
                                   "replacement-controller", "replacement-module"});

    check(static_cast<bool>(reloaded));
    check_not_equal(&built.application->box(), previous_box);
    check_null(built.application->box().get_by_id("save"));
    auto *replacement = built.application->box().get_by_id("replacement");
    check_not_null(replacement);
    check_not_null(dynamic_cast<flexUI::ButtonWidget *>(replacement->widget));
    check_equal(built.application->sources().script, "replacement-controller");
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("publishes service commands after UI mutation commit and resolves completions") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_valid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    auto endpoint = configure_storage_service(builder);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    auto *save = built.application->box().get_by_id("save");
    check(dispatch_click(*built.application));
    check_equal(save->text(), std::string("Saved"));

    auto request = built.application->try_receive_service_request();
    check(request);
    check_equal(request.request->script_request_id, std::uint64_t{71});
    check_equal(request.request->capability, std::string("storage/1"));
    check_equal(request.request->operation, std::string("write"));
    check_equal(request.request->payload, std::string("document"));
    check_equal(request.request->token.generation, built.application->generation());
    auto resolved_endpoint =
        built.application->resolve_service_request(*request.request);
    check(resolved_endpoint);
    check(resolved_endpoint.endpoint == endpoint);

    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.payload = "stored";
    check(built.application->completion_mailbox().try_post(completion));

    auto resolved = built.application->try_receive_service_completion();
    check(resolved);
    check_equal(resolved.completion->script_request_id, std::uint64_t{71});
    check_equal(resolved.completion->payload, std::string("stored"));
    check(built.application->try_receive_service_completion().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    check_equal(built.application->service_statistics().completed, std::uint64_t{1});
  }

  it("forwards a borrowed completion wakeup without consuming the record") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    std::atomic<std::uint64_t> wakeups{0};
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe))
        .completion_wakeup({count_completion_wakeup, &wakeups});
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto request = built.application->try_receive_service_request();
    check(request);

    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.payload = "worker-result";
    flexUI::ApplicationCompletionPostResult posted;
    std::thread worker([&] {
      posted = built.application->completion_mailbox().try_post(completion);
    });
    worker.join();

    check(posted);
    check_equal(wakeups.load(std::memory_order_acquire), std::uint64_t{1});
    auto resolved = built.application->try_receive_service_completion();
    check(resolved);
    check_equal(resolved.completion->payload, std::string("worker-result"));
  }

  it("accepts completion wakeup configuration on a moved-from builder") {
    std::atomic<std::uint64_t> wakeups{0};
    flexUI::DesktopApplicationBuilder source(nullptr);
    auto destination = std::move(source);

    auto &configured = source.completion_wakeup(
        {count_completion_wakeup, &wakeups});
    check_equal(&configured, &source);
    auto rejected = source.build();
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    check_equal(wakeups.load(std::memory_order_acquire), std::uint64_t{0});
    static_cast<void>(destination);
  }

  it("dispatches one service completion through the optional controller export") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-completion-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto request = built.application->try_receive_service_request();
    check(request);
    probe->emit_valid_mutation = true;

    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.payload = "stored";
    check(built.application->completion_mailbox().try_post(completion));
    auto dispatched = built.application->try_dispatch_service_completion();

    check(static_cast<bool>(dispatched));
    check(dispatched.status == flexUI::ApplicationServicePollStatus::Ready);
    check_true(dispatched.dispatched);
    check(dispatched.completion.has_value());
    check_equal(probe->service_completions.size(), std::size_t{1});
    if (!probe->service_completions.empty()) {
      check_equal(probe->service_completions.front().script_request_id, std::uint64_t{71});
      check_equal(probe->service_completions.front().payload, std::string("stored"));
    }
    check_equal(built.application->box().get_by_id("save")->text(),
                std::string("Completion applied"));
    check(built.application->try_dispatch_service_completion().status ==
          flexUI::ApplicationServicePollStatus::Empty);
  }

  it("delivers failed service status as data without fallback") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-completion-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto request = built.application->try_receive_service_request();
    check(request);
    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.status = flexUI::ApplicationCompletionStatus::Failed;
    completion.error_code = "write-failed";
    completion.error_message = "disk full";
    check(built.application->completion_mailbox().try_post(completion));

    auto dispatched = built.application->try_dispatch_service_completion();
    check(static_cast<bool>(dispatched));
    check_true(dispatched.dispatched);
    check_equal(probe->service_completions.size(), std::size_t{1});
    if (!probe->service_completions.empty()) {
      const auto &received = probe->service_completions.front();
      check(received.status == flexUI::ApplicationCompletionStatus::Failed);
      check_equal(received.error_code, std::string("write-failed"));
      check_equal(received.error_message, std::string("disk full"));
    }
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("returns an owning unhandled completion when the optional export is absent") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto request = built.application->try_receive_service_request();
    check(request);
    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    completion.payload = "unhandled";
    check(built.application->completion_mailbox().try_post(completion));

    auto dispatched = built.application->try_dispatch_service_completion();
    check(static_cast<bool>(dispatched));
    check_false(dispatched.dispatched);
    check(dispatched.completion.has_value());
    check_equal(dispatched.completion->payload, std::string("unhandled"));
    check(probe->service_completions.empty());

    check(dispatch_click(*built.application));
    auto second_request = built.application->try_receive_service_request();
    check(second_request);
    check(built.application->controller()->unmount());
    flexUI::ApplicationCompletion second_completion;
    second_completion.token = second_request.request->token;
    second_completion.payload = "after-unmount";
    check(built.application->completion_mailbox().try_post(second_completion));
    auto after_unmount = built.application->try_dispatch_service_completion();
    check(static_cast<bool>(after_unmount));
    check_false(after_unmount.dispatched);
    check_equal(after_unmount.completion->payload, std::string("after-unmount"));
  }

  it("faults the application controller when the completion callback fails") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-completion-failure", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto request = built.application->try_receive_service_request();
    check(request);
    flexUI::ApplicationCompletion completion;
    completion.token = request.request->token;
    check(built.application->completion_mailbox().try_post(completion));

    auto dispatched = built.application->try_dispatch_service_completion();
    check_false(static_cast<bool>(dispatched));
    check(dispatched.status == flexUI::ApplicationServicePollStatus::Ready);
    check_true(dispatched.dispatched);
    check(dispatched.error.code ==
          flexUI::DesktopApplicationErrorCode::ControllerServiceCompletionFailed);
    check(dispatched.error.stage ==
          flexUI::DesktopApplicationStage::ControllerServiceCompletion);
    check(dispatched.error.controller_error.code ==
          flexUI::ControllerErrorCode::ModuleCallFailed);
    check(built.application->controller()->state() == flexUI::ControllerState::Faulted);
  }

  it("discards reserved service commands when UI mutation preparation fails") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_invalid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("invalid-mutation", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.controller_error.code == flexUI::ControllerErrorCode::MutationFailed);
    check(built.application->try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    const auto stats = built.application->service_statistics();
    check_equal(stats.current_pending, std::size_t{0});
    check_equal(stats.discarded, std::uint64_t{1});
    check_equal(stats.published, std::uint64_t{0});
  }

  it("rejects service saturation before applying same-callback UI mutations") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationLimits limits;
    limits.service_requests.capacity = 1;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe))
        .limits(limits);
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto *save = built.application->box().get_by_id("save");
    save->set_text("Before");
    probe->service_request_id = 72;
    probe->emit_valid_mutation = true;

    const auto saturated = dispatch_click(*built.application);
    check_false(static_cast<bool>(saturated));
    check(saturated.error.controller_error.code == flexUI::ControllerErrorCode::CommandFailed);
    check(saturated.error.controller_error.command_error.code ==
          flexUI::ApplicationCommandErrorCode::QueueFull);
    check_equal(save->text(), std::string("Before"));
    check_equal(built.application->service_statistics().current_pending, std::size_t{1});
  }

  it("rejects unauthorized service commands before same-callback UI mutation") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    probe->emit_valid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("unauthorized-service", fake_factory(probe));
    auto built = builder.build();
    check(built);
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    auto *save = built.application->box().get_by_id("save");
    save->set_text("Before");
    auto dispatched = dispatch_click(*built.application);
    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.controller_error.code ==
          flexUI::ControllerErrorCode::CommandFailed);
    check(dispatched.error.controller_error.command_error.code ==
          flexUI::ApplicationCommandErrorCode::UnknownCapability);
    check_equal(save->text(), std::string("Before"));
    check_equal(built.application->service_statistics().current_pending,
                std::size_t{0});
  }

  it("rejects a missing required service while building the application") {
    flexUI::ApplicationServiceRegistryBuilder registry_builder;
    auto registry = registry_builder.build();
    check(registry);
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).services(
        std::move(registry.registry),
        {{"document.storage/1"}, {"document.storage/1"}});

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::DesktopApplicationErrorCode::ServiceRegistryFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::ServiceRegistry);
    check(built.error.registry_error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::MissingRequiredCapability);
  }

  it("cancels pending service identities when reload publishes a new generation") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    auto endpoint = configure_storage_service(builder);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto old_request = built.application->try_receive_service_request();
    check(old_request);
    auto old_endpoint =
        built.application->resolve_service_request(*old_request.request);
    check(old_endpoint);
    check(old_endpoint.endpoint == endpoint);
    const auto old_generation = built.application->generation();

    check(built.application->reload({xml_entry("replacement"),
                                     "#replacement { width: 120px; height: 36px; }",
                                     "replacement-controller", "replacement-module"}));
    check_equal(built.application->generation(), old_generation + 1);
    check_equal(built.application->service_statistics().cancelled, std::uint64_t{1});

    auto stale_resolution =
        built.application->resolve_service_request(*old_request.request);
    check_false(static_cast<bool>(stale_resolution));
    check(stale_resolution.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidRequest);

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    auto current_request = built.application->try_receive_service_request();
    check(current_request);
    auto current_endpoint =
        built.application->resolve_service_request(*current_request.request);
    check(current_endpoint);
    check(current_endpoint.endpoint == endpoint);

    flexUI::ApplicationCompletion stale;
    stale.token = old_request.request->token;
    auto rejected = built.application->completion_mailbox().try_post(stale);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::StaleGeneration);
  }

  it("routes a synthesized click through target and ancestor handlers after native bubbling") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder
        .xml_entry("<ui name=\"Bubble\" xmlns:on=\"urn:flexui:event\">"
                   "<main id=\"parent\" on:click=\"parent_handler\">"
                   "<button id=\"save\" text=\"Save\" on:click=\"save_document\"/>"
                   "</main></ui>")
        .stylesheet("#parent { width: 200px; height: 100px; }"
                    "#save { width: 120px; height: 36px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    probe->events.clear();
    probe->sequence.clear();
    built.application->box().set_event_callback(
        [probe](flexUI::Element &element, const flexUI::Event &event) {
          if (event.type == flexUI::EventType::MouseUp) {
            probe->sequence.push_back("native:" + element.id());
          }
        });

    const auto dispatched = dispatch_click(*built.application);
    check(static_cast<bool>(dispatched));
    check_equal(probe->calls.size(), std::size_t{2});
    check_equal(probe->events.size(), std::size_t{2});
    check_equal(probe->sequence.size(), std::size_t{4});
    if (probe->calls.size() == 2 && probe->events.size() == 2 && probe->sequence.size() == 4) {
      check_equal(probe->calls[0], "save_document");
      check_equal(probe->calls[1], "parent_handler");
      check_equal(probe->events[0].target.id, "save");
      check_equal(probe->events[0].current_target.id, "save");
      check_equal(probe->events[1].target.id, "save");
      check_equal(probe->events[1].current_target.id, "parent");
      check_equal(probe->sequence[0], "native:save");
      check_equal(probe->sequence[1], "native:parent");
      check_equal(probe->sequence[2], "script:save_document");
      check_equal(probe->sequence[3], "script:parent_handler");
    }
  }

  it("does not notify script when a widget consumes the routed event") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder
        .xml_entry("<ui name=\"Consumed\" xmlns:on=\"urn:flexui:event\">"
                   "<slider id=\"volume\" on:mouse_down=\"consumed_handler\"/>"
                   "</ui>")
        .stylesheet("#volume { width: 160px; height: 32px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    auto down = flexUI::Event::mouse_down(10.0F, 10.0F);
    const auto dispatched = built.application->dispatch_event(down);

    check(static_cast<bool>(dispatched));
    check_true(down.handled);
    check_false(down.propagate);
    check_true(probe->calls.empty());
  }

  it("returns a structured dispatch error and faults the controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("event-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);

    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code == flexUI::DesktopApplicationErrorCode::ControllerDispatchFailed);
    check(dispatched.error.stage == flexUI::DesktopApplicationStage::ControllerEvent);
    check(dispatched.error.controller_error.code == flexUI::ControllerErrorCode::ModuleCallFailed);
    check(built.application->controller()->state() == flexUI::ControllerState::Faulted);
  }

  it("reports native callback exceptions without faulting the script controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    probe->calls.clear();
    built.application->box().set_event_callback([](flexUI::Element &, const flexUI::Event &event) {
      if (event.type == flexUI::EventType::MouseUp) {
        throw std::runtime_error("injected native callback failure");
      }
    });

    const auto dispatched = dispatch_click(*built.application);

    check_false(static_cast<bool>(dispatched));
    check(dispatched.error.code == flexUI::DesktopApplicationErrorCode::EventDispatchFailed);
    check(dispatched.error.stage == flexUI::DesktopApplicationStage::ControllerEvent);
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);
    check(probe->calls.empty());
  }

  it("preserves native Box callbacks when no script controller is configured") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Native\"><button id=\"save\" text=\"Save\"/></ui>")
        .stylesheet("#save { width: 120px; height: 36px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    bool clicked = false;
    built.application->box().get_by_id("save")->on_click([&] { clicked = true; });
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    const auto dispatched = dispatch_click(*built.application);

    check(static_cast<bool>(dispatched));
    check_true(clicked);
  }

  it("gates host text and composition input through the application facade") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Input\"><input id=\"editor\"/></ui>")
        .stylesheet("#editor { width: 160px; height: 32px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *editor = built.application->box().get_by_id("editor");
    auto *input = dynamic_cast<flexUI::InputWidget *>(editor->widget);
    check_not_null(input);

    const auto ignored =
        flexUI::host::dispatch_text_input_if_focused(*built.application, "ignored");
    check(static_cast<bool>(ignored));
    check_false(ignored.dispatched);
    check(input->text().empty());

    built.application->box().set_focus(editor);
    const auto text = flexUI::host::dispatch_text_input_if_focused(*built.application, "x");
    const auto composition_start =
        flexUI::host::dispatch_composition_start_if_focused(*built.application);
    const auto composition_update =
        flexUI::host::dispatch_composition_update_if_focused(*built.application, "zh");
    const auto composition_end =
        flexUI::host::dispatch_composition_end_if_focused(*built.application);

    check(static_cast<bool>(text));
    check_true(text.dispatched);
    check_equal(input->text(), std::string("x"));
    check(static_cast<bool>(composition_start));
    check_true(composition_start.dispatched);
    check(static_cast<bool>(composition_update));
    check_true(composition_update.dispatched);
    check(static_cast<bool>(composition_end));
    check_true(composition_end.dispatched);
  }

  it("rejects host text input on a non-owner thread before reading Box state") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Input\"><input id=\"editor\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::host::HostApplicationDispatchResult dispatch_result;
    std::thread worker([&] {
      dispatch_result =
          flexUI::host::dispatch_text_input_if_focused(*built.application, "rejected");
    });
    worker.join();

    check_false(static_cast<bool>(dispatch_result));
    check_false(dispatch_result.dispatched);
    check(dispatch_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
  }

  it("rejects reload from a non-owner thread without touching active state") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"active\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto *active_box = &built.application->box();
    flexUI::DesktopApplicationResult reload_result;
    std::thread worker([&] {
      reload_result = built.application->reload(
          {"<ui name=\"Replacement\"><div id=\"replacement\"/></ui>", "", "", ""});
    });
    worker.join();

    check_false(static_cast<bool>(reload_result));
    check(reload_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_equal(&built.application->box(), active_box);
    check_not_null(built.application->box().get_by_id("active"));
    check_null(built.application->box().get_by_id("replacement"));
  }

  it("rejects event dispatch from a non-owner thread before routing native input") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><button id=\"save\" text=\"Save\"/></ui>")
        .stylesheet("#save { width: 120px; height: 36px; }");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    bool clicked = false;
    built.application->box().get_by_id("save")->on_click([&] { clicked = true; });
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    flexUI::DesktopApplicationResult dispatch_result;
    std::thread worker([&] {
      auto down = flexUI::Event::mouse_down(10.0F, 10.0F);
      dispatch_result = built.application->dispatch_event(down);
    });
    worker.join();

    check_false(static_cast<bool>(dispatch_result));
    check(dispatch_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_false(clicked);
  }

  it("keeps legacy Flex loading behind the compatibility entry") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.legacy_flex_entry_compatibility("ui Legacy { button old { text: \"Old\" } }");

    auto built = builder.build();
    check(static_cast<bool>(built));
    check_not_null(built.application.get());
    if (!built) {
      return;
    }
    check_true(built.application->uses_legacy_flex_compatibility());
    check_not_null(
        dynamic_cast<flexUI::ButtonWidget *>(built.application->box().get_by_id("old")->widget));
  }
}

spec("FlexUI desktop application owns close and shutdown state") {
  it("dispatches validated owner-thread frames and reports controller failures") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto frame = built.application->frame(0.016);
    check(static_cast<bool>(frame));
    check_equal(probe->frame_deltas.size(), std::size_t{1});
    if (!probe->frame_deltas.empty()) {
      check_within(probe->frame_deltas.front(), 0.016, 0.000001);
    }

    const auto invalid = built.application->frame(std::numeric_limits<double>::quiet_NaN());
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == flexUI::DesktopApplicationErrorCode::InvalidArgument);
    check(invalid.error.stage == flexUI::DesktopApplicationStage::ControllerFrame);

    check(built.application->request_close());
    const auto closed = built.application->frame(0.016);
    check_false(static_cast<bool>(closed));
    check(closed.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
  }

  it("nests on_frame module errors and faults the controller") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("frame-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto frame = built.application->frame(0.016);
    check_false(static_cast<bool>(frame));
    check(frame.error.code == flexUI::DesktopApplicationErrorCode::ControllerFrameFailed);
    check(frame.error.controller_error.stage == flexUI::ControllerStage::Frame);
    check(built.application->controller()->state() == flexUI::ControllerState::Faulted);
  }

  it("rejects frames from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::DesktopApplicationResult frame_result;
    std::thread worker([&] { frame_result = built.application->frame(0.016); });
    worker.join();

    check_false(static_cast<bool>(frame_result));
    check(frame_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(frame_result.error.stage == flexUI::DesktopApplicationStage::ControllerFrame);
  }

  it("stops new work after close and unmounts exactly once during shutdown") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
    const auto premature_shutdown = built.application->shutdown();
    check_false(static_cast<bool>(premature_shutdown));
    check(premature_shutdown.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check(premature_shutdown.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
    check(built.application->request_close());
    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);

    auto event = flexUI::Event::mouse_move(1.0F, 2.0F);
    const auto dispatch = built.application->dispatch_event(event);
    const auto reload =
        built.application->reload({"<ui name=\"Closed\"><div id=\"closed\"/></ui>", "", "", ""});
    check_false(static_cast<bool>(dispatch));
    check(dispatch.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);
    check_false(static_cast<bool>(reload));
    check(reload.error.code == flexUI::DesktopApplicationErrorCode::InvalidState);

    check(built.application->shutdown());
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
    check_not_null(built.application->box().get_by_id("save"));
    check(built.application->controller()->state() == flexUI::ControllerState::Empty);
    check(built.application->shutdown());
    check_equal(probe->calls.size(), std::size_t{2});
    if (probe->calls.size() == 2) {
      check_equal(probe->calls[0], "on_mount");
      check_equal(probe->calls[1], "on_unmount");
    }
  }

  it("closes the completion mailbox before publishing CloseRequested") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto generation = built.application->generation();
    flexUI::ApplicationCompletion queued;
    queued.token = {1, generation};
    queued.payload = "finished";
    check(built.application->completion_mailbox().try_post(queued));

    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check(built.application->completion_mailbox().state() ==
          flexUI::ApplicationCompletionMailboxState::Closed);
    check_equal(built.application->completion_mailbox().statistics().cancelled, std::uint64_t{1});

    flexUI::ApplicationCompletion late;
    late.token = {2, generation};
    auto rejected = built.application->completion_mailbox().try_post(late);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::Closed);
    const auto dispatched = built.application->try_dispatch_service_completion();
    check(dispatched.status == flexUI::ApplicationServicePollStatus::Closed);
    check_false(dispatched.dispatched);
    check_false(dispatched.completion.has_value());
  }

  it("advances completion generation atomically with application reload") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Before\"><div id=\"before\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    const auto old_generation = built.application->generation();
    flexUI::ApplicationCompletion queued;
    queued.token = {1, old_generation};
    check(built.application->completion_mailbox().try_post(queued));

    check(built.application->reload({"<ui name=\"After\"><div id=\"after\"/></ui>", "", "", ""}));
    const auto current_generation = built.application->generation();
    check_equal(current_generation, old_generation + 1);
    check_not_null(built.application->box().get_by_id("after"));
    check_equal(built.application->completion_mailbox().statistics().cancelled, std::uint64_t{1});
    const auto old_dispatch = built.application->try_dispatch_service_completion();
    check(old_dispatch.status == flexUI::ApplicationServicePollStatus::Empty);
    check_false(old_dispatch.dispatched);

    flexUI::ApplicationCompletion stale;
    stale.token = {2, old_generation};
    auto rejected = built.application->completion_mailbox().try_post(stale);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationCompletionErrorCode::StaleGeneration);

    flexUI::ApplicationCompletion current;
    current.token = {3, current_generation};
    check(built.application->completion_mailbox().try_post(current));
    auto received = built.application->completion_mailbox().try_receive();
    check(received.completion.has_value());
    check_equal(received.completion->token.id, std::uint64_t{3});
  }

  it("fails application publication when completion limits are invalid") {
    flexUI::DesktopApplicationLimits limits;
    limits.completion.capacity = 3;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>").limits(limits);

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::CompletionMailboxFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::CompletionMailbox);
    check(built.error.completion_error.code ==
          flexUI::ApplicationCompletionErrorCode::InvalidCapacity);
  }

  it("fails application publication when service request limits are invalid") {
    flexUI::DesktopApplicationLimits limits;
    limits.service_requests.capacity = 0;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>").limits(limits);

    auto built = builder.build();
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::DesktopApplicationErrorCode::ServiceRequestFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::ServiceRequests);
    check(built.error.service_error.code == flexUI::ApplicationServiceErrorCode::InvalidCapacity);
  }

  it("cancels pending service requests before publishing CloseRequested") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("service-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();
    check(dispatch_click(*built.application));
    check_equal(built.application->service_statistics().current_pending, std::size_t{1});

    check(built.application->request_close());
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check_equal(built.application->service_statistics().cancelled, std::uint64_t{1});
    check(built.application->try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Closed);
    check(built.application->try_receive_service_completion().status ==
          flexUI::ApplicationServicePollStatus::Closed);
  }

  it("rejects service polling from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::ApplicationServiceRequestResult request;
    flexUI::ApplicationServiceCompletionResult completion;
    flexUI::DesktopApplicationCompletionDispatchResult dispatch;
    std::thread worker([&] {
      request = built.application->try_receive_service_request();
      completion = built.application->try_receive_service_completion();
      dispatch = built.application->try_dispatch_service_completion();
    });
    worker.join();

    check(request.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(completion.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(dispatch.error.code == flexUI::DesktopApplicationErrorCode::ServiceRequestFailed);
    check(dispatch.error.stage == flexUI::DesktopApplicationStage::ServiceRequests);
    check(dispatch.error.service_error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
  }

  it("retains shutdown state while reporting an on_unmount failure") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry()).script("unmount-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.application->request_close());
    const auto shutdown = built.application->shutdown();

    check_false(static_cast<bool>(shutdown));
    check(shutdown.error.code == flexUI::DesktopApplicationErrorCode::ControllerUnmountFailed);
    check(shutdown.error.controller_error.stage == flexUI::ControllerStage::Unmount);
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
    check(built.application->controller()->state() == flexUI::ControllerState::Empty);
    check_not_null(built.application->box().get_by_id("save"));
  }

  it("rejects reentrant shutdown without losing the later cleanup opportunity") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->shutdown_during_event = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(xml_entry())
        .stylesheet("#save { width: 120px; height: 36px; }")
        .script("shutdown-during-event", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    probe->application = built.application.get();
    built.application->box().set_viewport(320.0F, 200.0F);
    built.application->box().update();

    check(dispatch_click(*built.application));
    check_false(static_cast<bool>(probe->reentrant_shutdown));
    check(probe->reentrant_shutdown.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidState);
    check(built.application->state() == flexUI::DesktopApplicationState::CloseRequested);
    check(built.application->controller()->state() == flexUI::ControllerState::Mounted);

    probe->shutdown_during_event = false;
    check(built.application->request_close());
    check(built.application->shutdown());
    check(built.application->state() == flexUI::DesktopApplicationState::Shutdown);
  }

  it("rejects lifecycle transitions from a non-owner thread") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Static\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::DesktopApplicationResult close_result;
    flexUI::DesktopApplicationResult shutdown_result;
    std::thread worker([&] {
      close_result = built.application->request_close();
      shutdown_result = built.application->shutdown();
    });
    worker.join();

    check_false(static_cast<bool>(close_result));
    check(close_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(close_result.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check_false(static_cast<bool>(shutdown_result));
    check(shutdown_result.error.code == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(shutdown_result.error.stage == flexUI::DesktopApplicationStage::Lifecycle);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
  }
}

spec("FlexUI desktop content preflight runs before stylesheet admission") {
  it("reports an unknown tag before an invalid stylesheet") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Schema\">\n  <unknown id=\"root\"/>\n</ui>")
           .stylesheet("#root { widht: 90px; }");
    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(built.error.ui_error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_equal(built.error.ui_error.line, 2);
    check_equal(built.error.ui_error.column, 4);
    check_true(built.error.css_diagnostics.empty());
  }

  it("reports forbidden leaf children before an invalid stylesheet") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Schema\"><input id=\"root\"><label id=\"child\"/></input></ui>")
           .stylesheet("#root { widht: 90px; }");
    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(built.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidNode);
    check_true(built.error.css_diagnostics.empty());
  }
}


spec("FlexUI schema rejection preserves the active application") {
  it("rejects an invalid later sibling before invoking factories during reload") {
    int calls = 0;
    auto registry = flexUI::WidgetRegistry::builtins();
    check_false(static_cast<bool>(registry.register_widget("probe", [&](const flexUI::UiNodeDefinition &) {
      ++calls;
      return std::make_unique<flexUI::ButtonWidget>();
    }, flexUI::UiContentModel::Empty)));
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.widget_registry(std::move(registry))
           .xml_entry("<ui name=\"Stable\"><div id=\"root\"/></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto *active_box = &built.application->box();
    auto *active_root = active_box->root();
    const auto active_source = built.application->sources().ui;
    const auto failed = built.application->reload({
      "<ui name=\"Replacement\"><div id=\"replacement\"><probe id=\"first\"/><input id=\"last\" text=\"forbidden\"/></div></ui>",
      "#replacement { widht: 90px; }", "", "schema-reload"});
    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(failed.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(failed.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidProperty);
    check_true(failed.error.css_diagnostics.empty());
    check_equal(calls, 0);
    check_equal(&built.application->box(), active_box);
    check_equal(active_box->root(), active_root);
    check_null(active_box->get_by_id("replacement"));
    check_null(active_box->get_by_id("first"));
    check_equal(built.application->sources().ui, active_source);
    check(built.application->state() == flexUI::DesktopApplicationState::Ready);
  }
}


spec("FlexUI anonymous application reload keeps named state transactional") {
  it("keeps an anonymous root and a named handle when a replacement lacks a binding id") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Stable\"><main><label id=\"title\" text=\"Stable\"/></main></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto *active_box = &built.application->box();
    auto *active_root = active_box->root();
    auto *title = active_box->get_by_id("title");
    const auto handle = active_box->handle_for(*title);
    const auto source = built.application->sources().ui;
    const auto failed = built.application->reload({
      "<ui name=\"Replacement\" xmlns:bind=\"urn:flexui:binding\"><main><label bind:text=\"$title\"/></main></ui>", "", "", "missing-binding-id"});
    check_false(static_cast<bool>(failed));
    check(failed.error.ui_error.code == flexUI::UiDocumentErrorCode::RequiredElementId);
    check(failed.error.ui_error.line > 0);
    check_equal(&built.application->box(), active_box);
    check_equal(active_box->root(), active_root);
    check_equal(active_box->resolve_handle(handle), title);
    check_null(active_box->get_by_id(""));
    check_equal(built.application->sources().ui, source);
  }
}


spec("FlexUI natural text obeys content models during reload") {
  it("preserves the active application when natural text is forbidden on a leaf") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Text\"><button id=\"save\">Save &amp; close</button></ui>");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto *box = &built.application->box();
    auto *save = box->get_by_id("save");
    const auto source = built.application->sources().ui;
    const auto failed = built.application->reload({"<ui name=\"Bad\"><input>forbidden</input></ui>", "", "", "natural-text-reload"});
    check_false(static_cast<bool>(failed));
    check(failed.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidProperty);
    check_equal(&built.application->box(), box);
    check_equal(box->get_by_id("save"), save);
    check_equal(dynamic_cast<flexUI::ButtonWidget *>(save->widget)->text(), "Save & close");
    check_equal(built.application->sources().ui, source);
  }
}


spec("Desktop XML component full-generation recomposition") {
  it("adds, replaces and removes component scopes through one candidate owner") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_valid_mutation = true;
    const std::string root =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel = R"xml(
      <ui name="Panel" xmlns:on="urn:flexui:event"
          xmlns:bind="urn:flexui:binding">
        <div id="panel">
          <div id="action" on:click="save_document">Original</div>
          <label id="caption" bind:text="$caption">Initial</label>
        </div>
      </ui>
    )xml";
    const std::string replacement = R"xml(
      <ui name="Panel" xmlns:on="urn:flexui:event"
          xmlns:bind="urn:flexui:binding">
        <div id="panel">
          <div id="action" on:click="save_document">New action</div>
          <label id="caption" bind:text="$caption">Updated template</label>
        </div>
      </ui>
    )xml";

    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sales", "left", panel)
        .scoped_binding_alias("sales", "caption", "sales_caption")
        .binding_input_string("sales_caption", "Revenue")
        .binding_input_string("costs_caption", "Expenses")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    const auto first_generation = app.generation();
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Revenue");
    check_null(app.box().get_by_id("costs::caption"));

    // Staging a second complete candidate admits a new scope and its event
    // handler without registering handlers against the current controller.
    const auto added = app.recompose_scoped_xml_components(
        {{"sales", "left", panel}, {"costs", "right", panel}},
        {{"sales", "caption", "sales_caption"},
         {"costs", "caption", "costs_caption"}});
    check(static_cast<bool>(added));
    if (!added) return;
    check_equal(app.generation(), first_generation + 1);
    check_equal(app.program().bindings().size(), std::size_t{2});
    check_equal(app.program().event_bindings().size(), std::size_t{2});
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Revenue");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");
    auto *costs_action = app.box().get_by_id("costs::action");
    check_not_null(costs_action);
    check_not_null(app.program().find_event_binding(
        "costs::action", flexUI::UiEventKind::Click));
    if (!costs_action) return;
    flexUI::ScriptEventSnapshot click;
    click.event = flexUI::UiEventKind::Click;
    click.target = app.box().handle_for(*costs_action);
    click.current_target = click.target;
    check(app.controller()->dispatch(click));
    check_equal(costs_action->text(), "Saved");
    check_equal(probe->events.back().current_target.id, "costs::action");

    // Standard reload must now use the newly committed component set, and
    // reconstruct typed input values from immutable application seed inputs.
    app.box().bindings().inputs().set_string("costs_caption", "Running");
    app.box().update();
    check_equal(app.box().get_by_id("costs::caption")->text(), "Running");
    check(app.reload({root, "", "valid-controller", "after-recompose"}));
    app.box().update();
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");
    check_not_null(app.box().get_by_id("sales::action"));

    // Remove one scope and replace the remaining template atomically. The
    // removed element, binding and compiled event must all disappear together.
    const auto before_remove = app.generation();
    const auto removed = app.recompose_scoped_xml_components(
        {{"costs", "right", replacement}},
        {{"costs", "caption", "costs_caption"}});
    check(static_cast<bool>(removed));
    if (!removed) return;
    check_equal(app.generation(), before_remove + 1);
    app.box().update();
    check_null(app.box().get_by_id("sales::action"));
    check_null(app.box().get_by_id("sales::caption"));
    check_null(app.program().find_event_binding(
        "sales::action", flexUI::UiEventKind::Click));
    check_equal(app.program().bindings().size(), std::size_t{1});
    check_equal(app.program().event_bindings().size(), std::size_t{1});
    check_equal(app.box().get_by_id("costs::action")->text(), "New action");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");

    // The empty desired set is not an in-place detach: it publishes a new
    // complete root application generation with no component handlers.
    const auto emptied = app.recompose_scoped_xml_components({}, {});
    check(static_cast<bool>(emptied));
    if (!emptied) return;
    check_null(app.box().get_by_id("costs::action"));
    check_null(app.box().get_by_id("costs::caption"));
    check_equal(app.program().event_bindings().size(), std::size_t{0});
    check_equal(app.program().bindings().size(), std::size_t{0});
    check_not_null(app.box().get_by_id("left"));
    check_not_null(app.box().get_by_id("right"));
    check(app.reload({root, "", "valid-controller", "empty-reload"}));
    check_null(app.box().get_by_id("costs::action"));
  }

  it("fails closed on invalid sources, undeclared aliases and wrong owner") {
    auto probe = std::make_shared<ModuleProbe>();
    const std::string root =
        "<ui name=\"App\"><main id=\"root\"><section id=\"host\"/></main></ui>";
    const std::string panel =
        "<ui name=\"Panel\" xmlns:bind=\"urn:flexui:binding\">"
        "<label id=\"caption\" bind:text=\"$caption\">Initial</label></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("only", "host", panel)
        .scoped_binding_alias("only", "caption", "seed")
        .binding_input_string("seed", "Original");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto *original_box = &app.box();
    auto *original_program = &app.program();
    const auto original_generation = app.generation();

    const auto malformed = app.recompose_scoped_xml_components(
        {{"only", "host", "<ui name=\"Bad\"><label"}},
        {{"only", "caption", "seed"}});
    check_false(static_cast<bool>(malformed));
    check(malformed.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check_equal(app.generation(), original_generation);
    check_equal(&app.box(), original_box);
    check_equal(&app.program(), original_program);

    // The proposed aliases belong to the proposed component set. Removing
    // the component while leaving an alias is never accepted as a fallback.
    const auto dangling = app.recompose_scoped_xml_components(
        {}, {{"only", "caption", "seed"}});
    check_false(static_cast<bool>(dangling));
    check(dangling.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    check_equal(app.generation(), original_generation);
    check_equal(&app.box(), original_box);

    flexUI::DesktopApplicationErrorCode other_thread =
        flexUI::DesktopApplicationErrorCode::None;
    std::thread worker([&] {
      other_thread = app.recompose_scoped_xml_components({}, {}).error.code;
    });
    worker.join();
    check(other_thread == flexUI::DesktopApplicationErrorCode::WrongThread);
    check_equal(app.generation(), original_generation);

    // A normal reload after rejected candidates must still compile the old
    // configuration, including its input alias and original component.
    check(app.reload({root, "", "", "after-invalid-recompose"}));
    app.box().update();
    check_not_null(app.box().get_by_id("only::caption"));
    check_equal(app.box().get_by_id("only::caption")->text(), "Original");

    check(app.request_close());
    const auto rejected_closed = app.recompose_scoped_xml_components({}, {});
    check_false(static_cast<bool>(rejected_closed));
    check(rejected_closed.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidState);
    check(app.shutdown());
  }
}

spec("Desktop XML components share one native ScriptController and Effects engine") {
  it("preserves ControllerFaulted reporting in native bridge after script failure") {
    auto probe = std::make_shared<ModuleProbe>();
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(
        "<ui name=\"Dashboard\"><main id=\"root\"><section id=\"host\"/></main></ui>")
        .scoped_xml_component(
            "sales", "host",
            "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
            "<button id=\"action\" on:key_down=\"save_document\">Initial</button>"
            "</ui>")
        .script("event-failure", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto *action = app.box().get_by_id("sales::action");
    check_not_null(action);
    if (!action) return;
    app.box().set_focus(action);
    auto key = flexUI::Event::key_down(flexUI::KeyCode::A);
    const auto failed = app.dispatch_event(key);
    check_false(static_cast<bool>(failed));
    check(failed.error.code ==
          flexUI::DesktopApplicationErrorCode::ControllerDispatchFailed);
    check(failed.error.controller_error.code ==
          flexUI::ControllerErrorCode::ModuleCallFailed);
    check(app.controller()->state() == flexUI::ControllerState::Faulted);
    check_equal(probe->events.size(), std::size_t{1});

    // Preserve the original fault result for the NEXT native event: the
    // route remains defined even though the module cannot be called again.
    key = flexUI::Event::key_down(flexUI::KeyCode::A);
    const auto rejected = app.dispatch_event(key);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::DesktopApplicationErrorCode::ControllerDispatchFailed);
    check(rejected.error.controller_error.code ==
          flexUI::ControllerErrorCode::ControllerFaulted);
    check_equal(probe->events.size(), std::size_t{1});
    const auto retired = app.retire_scoped_event_handlers("sales");
    check_false(static_cast<bool>(retired));
    check(retired.error.controller_error.code ==
          flexUI::ControllerErrorCode::InvalidState);
  }


  it("retires one scoped event route while preserving native bridge, Binding and other scopes") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_valid_mutation = true;
    const std::string root =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel = R"xml(
      <ui name="Panel" xmlns:on="urn:flexui:event"
          xmlns:bind="urn:flexui:binding">
        <div id="panel">
          <button id="action" on:key_down="save_document">Initial</button>
          <label id="caption" bind:text="$caption">Before</label>
        </div>
      </ui>
    )xml";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left", panel)
        .scoped_xml_component("sales", "right", panel)
        .scoped_binding_alias("sale", "caption", "sale_caption")
        .scoped_binding_alias("sales", "caption", "sales_caption")
        .binding_input_string("sale_caption", "One")
        .binding_input_string("sales_caption", "Two")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto *sale = app.box().get_by_id("sale::action");
    auto *sales = app.box().get_by_id("sales::action");
    check_not_null(sale);
    check_not_null(sales);
    if (!sale || !sales) return;
    app.box().set_focus(sales);
    auto event = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(event));
    check_equal(probe->events.size(), std::size_t{1});
    check_equal(probe->events.back().current_target.id, "sales::action");

    const auto generation = app.generation();
    const auto missing = app.retire_scoped_event_handlers("other");
    check_false(static_cast<bool>(missing));
    check(missing.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    check_equal(app.generation(), generation);
    check(app.controller()->has_active_event_binding(
        "sales::action", flexUI::UiEventKind::KeyDown));
    bool reentrant_blocked = false;
    app.box().set_event_callback([&](flexUI::Element &node, const flexUI::Event &native) {
      if (&node == sales && native.type == flexUI::EventType::KeyDown) {
        auto attempted = app.retire_scoped_event_handlers("sales");
        reentrant_blocked = !static_cast<bool>(attempted) &&
            attempted.error.code == flexUI::DesktopApplicationErrorCode::InvalidState;
      }
    });
    event = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(event));
    check(reentrant_blocked);
    check_equal(probe->events.size(), std::size_t{2});
    app.box().set_event_callback({});

    check(app.retire_scoped_event_handlers("sales"));
    check(app.retire_scoped_event_handlers("sales")); // Idempotent, same generation.
    check_equal(app.generation(), generation);
    check_false(app.controller()->has_active_event_binding(
        "sales::action", flexUI::UiEventKind::KeyDown));
    check(app.controller()->has_active_event_binding(
        "sale::action", flexUI::UiEventKind::KeyDown));
    check_not_null(app.program().find_event_binding(
        "sales::action", flexUI::UiEventKind::KeyDown));
    check_not_null(app.box().get_by_id("sales::action"));
    app.box().bindings().inputs().set_string("sales_caption", "Still Bound");
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Still Bound");
    event = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(event));
    check_equal(probe->events.size(), std::size_t{2});
    auto old_snapshot = flexUI::ScriptEventSnapshot{};
    old_snapshot.event = flexUI::UiEventKind::KeyDown;
    old_snapshot.target = app.box().handle_for(*sales);
    old_snapshot.current_target = old_snapshot.target;
    check(app.controller()->dispatch(old_snapshot));
    check_equal(probe->events.size(), std::size_t{2});

    app.box().set_focus(sale);
    event = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(event));
    check_equal(probe->events.size(), std::size_t{3});
    check_equal(probe->events.back().current_target.id, "sale::action");

    flexUI::DesktopApplicationErrorCode other_thread =
        flexUI::DesktopApplicationErrorCode::None;
    std::thread worker([&] {
      other_thread = app.retire_scoped_event_handlers("sale").error.code;
    });
    worker.join();
    check(other_thread == flexUI::DesktopApplicationErrorCode::WrongThread);
    check(app.controller()->has_active_event_binding(
        "sale::action", flexUI::UiEventKind::KeyDown));

    check(app.reload({root, "", "valid-controller", "event-routes-new-generation"}));
    // A detached reload publishes fresh typed input seeds; their native
    // Binding projections run in the ordinary Box update stage.
    app.box().update();
    check_equal(app.generation(), generation + 1);
    check(app.controller()->has_active_event_binding(
        "sales::action", flexUI::UiEventKind::KeyDown));
    auto *fresh_sales = app.box().get_by_id("sales::action");
    check_not_null(fresh_sales);
    if (!fresh_sales) return;
    app.box().set_focus(fresh_sales);
    event = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(event));
    check_equal(probe->events.size(), std::size_t{4});
    check_equal(probe->events.back().current_target.id, "sales::action");
    check(app.box().get_by_id("sales::caption")->text() == "Two");
  }


  it("redirects per-instance String Bool Number and resets on full reload") {
    const std::string root =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel = R"xml(
      <ui name="Panel" xmlns:bind="urn:flexui:binding">
        <div id="panel">
          <label id="caption" bind:text="$caption">Initial</label>
          <div id="state" bind:class_active="${enabled &amp;&amp; quantity &gt; 1}"/>
          <label id="footer" bind:text="$shared">Shared</label>
        </div>
      </ui>
    )xml";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sales", "left", panel)
        .scoped_xml_component("costs", "right", panel)
        .scoped_binding_alias("sales", "caption", "sales_caption")
        .scoped_binding_alias("sales", "enabled", "sales_enabled")
        .scoped_binding_alias("sales", "quantity", "sales_quantity")
        .scoped_binding_alias("costs", "caption", "costs_caption")
        .scoped_binding_alias("costs", "enabled", "costs_enabled")
        .scoped_binding_alias("costs", "quantity", "costs_quantity")
        .binding_input_string("sales_caption", "Revenue")
        .binding_input_string("costs_caption", "Expenses")
        .binding_input_string("shared", "One application")
        .binding_input_bool("sales_enabled", true)
        .binding_input_bool("costs_enabled", true)
        .binding_input_number("sales_quantity", 5)
        .binding_input_number("costs_quantity", 0);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check_equal(app.program().bindings().size(), std::size_t{6});
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Revenue");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");
    check_equal(app.box().get_by_id("sales::footer")->text(), "One application");
    check_equal(app.box().get_by_id("costs::footer")->text(), "One application");
    check(app.box().get_by_id("sales::state")->has_class(flexUI::Symbol("active")));
    check_false(app.box().get_by_id("costs::state")->has_class(flexUI::Symbol("active")));
    app.box().bindings().inputs().set_string("sales_caption", "Running");
    app.box().bindings().inputs().set_number("costs_quantity", 3.0);
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Running");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");
    check(app.box().get_by_id("costs::state")->has_class(flexUI::Symbol("active")));

    check(app.reload({root, "", "", "alias-reload"}));
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Revenue");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Expenses");
    check_false(app.box().get_by_id("costs::state")->has_class(flexUI::Symbol("active")));
  }

  it("refuses unknown scopes and unseeded alias targets before candidate mount") {
    const std::string root =
        "<ui name=\"App\"><main id=\"root\"><section id=\"host\"/></main></ui>";
    const std::string panel =
        "<ui name=\"Panel\" xmlns:bind=\"urn:flexui:binding\">"
        "<label id=\"caption\" bind:text=\"$caption\">Initial</label></ui>";
    flexUI::DesktopApplicationBuilder unknown_scope(nullptr);
    unknown_scope.xml_entry(root)
        .scoped_xml_component("actual", "host", panel)
        .binding_input_string("caption_value", "Ready")
        .scoped_binding_alias("other", "caption", "caption_value");
    const auto missing_scope = unknown_scope.build();
    check_false(static_cast<bool>(missing_scope));
    check(missing_scope.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    flexUI::DesktopApplicationBuilder unseeded(nullptr);
    unseeded.xml_entry(root)
        .scoped_xml_component("actual", "host", panel)
        .scoped_binding_alias("actual", "caption", "unknown");
    const auto missing_input = unseeded.build();
    check_false(static_cast<bool>(missing_input));
    check(missing_input.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    // Name resolution succeeds, but a Bool input cannot drive bind:text.
    // The detached candidate fails instead of projecting an invalid label.
    flexUI::DesktopApplicationBuilder wrong_type(nullptr);
    wrong_type.xml_entry(root)
        .scoped_xml_component("actual", "host", panel)
        .scoped_binding_alias("actual", "caption", "flag")
        .binding_input_bool("flag", true);
    const auto incompatible = wrong_type.build();
    check_false(static_cast<bool>(incompatible));
    check(incompatible.error.code ==
          flexUI::DesktopApplicationErrorCode::UiInstantiationFailed);
    check(incompatible.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::BindingInstallFailed);
  }

  it("dispatches same-local-id handlers to distinct qualified UiHandles") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_valid_mutation = true;
    const std::string root_ui =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/>"
        "</main></ui>";
    const std::string panel_xml =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<div id=\"action\" slot=\"control\" on:click=\"save_document\">"
        "Original</div></ui>";

    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("sales", "left", panel_xml)
        .scoped_xml_component("costs", "right", panel_xml)
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    const auto &owners = app.scoped_component_ownership();
    check_equal(owners.size(), std::size_t{2});
    if (owners.size() != 2) return;
    check_equal(owners[0].scope, "sales");
    check_equal(owners[0].host_id, "left");
    check_equal(owners[0].nodes.size(), std::size_t{1});
    check_equal(owners[0].nodes[0].qualified_id, "sales::action");
    check(owners[0].nodes[0].has_compiled_events);
    check_equal(owners[1].scope, "costs");
    check_equal(owners[1].host_id, "right");
    check_equal(owners[1].nodes[0].qualified_id, "costs::action");
    check(owners[1].nodes[0].has_compiled_events);
    // The live topology view verifies the original Box identities without
    // retaining pointers in any second runtime state or releasing resources.
    const auto sales_topology = app.inspect_scoped_component_topology("sales");
    const auto costs_topology = app.inspect_scoped_component_topology("costs");
    check(static_cast<bool>(sales_topology));
    check(static_cast<bool>(costs_topology));
    check(sales_topology.resolved_compiled_nodes == std::size_t{1});
    check(costs_topology.resolved_compiled_nodes == std::size_t{1});
    check(sales_topology.compiled_event_nodes == std::size_t{1});
    check(costs_topology.compiled_event_nodes == std::size_t{1});
    check(sales_topology.compiled_widget_hosts == std::size_t{0});
    const auto unknown_topology = app.inspect_scoped_component_topology("unknown");
    check(unknown_topology.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
    flexUI::DesktopApplicationErrorCode wrong_topology_thread =
        flexUI::DesktopApplicationErrorCode::None;
    std::thread topology_inspector([&] {
      wrong_topology_thread =
          app.inspect_scoped_component_topology("sales").error.code;
    });
    topology_inspector.join();
    check(wrong_topology_thread == flexUI::DesktopApplicationErrorCode::WrongThread);
    auto &box = app.box();
    auto* sales = box.get_by_id("sales::action");
    auto* costs = box.get_by_id("costs::action");
    check_not_null(sales);
    check_not_null(costs);
    check(sales != costs);
    check_null(box.get_by_id("action"));
    check_not_null(app.program().find_event_binding(
        "sales::action", flexUI::UiEventKind::Click));
    check_not_null(app.program().find_event_binding(
        "costs::action", flexUI::UiEventKind::Click));
    check_equal(*sales->attribute("data-slot"), "control");
    check_equal(*costs->attribute("data-slot"), "control");

    flexUI::ScriptEventSnapshot event;
    event.event = flexUI::UiEventKind::Click;
    event.target = box.handle_for(*sales);
    event.current_target = event.target;
    check(event.target);
    check(app.controller()->dispatch(event));
    check_equal(sales->text(), "Saved");
    check_equal(costs->text(), "Original");
    check_equal(probe->events.size(), std::size_t{1});
    check_equal(probe->events.front().current_target.id, "sales::action");

    event.target = box.handle_for(*costs);
    event.current_target = event.target;
    check(app.controller()->dispatch(event));
    check_equal(costs->text(), "Saved");
    check_equal(probe->events.size(), std::size_t{2});
    check_equal(probe->events.back().current_target.id, "costs::action");
    check(app.controller()->state() == flexUI::ControllerState::Mounted);
    check_equal(probe->created_sources.size(), std::size_t{1});
  }

  it("routes real focused native key events to the correct component instance") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_valid_mutation = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/>"
        "</main></ui>")
        .scoped_xml_component(
            "sales", "left",
            "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
            "<button id=\"action\" on:key_down=\"save_document\">Initial</button>"
            "</ui>")
        .scoped_xml_component(
            "costs", "right",
            "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
            "<button id=\"action\" on:key_down=\"save_document\">Initial</button>"
            "</ui>")
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto &box = app.box();
    auto *sales = box.get_by_id("sales::action");
    auto *costs = box.get_by_id("costs::action");
    check_not_null(sales);
    check_not_null(costs);
    if (!sales || !costs) return;

    box.set_focus(sales);
    auto key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    check_equal(sales->text(), "Saved");
    check_equal(costs->text(), "Initial");
    check_equal(probe->events.size(), std::size_t{1});
    check_equal(probe->events.front().target.id, "sales::action");
    check_equal(probe->events.front().current_target.id, "sales::action");

    box.set_focus(costs);
    key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    check_equal(costs->text(), "Saved");
    check_equal(probe->events.size(), std::size_t{2});
    check_equal(probe->events.back().target.id, "costs::action");
    check(app.controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("reserves commands before applying scoped UI mutations, then publishes once") {
    auto probe = std::make_shared<ModuleProbe>();
    probe->emit_valid_mutation = true;
    probe->emit_service_command = true;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/></main></ui>")
        .scoped_xml_component(
            "sales", "left",
            "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
            "<div id=\"action\" on:click=\"save_document\">Initial</div></ui>")
        .script("valid-controller", fake_factory(probe));
    static_cast<void>(configure_storage_service(builder));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;

    auto &app = *built.application;
    auto* target = app.box().get_by_id("sales::action");
    check_not_null(target);
    flexUI::ScriptEventSnapshot event;
    event.event = flexUI::UiEventKind::Click;
    event.target = app.box().handle_for(*target);
    event.current_target = event.target;
    check(app.controller()->dispatch(event));
    check_equal(target->text(), "Saved");
    const auto command = app.try_receive_service_request();
    check(static_cast<bool>(command));
    const auto stats = app.service_statistics();
    check_equal(stats.published, std::uint64_t{1});
    check_equal(stats.current_pending, std::size_t{1});
    check(app.controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("preserves active Box/controller when scoped reload compilation fails") {
    auto probe = std::make_shared<ModuleProbe>();
    const std::string root_ui =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/>"
        "</main></ui>";
    const std::string panel_xml =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<div id=\"action\" on:click=\"save_document\">Original</div></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("sales", "left", panel_xml)
        .scoped_xml_component("costs", "right", panel_xml)
        .script("valid-controller", fake_factory(probe));
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    auto *before = app.box().get_by_id("sales::action");
    check_not_null(before);
    const auto &before_owners = app.scoped_component_ownership();
    check_equal(before_owners.size(), std::size_t{2});
    if (before_owners.size() != 2) return;
    check_equal(before_owners[0].nodes[0].qualified_id, "sales::action");
    const auto generation = app.generation();

    const auto rejected = app.reload(
        {"<ui name=\"Wrong\"><main id=\"root\"><div id=\"left\"/></main></ui>",
         "", "valid-controller", "scoped-reload"});
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(rejected.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::MissingComponentHost);
    check(app.generation() == generation);
    check_equal(app.scoped_component_ownership().size(), std::size_t{2});
    check_equal(app.scoped_component_ownership()[0].nodes[0].qualified_id,
                "sales::action");
    check(app.box().get_by_id("sales::action") == before);
    check_not_null(app.box().get_by_id("costs::action"));
    check(app.controller()->state() == flexUI::ControllerState::Mounted);

    // The registered component definitions are recompiled on successful
    // reload and remain in the single post-reload controller handler table.
    const auto accepted = app.reload(
        {root_ui, "", "valid-controller", "scoped-reload"});
    check(static_cast<bool>(accepted));
    check_not_null(app.box().get_by_id("sales::action"));
    check_not_null(app.box().get_by_id("costs::action"));
    check_not_null(app.program().find_event_binding(
        "costs::action", flexUI::UiEventKind::Click));
  }

  it("declares shared typed binding inputs before scoped component mount and reload") {
    const std::string root_ui =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/>"
        "</main></ui>";
    const std::string component =
        "<ui name=\"Panel\" xmlns:bind=\"urn:flexui:binding\">"
        "<label id=\"caption\" bind:text=\"$caption\">Initial</label>"
        "</ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("sales", "left", component)
        .scoped_xml_component("costs", "right", component)
        .binding_input_string("caption", "Ready");
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check_equal(app.program().bindings().size(), std::size_t{2});
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Ready");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Ready");

    app.box().bindings().inputs().set_string("caption", "Running");
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Running");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Running");

    // Check the actual native DrawText commands, not only Element::text().
    // A mounted LabelWidget must not use its original constructor text as a
    // fallback after a shared bound string becomes empty.
    const auto painted_caption = [&](const char *id) {
      auto *label = app.box().get_by_id(id);
      check_not_null(label);
      if (!label || !label->widget) return std::string{};
      label->set_layout_bounds(0.0f, 0.0f, 240.0f, 48.0f);
      flexUI::RenderCommandList commands(flex::RendererCapabilities{});
      label->widget->emit_render_commands(*label, commands);
      std::string drawn;
      for (const auto &command : commands.commands()) {
        if (const auto *text =
                std::get_if<flexUI::DrawTextCommand>(&command)) {
          drawn += text->text;
        }
      }
      return drawn;
    };
    for (const char *id : {"sales::caption", "costs::caption"}) {
      check(painted_caption(id).find("Running") != std::string::npos);
    }

    app.box().bindings().inputs().set_string("caption", "");
    app.box().update();
    for (const char *id : {"sales::caption", "costs::caption"}) {
      check_equal(app.box().get_by_id(id)->text(), "");
      check(painted_caption(id).empty());
    }

    app.box().bindings().inputs().set_string("caption", "Again");
    app.box().update();
    for (const char *id : {"sales::caption", "costs::caption"}) {
      check_equal(app.box().get_by_id(id)->text(), "Again");
      check(painted_caption(id).find("Again") != std::string::npos);
    }

    // Reload publishes a new candidate with its original explicitly seeded
    // application-global inputs, rather than retaining stale pointers into
    // the prior component binding runtime.
    check(app.reload({root_ui, "", "", "reloaded"}));
    app.box().update();
    check_equal(app.box().get_by_id("sales::caption")->text(), "Ready");
    check_equal(app.box().get_by_id("costs::caption")->text(), "Ready");
  }


  it("seeds shared boolean and numeric MIR class bindings across instances") {
    const std::string root_ui =
        "<ui name=\"Dashboard\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/>"
        "</main></ui>";
    const std::string component = R"xml(
      <ui name="Panel" xmlns:bind="urn:flexui:binding">
        <div id="state" bind:class_active="${enabled}"
             bind:class_busy="${quantity &gt; 1}">Status</div>
      </ui>
    )xml";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("sales", "left", component)
        .scoped_xml_component("costs", "right", component)
        .binding_input_bool("enabled", true)
        .binding_input_number("quantity", 0.25);
    auto built = builder.build();
    check(static_cast<bool>(built));
    if (!built) return;
    auto &app = *built.application;
    check_equal(app.program().bindings().size(), std::size_t{4});
    app.box().update();
    for (const char *id : {"sales::state", "costs::state"}) {
      auto *node = app.box().get_by_id(id);
      check_not_null(node);
      if (!node) return;
      check(node->has_class(flexUI::Symbol("active")));
      check_false(node->has_class(flexUI::Symbol("busy")));
    }
    app.box().bindings().inputs().set_bool("enabled", false);
    app.box().bindings().inputs().set_number("quantity", 2.0);
    app.box().update();
    for (const char *id : {"sales::state", "costs::state"}) {
      auto *node = app.box().get_by_id(id);
      check_false(node->has_class(flexUI::Symbol("active")));
      check(node->has_class(flexUI::Symbol("busy")));
    }
    check(app.reload({root_ui, "", "", "typed-inputs-reload"}));
    app.box().update();
    for (const char *id : {"sales::state", "costs::state"}) {
      auto *node = app.box().get_by_id(id);
      check(node->has_class(flexUI::Symbol("active")));
      check_false(node->has_class(flexUI::Symbol("busy")));
    }
  }

  it("rejects incompatible typed input before candidate Box publication") {
    const std::string root_ui =
        "<ui name=\"Dashboard\"><main id=\"root\"><div id=\"host\"/>"
        "</main></ui>";
    const std::string component =
        "<ui name=\"Panel\" xmlns:bind=\"urn:flexui:binding\">"
        "<label id=\"caption\" bind:text=\"$caption\">Initial</label></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("panel", "host", component)
        .binding_input_bool("caption", true);
    const auto rejected = builder.build();
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::DesktopApplicationErrorCode::UiInstantiationFailed);
    check(rejected.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::BindingInstallFailed);
  }

  it("rejects aggregate scoped XML source amplification before parsing") {
    const std::string root_ui =
        "<ui name=\"App\"><main id=\"root\"><div id=\"left\"/>"
        "<div id=\"right\"/></main></ui>";
    const std::string component =
        "<ui name=\"Panel\"><label id=\"caption\">Caption</label></ui>";
    flexUI::DesktopApplicationLimits limits;
    limits.document.max_source_bytes =
        root_ui.size() + 2 * component.size() - 1;
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root_ui)
        .scoped_xml_component("first", "left", component)
        .scoped_xml_component("second", "right", component)
        .limits(limits);
    const auto rejected = builder.build();
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code ==
          flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(rejected.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::SourceTooLarge);
    // The same sources fit the exact aggregate limit without per-instance
    // multiplication of the budget.
    ++limits.document.max_source_bytes;
    builder.limits(limits);
    const auto admitted = builder.build();
    check(static_cast<bool>(admitted));
  }

  it("rejects duplicate/nonfinite typed input declarations before UI publication") {
    const std::string root_ui =
        "<ui name=\"Inputs\"><main id=\"root\"/></ui>";
    flexUI::DesktopApplicationBuilder duplicates(nullptr);
    duplicates.xml_entry(root_ui)
        .binding_input_string("caption", "Ready")
        .binding_input_bool("caption", true);
    auto duplicated = duplicates.build();
    check_false(static_cast<bool>(duplicated));
    check(duplicated.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);

    flexUI::DesktopApplicationBuilder nonfinite(nullptr);
    nonfinite.xml_entry(root_ui)
        .binding_input_number("quantity",
            std::numeric_limits<double>::infinity());
    auto invalid = nonfinite.build();
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code ==
          flexUI::DesktopApplicationErrorCode::InvalidConfiguration);
  }

  it("rejects duplicate component scopes and missing script exports before mount") {
    const std::string root_ui =
        "<ui name=\"Root\"><main id=\"root\"><div id=\"left\"/>"
        "<div id=\"right\"/></main></ui>";
    const std::string panel_xml =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<div id=\"go\" on:click=\"save_document\">Go</div></ui>";
    flexUI::DesktopApplicationBuilder duplicates(nullptr);
    duplicates.xml_entry(root_ui)
        .scoped_xml_component("same", "left", panel_xml)
        .scoped_xml_component("same", "right", panel_xml);
    const auto duplicate = duplicates.build();
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code ==
          flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(duplicate.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::InvalidComponentScope);

    flexUI::DesktopApplicationBuilder scriptless(nullptr);
    scriptless.xml_entry(root_ui)
        .scoped_xml_component("sales", "left", panel_xml);
    const auto no_module = scriptless.build();
    check_false(static_cast<bool>(no_module));
    check(no_module.error.code ==
          flexUI::DesktopApplicationErrorCode::ScriptRequired);
  }
}
