#include <flexUI/application_service_dispatcher.h>

#include <tinytest.hpp>

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
