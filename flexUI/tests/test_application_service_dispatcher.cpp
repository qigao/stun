#include <flexUI/application_service_dispatcher.h>

#include <tinytest.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

enum class EndpointBehavior {
  CompleteSynchronously,
  Reject,
  Throw,
};

struct DispatcherProbe {
  std::size_t command_count = 1;
  std::vector<std::uint64_t> submitted_ids;
  std::vector<flexUI::ApplicationServiceCompletion> completions;
};

class DispatcherEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  DispatcherEndpoint(std::shared_ptr<DispatcherProbe> probe,
                     EndpointBehavior behavior)
      : probe_(std::move(probe)), behavior_(behavior) {}

  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &request,
      flexUI::IApplicationServiceCompletionSink &completion_sink) override {
    probe_->submitted_ids.push_back(request.script_request_id);
    if (behavior_ == EndpointBehavior::Throw) {
      throw std::runtime_error("injected endpoint exception");
    }
    if (behavior_ == EndpointBehavior::Reject) {
      return {{flexUI::ApplicationServiceSubmitErrorCode::Rejected,
               "injected endpoint rejection"}};
    }

    flexUI::ApplicationCompletion completion;
    completion.token = request.token;
    completion.payload = "result-" + std::to_string(request.script_request_id);
    const auto posted = completion_sink.try_post(completion);
    if (!posted) {
      return {{flexUI::ApplicationServiceSubmitErrorCode::InternalFailure,
               posted.error.message}};
    }
    return {};
  }

private:
  std::shared_ptr<DispatcherProbe> probe_;
  EndpointBehavior behavior_;
};

class DispatcherModule final : public flexUI::IScriptModule {
public:
  explicit DispatcherModule(std::shared_ptr<DispatcherProbe> probe)
      : probe_(std::move(probe)) {}

  flexUI::ScriptResolveResult resolve_export(
      std::string_view name, flexUI::ScriptCallbackKind callback) override {
    if (name == "on_frame" && callback == flexUI::ScriptCallbackKind::Frame) {
      return {flexUI::ScriptExportHandle{1}, {}};
    }
    if (name == "on_service_completion" &&
        callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
      return {flexUI::ScriptExportHandle{2}, {}};
    }
    return {};
  }

  flexUI::ScriptCallResult call(
      flexUI::ScriptExportHandle handle,
      const flexUI::ScriptCallContext &context) override {
    flexUI::ScriptCallResult result;
    if (handle.value == 1 && context.callback == flexUI::ScriptCallbackKind::Frame) {
      if (!commands_emitted_) {
        for (std::size_t index = 0; index < probe_->command_count; ++index) {
          const auto appended = result.commands.append(
              {static_cast<std::uint64_t>(101 + index), "storage/1", "write",
               "document"});
          if (!appended) {
            return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                     appended.error.message}};
          }
        }
        commands_emitted_ = true;
      }
      return result;
    }
    if (handle.value == 2 &&
        context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        context.service_completion != nullptr) {
      probe_->completions.push_back(*context.service_completion);
      return result;
    }
    return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
             "invalid dispatcher test callback"}};
  }

private:
  std::shared_ptr<DispatcherProbe> probe_;
  bool commands_emitted_ = false;
};

/// Real compiler-scoped callback source; the host must never be asked to
/// submit commands already cancelled by application-owned scope retirement.
class ScopedDispatcherModule final : public flexUI::IScriptModule {
public:
  explicit ScopedDispatcherModule(std::shared_ptr<DispatcherProbe> probe)
      : probe_(std::move(probe)) {}

  flexUI::ScriptResolveResult resolve_export(
      std::string_view name, flexUI::ScriptCallbackKind callback) override {
    if (name == "write_document" && callback == flexUI::ScriptCallbackKind::Event) {
      return {flexUI::ScriptExportHandle{1}, {}};
    }
    if (name == "on_service_completion" &&
        callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
      return {flexUI::ScriptExportHandle{2}, {}};
    }
    return {};
  }

  flexUI::ScriptCallResult call(
      flexUI::ScriptExportHandle handle,
      const flexUI::ScriptCallContext &context) override {
    flexUI::ScriptCallResult result;
    if (handle.value == 1 && context.callback == flexUI::ScriptCallbackKind::Event &&
        context.event != nullptr) {
      const auto &id = context.event->current_target.id;
      std::uint64_t request_id = 0;
      if (id == "sale::action") {
        request_id = 201;
      } else if (id == "sales::action") {
        request_id = 202;
      } else {
        return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
                 "unexpected compiled scope event target"}};
      }
      const auto appended = result.commands.append(
          {request_id, "storage/1", "write", "document"});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      }
      return result;
    }
    if (handle.value == 2 &&
        context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        context.service_completion != nullptr) {
      probe_->completions.push_back(*context.service_completion);
      return result;
    }
    return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
             "unexpected scoped dispatcher callback"}};
  }

private:
  std::shared_ptr<DispatcherProbe> probe_;
};

/// A native endpoint owns request bytes, an exact-token cancellation lease
/// and the borrowed live completion sink until it posts the actual terminal
/// record. No fake terminal result is generated by scope quiescence.
class DeferredCooperativeEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &request,
      flexUI::IApplicationServiceCompletionSink &sink) override {
    accepted_.push_back({request, &sink});
    return {};
  }

  bool has_request(std::uint64_t request_id) const {
    return std::any_of(accepted_.begin(), accepted_.end(),
                       [request_id](const Entry &entry) {
                         return entry.request.script_request_id == request_id;
                       });
  }

  bool stop_was_requested(std::uint64_t request_id) const {
    auto it = std::find_if(accepted_.begin(), accepted_.end(),
                           [request_id](const Entry &entry) {
                             return entry.request.script_request_id == request_id;
                           });
    return it != accepted_.end() && it->request.cancellation &&
           it->request.cancellation->requested();
  }

  // Called from a REAL worker thread after it observes the cancellation lease.
  // The endpoint, not the Box or queue, chooses its terminal status and posts.
  bool finish_from_worker(std::uint64_t request_id) {
    auto it = std::find_if(accepted_.begin(), accepted_.end(),
                           [request_id](const Entry &entry) {
                             return entry.request.script_request_id == request_id;
                           });
    if (it == accepted_.end() || it->sink == nullptr || !it->request.cancellation)
      return false;

    flexUI::ApplicationCompletion completion;
    completion.token = it->request.token;
    if (it->request.cancellation->requested()) {
      completion.status = flexUI::ApplicationCompletionStatus::Cancelled;
      completion.error_message = "native worker observed cooperative stop";
    } else {
      completion.status = flexUI::ApplicationCompletionStatus::Succeeded;
      completion.payload = "result-" + std::to_string(request_id);
    }
    const auto posted = it->sink->try_post(completion);
    if (!posted) return false;
    accepted_.erase(it); // Worker released its own request and lease.
    return true;
  }

private:
  struct Entry {
    flexUI::ApplicationServiceRequest request;
    flexUI::IApplicationServiceCompletionSink *sink = nullptr;
  };
  std::vector<Entry> accepted_;
};

struct DispatcherFixture {
  std::shared_ptr<DispatcherProbe> probe;
  std::shared_ptr<DispatcherEndpoint> endpoint;
  flexUI::DesktopApplicationBuildResult built;
};

DispatcherFixture build_application(
    EndpointBehavior behavior = EndpointBehavior::CompleteSynchronously,
    std::size_t command_count = 1, std::size_t completion_capacity = 256) {
  DispatcherFixture fixture;
  fixture.probe = std::make_shared<DispatcherProbe>();
  fixture.probe->command_count = command_count;
  fixture.endpoint =
      std::make_shared<DispatcherEndpoint>(fixture.probe, behavior);

  flexUI::ApplicationServiceRegistryBuilder registry_builder;
  check(registry_builder.register_service(
      {"storage/1", {{"write", 1024}}}, fixture.endpoint));
  auto registry = registry_builder.build();
  check(registry);

  flexUI::DesktopApplicationLimits limits;
  limits.completion.capacity = completion_capacity;
  flexUI::DesktopApplicationBuilder builder(nullptr);
  builder.xml_entry("<ui name=\"DispatcherTest\"><div id=\"surface\"/></ui>")
      .limits(limits)
      .services(std::move(registry.registry),
                {{"storage/1"}, {"storage/1"}})
      .script("dispatcher-test", [probe = fixture.probe](std::string_view,
                                                          std::string_view) {
        return flexUI::ScriptModuleFactoryResult{
            std::make_unique<DispatcherModule>(probe), {}};
      });
  fixture.built = builder.build();
  check(fixture.built);
  if (fixture.built) {
    check(fixture.built.application->frame(0.0));
  }
  return fixture;
}

} // namespace

spec("FlexUI application service dispatcher") {
  it("rejects invalid request bounds") {
    auto fixture = build_application();
    if (!fixture.built) {
      return;
    }

    flexUI::ApplicationServiceDispatcherLimits limits;
    limits.max_requests_per_pump = 0;
    auto zero = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application, limits);
    check_false(static_cast<bool>(zero));
    check(zero.error.code ==
          flexUI::ApplicationServiceDispatcherErrorCode::InvalidLimits);

    limits.max_requests_per_pump =
        flexUI::ApplicationServiceDispatcherLimits::kMaximumRequestsPerPump + 1;
    auto excessive = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application, limits);
    check_false(static_cast<bool>(excessive));
    check(excessive.error.code ==
          flexUI::ApplicationServiceDispatcherErrorCode::InvalidLimits);
  }

  it("submits requests in bounded FIFO pumps") {
    auto fixture = build_application(EndpointBehavior::CompleteSynchronously, 2);
    if (!fixture.built) {
      return;
    }
    flexUI::ApplicationServiceDispatcherLimits limits;
    limits.max_requests_per_pump = 1;
    auto created = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application, limits);
    check(created);
    if (!created) {
      return;
    }

    auto first = created.dispatcher->pump();
    check(first);
    check(first.status == flexUI::ApplicationServiceDispatchStatus::Progress);
    check_equal(first.submitted, std::size_t{1});
    check_equal(fixture.probe->submitted_ids.size(), std::size_t{1});
    check_equal(fixture.probe->submitted_ids.front(), std::uint64_t{101});
    check(fixture.built.application->try_dispatch_service_completion());

    auto second = created.dispatcher->pump();
    check(second);
    check_equal(second.submitted, std::size_t{1});
    check_equal(fixture.probe->submitted_ids.size(), std::size_t{2});
    check_equal(fixture.probe->submitted_ids.back(), std::uint64_t{102});
    check(fixture.built.application->try_dispatch_service_completion());
    check_equal(fixture.probe->completions.size(), std::size_t{2});
  }

  it("converts endpoint rejection into a failed completion") {
    auto fixture = build_application(EndpointBehavior::Reject);
    if (!fixture.built) {
      return;
    }
    auto created = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application);
    check(created);
    if (!created) {
      return;
    }

    const auto pumped = created.dispatcher->pump();
    check(pumped);
    check_equal(pumped.failed, std::size_t{1});
    check(fixture.built.application->try_dispatch_service_completion());
    check_equal(fixture.probe->completions.size(), std::size_t{1});
    check(fixture.probe->completions.front().status ==
          flexUI::ApplicationCompletionStatus::Failed);
    check_equal(fixture.probe->completions.front().error_code,
                std::string("service.submit.rejected"));
    check_equal(fixture.probe->completions.front().error_message,
                std::string("injected endpoint rejection"));
  }

  it("converts endpoint exceptions without crossing the host boundary") {
    auto fixture = build_application(EndpointBehavior::Throw);
    if (!fixture.built) {
      return;
    }
    auto created = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application);
    check(created);
    if (!created) {
      return;
    }

    check(created.dispatcher->pump());
    check(fixture.built.application->try_dispatch_service_completion());
    check_equal(fixture.probe->completions.front().error_code,
                std::string("service.submit.exception"));
  }

  it("never submits a retired scope's queued command to the real endpoint") {
    auto probe = std::make_shared<DispatcherProbe>();
    auto endpoint = std::make_shared<DispatcherEndpoint>(
        probe, EndpointBehavior::CompleteSynchronously);
    flexUI::ApplicationServiceRegistryBuilder registry_builder;
    check(registry_builder.register_service(
        {"storage/1", {{"write", 1024}}}, endpoint));
    auto registry = registry_builder.build();
    check(registry);
    if (!registry) return;

    const std::string root =
        "<ui name=\"ScopedDispatcher\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<button id=\"action\" on:key_down=\"write_document\">Run</button></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left", panel)
        .scoped_xml_component("sales", "right", panel)
        .services(std::move(registry.registry),
                  {{"storage/1"}, {"storage/1"}})
        .script("scoped-dispatcher", [probe](std::string_view, std::string_view) {
          return flexUI::ScriptModuleFactoryResult{
              std::make_unique<ScopedDispatcherModule>(probe), {}};
        });
    auto built = builder.build();
    check(built);
    if (!built) return;
    auto &app = *built.application;
    auto *sale = app.box().get_by_id("sale::action");
    auto *sales = app.box().get_by_id("sales::action");
    check_not_null(sale);
    check_not_null(sales);
    if (!sale || !sales) return;

    app.box().set_focus(sale);
    auto key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    app.box().set_focus(sales);
    key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    check_equal(app.service_statistics().current_pending, std::size_t{2});

    // The application must retire all script routes before calling the
    // queue quiescence facade. No service endpoint has been contacted yet.
    check(app.retire_scoped_event_handlers("sale"));
    const auto quiesced = app.quiesce_retired_scoped_service_requests("sale");
    check(quiesced);
    check_equal(quiesced.cancelled_queued, std::size_t{1});
    check_equal(quiesced.newly_suppressed_in_flight, std::size_t{0});
    check(probe->submitted_ids.empty());

    auto dispatcher = flexUI::ApplicationServiceDispatcher::create(app);
    check(dispatcher);
    if (!dispatcher) return;
    const auto pumped = dispatcher.dispatcher->pump();
    check(pumped);
    check_equal(pumped.submitted, std::size_t{1});
    check_equal(probe->submitted_ids.size(), std::size_t{1});
    check_equal(probe->submitted_ids.front(), std::uint64_t{202});
    check(app.try_receive_service_request().status ==
          flexUI::ApplicationServicePollStatus::Empty);

    auto completion = app.try_dispatch_service_completion();
    check(completion);
    check(completion.dispatched);
    check_equal(probe->completions.size(), std::size_t{1});
    check_equal(probe->completions.front().script_request_id, std::uint64_t{202});
    check_equal(app.service_statistics().cancelled, std::uint64_t{1});
    check_equal(app.service_statistics().completed, std::uint64_t{1});
    check_equal(app.service_statistics().current_pending, std::size_t{0});
    check(app.controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("signals native in-flight cooperative stop but waits for the real worker terminal") {
    auto probe = std::make_shared<DispatcherProbe>();
    auto endpoint = std::make_shared<DeferredCooperativeEndpoint>();
    flexUI::ApplicationServiceRegistryBuilder registry_builder;
    check(registry_builder.register_service(
        {"storage/1", {{"write", 1024}}}, endpoint));
    auto registry = registry_builder.build();
    check(registry);
    if (!registry) return;

    const std::string root =
        "<ui name=\"AsyncScoped\"><main id=\"root\">"
        "<section id=\"left\"/><section id=\"right\"/></main></ui>";
    const std::string panel =
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<button id=\"action\" on:key_down=\"write_document\">Run</button></ui>";
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry(root)
        .scoped_xml_component("sale", "left", panel)
        .scoped_xml_component("sales", "right", panel)
        .services(std::move(registry.registry),
                  {{"storage/1"}, {"storage/1"}})
        .script("native-cooperative", [probe](std::string_view, std::string_view) {
          return flexUI::ScriptModuleFactoryResult{
              std::make_unique<ScopedDispatcherModule>(probe), {}};
        });
    auto built = builder.build();
    check(built);
    if (!built) return;
    auto &app = *built.application;
    auto *sale = app.box().get_by_id("sale::action");
    auto *sales = app.box().get_by_id("sales::action");
    check_not_null(sale);
    check_not_null(sales);
    if (!sale || !sales) return;

    app.box().set_focus(sale);
    auto key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    app.box().set_focus(sales);
    key = flexUI::Event::key_down(flexUI::KeyCode::A);
    check(app.dispatch_event(key));
    check_equal(app.service_statistics().current_pending, std::size_t{2});

    flexUI::ApplicationServiceDispatcherLimits limits;
    limits.max_requests_per_pump = 1;
    auto created = flexUI::ApplicationServiceDispatcher::create(app, limits);
    check(created);
    if (!created) return;
    const auto first = created.dispatcher->pump();
    check(first);
    check_equal(first.submitted, std::size_t{1});
    check(endpoint->has_request(201));
    check_false(endpoint->stop_was_requested(201));

    // InFlight must remain counted and owned after the cooperative signal.
    check(app.retire_scoped_event_handlers("sale"));
    const auto q = app.quiesce_retired_scoped_service_requests("sale");
    check(q);
    check_equal(q.cancelled_queued, std::size_t{0});
    check_equal(q.newly_suppressed_in_flight, std::size_t{1});
    check(endpoint->stop_was_requested(201));
    check_equal(app.service_statistics().current_pending, std::size_t{2});
    check(probe->completions.empty());

    bool worker_cancel_posted = false;
    std::thread worker([&] {
      worker_cancel_posted = endpoint->finish_from_worker(201);
    });
    worker.join();
    check(worker_cancel_posted);
    // Completion exists, but the queue does not release its lease until the
    // owner-thread mailbox consumer claims that exact token.
    check_equal(app.service_statistics().current_pending, std::size_t{2});
    const auto dropped = app.try_dispatch_service_completion();
    check(dropped.status == flexUI::ApplicationServicePollStatus::Suppressed);
    check_false(dropped.dispatched);
    check_false(dropped.completion.has_value());
    check(probe->completions.empty());
    check_equal(app.service_statistics().current_pending, std::size_t{1});

    const auto second = created.dispatcher->pump();
    check(second);
    check_equal(second.submitted, std::size_t{1});
    check(endpoint->has_request(202));
    check_false(endpoint->stop_was_requested(202));
    bool worker_success_posted = false;
    std::thread sibling_worker([&] {
      worker_success_posted = endpoint->finish_from_worker(202);
    });
    sibling_worker.join();
    check(worker_success_posted);
    const auto delivered = app.try_dispatch_service_completion();
    check(delivered);
    check(delivered.dispatched);
    check_equal(probe->completions.size(), std::size_t{1});
    check_equal(probe->completions.front().script_request_id, std::uint64_t{202});
    check(probe->completions.front().status ==
          flexUI::ApplicationCompletionStatus::Succeeded);
    check_equal(app.service_statistics().current_pending, std::size_t{0});
    check_equal(app.service_statistics().suppressed_completions, std::uint64_t{1});
    check(app.controller()->state() == flexUI::ControllerState::Mounted);
  }

  it("rejects a foreign owner-thread pump") {
    auto fixture = build_application();
    if (!fixture.built) {
      return;
    }
    auto created = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application);
    check(created);
    if (!created) {
      return;
    }

    flexUI::ApplicationServiceDispatchResult foreign;
    std::thread worker([&] { foreign = created.dispatcher->pump(); });
    worker.join();
    check_false(static_cast<bool>(foreign));
    check(foreign.error.code ==
          flexUI::ApplicationServiceDispatcherErrorCode::WrongThread);
  }

  it("retries one terminal completion after mailbox backpressure") {
    auto fixture = build_application(EndpointBehavior::Reject, 2, 1);
    if (!fixture.built) {
      return;
    }

    auto first_request =
        fixture.built.application->try_receive_service_request();
    check(first_request);
    flexUI::ApplicationCompletion occupying;
    occupying.token = first_request.request->token;
    occupying.payload = "occupying";
    check(fixture.built.application->completion_mailbox().try_post(occupying));

    auto created = flexUI::ApplicationServiceDispatcher::create(
        *fixture.built.application);
    check(created);
    if (!created) {
      return;
    }
    auto blocked = created.dispatcher->pump();
    check(blocked);
    check(blocked.status == flexUI::ApplicationServiceDispatchStatus::Blocked);
    check_true(created.dispatcher->has_pending_completion());

    check(fixture.built.application->try_dispatch_service_completion());
    auto retried = created.dispatcher->pump();
    check(retried);
    check(retried.status == flexUI::ApplicationServiceDispatchStatus::Progress);
    check_false(created.dispatcher->has_pending_completion());
    check(fixture.built.application->try_dispatch_service_completion());
    check_equal(fixture.probe->completions.size(), std::size_t{2});
  }
}
