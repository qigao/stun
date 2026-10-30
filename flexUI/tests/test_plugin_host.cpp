#include <tinytest.hpp>

#include <flexUI/plugin_host.h>
#include <flexUI/application_service_dispatcher.h>
#include <flexUI/controller.h>
#include <flexUI/ui_xml.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

#ifndef FLEXUI_TEST_ECHO_PLUGIN
#error FLEXUI_TEST_ECHO_PLUGIN must name the test plugin library
#endif
#ifndef FLEXUI_TEST_BAD_ABI_PLUGIN
#error FLEXUI_TEST_BAD_ABI_PLUGIN must name the bad ABI test library
#endif
#ifndef FLEXUI_TEST_MISSING_ENTRY_PLUGIN
#error FLEXUI_TEST_MISSING_ENTRY_PLUGIN must name the missing-entry test library
#endif
#ifndef FLEXUI_TEST_FAIL_START_PLUGIN
#error FLEXUI_TEST_FAIL_START_PLUGIN must name the start-failure test library
#endif
#ifndef FLEXUI_TEST_FAIL_CREATE_PLUGIN
#error FLEXUI_TEST_FAIL_CREATE_PLUGIN must name the create-failure test library
#endif
#ifndef FLEXUI_TEST_FAIL_STOP_PLUGIN
#error FLEXUI_TEST_FAIL_STOP_PLUGIN must name the stop-failure test library
#endif
#ifndef FLEXUI_TEST_STATIC_CRT_PLUGIN
#error FLEXUI_TEST_STATIC_CRT_PLUGIN must name the static-CRT test library
#endif

namespace {

class RecordingSink final : public flexUI::IApplicationServiceCompletionSink {
public:
  explicit RecordingSink(bool reject_first = false)
      : reject_first_(reject_first) {}

  flexUI::ApplicationCompletionPostResult
  try_post(const flexUI::ApplicationCompletion &completion) override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++attempts_;
    if (reject_first_) {
      reject_first_ = false;
      return {{flexUI::ApplicationCompletionErrorCode::QueueFull,
               "test sink is full"}};
    }
    completions_.push_back(completion);
    ready_.notify_all();
    return {};
  }

  bool wait_for_count(std::size_t count, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return ready_.wait_for(lock, timeout,
                           [this, count] { return completions_.size() >= count; });
  }

  std::size_t attempts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attempts_;
  }

  flexUI::ApplicationCompletion first() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completions_.front();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  bool reject_first_ = false;
  std::size_t attempts_ = 0;
  std::vector<flexUI::ApplicationCompletion> completions_;
};

class ScopedPluginScript final : public flexUI::IScriptModule {
public:
  explicit ScopedPluginScript(
      std::shared_ptr<std::vector<flexUI::ApplicationServiceCompletion>> completions)
      : completions_(std::move(completions)) {}

  flexUI::ScriptResolveResult resolve_export(
      std::string_view name, flexUI::ScriptCallbackKind kind) override {
    if (name == "start_plugin_request" && kind == flexUI::ScriptCallbackKind::Event)
      return {flexUI::ScriptExportHandle{1}, {}};
    if (name == "on_service_completion" &&
        kind == flexUI::ScriptCallbackKind::ServiceCompletion)
      return {flexUI::ScriptExportHandle{2}, {}};
    return {};
  }

  flexUI::ScriptCallResult call(flexUI::ScriptExportHandle handle,
                                const flexUI::ScriptCallContext &context) override {
    flexUI::ScriptCallResult output;
    if (handle.value == 1 && context.callback == flexUI::ScriptCallbackKind::Event &&
        context.event != nullptr) {
      const auto &id = context.event->current_target.id;
      const bool retiring = id == "sale::action";
      if (!retiring && id != "sales::action")
        return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
                 "unexpected scoped plugin event target"}};
      auto appended = output.commands.append(
          {retiring ? 501u : 502u, "test.echo/1",
           retiring ? "cancel-watch" : "echo", "native-payload"});
      if (!appended)
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      return output;
    }
    if (handle.value == 2 &&
        context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        context.service_completion != nullptr) {
      completions_->push_back(*context.service_completion);
      return output;
    }
    return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
             "unexpected scoped plugin callback"}};
  }

private:
  std::shared_ptr<std::vector<flexUI::ApplicationServiceCompletion>> completions_;
};

class BuiltinEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &,
      flexUI::IApplicationServiceCompletionSink &) override {
    return {{flexUI::ApplicationServiceSubmitErrorCode::Rejected,
             "test endpoint does not execute requests"}};
  }
};

class ThrowingSink final : public flexUI::IApplicationServiceCompletionSink {
public:
  flexUI::ApplicationCompletionPostResult
  try_post(const flexUI::ApplicationCompletion &) override {
    throw std::runtime_error("test sink exception");
  }
};

flexUI::ApplicationServiceRequest request(std::uint64_t id,
                                          std::string operation,
                                          std::string payload = "payload") {
  return {{id, 1}, id, "test.echo/1", std::move(operation),
          std::move(payload)};
}

flexUI::ApplicationServiceRequest request_for(std::uint64_t id,
                                              std::string capability,
                                              std::string operation,
                                              std::string payload) {
  return {{id, 1}, id, std::move(capability), std::move(operation),
          std::move(payload)};
}

flexUI::ApplicationServiceResolveResult
resolve(const flexUI::PluginHostBuildResult &built,
        const flexUI::ApplicationServiceRequest &value) {
  flexUI::ApplicationCapabilityManifest manifest;
  manifest.allowed = {value.capability};
  manifest.required = {value.capability};
  return built.registry->resolve(value, manifest);
}

flexUI::PluginHostBuildResult build_echo_host(
    flexUI::PluginHostLimits limits = {}) {
  flexUI::PluginHostBuilder builder(limits);
  auto loaded = builder.load_plugin(std::filesystem::path(FLEXUI_TEST_ECHO_PLUGIN));
  if (!loaded) {
    flexUI::PluginHostBuildResult failed;
    failed.error = std::move(loaded.error);
    return failed;
  }
  return builder.build();
}

flexUI::PluginHostBuildResult
build_host(const std::filesystem::path &plugin_path) {
  flexUI::PluginHostBuilder builder;
  auto loaded = builder.load_plugin(plugin_path);
  if (!loaded) {
    flexUI::PluginHostBuildResult failed;
    failed.error = std::move(loaded.error);
    return failed;
  }
  return builder.build();
}

} // namespace

suite("FlexUI PluginHost") {
  group("loader validation") {
    it("rejects relative paths before loading") {
      flexUI::PluginHostBuilder builder;
      const auto result = builder.load_plugin("relative-plugin.dll");
      check_false(static_cast<bool>(result));
      check_equal(static_cast<int>(result.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidPath));
    }

    it("rejects a library without the fixed entry symbol") {
      flexUI::PluginHostBuilder builder;
      const auto result = builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_MISSING_ENTRY_PLUGIN));
      check_false(static_cast<bool>(result));
      check_equal(
          static_cast<int>(result.error.code),
          static_cast<int>(flexUI::PluginHostErrorCode::EntryPointMissing));
    }

    it("rejects an incompatible ABI major") {
      flexUI::PluginHostBuilder builder;
      const auto result = builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_BAD_ABI_PLUGIN));
      check_false(static_cast<bool>(result));
      check_equal(static_cast<int>(result.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::UnsupportedAbi));
    }

    it("reports create failure without publishing a partial plugin") {
      flexUI::PluginHostBuilder builder;
      const auto result = builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_FAIL_CREATE_PLUGIN));
      check_false(static_cast<bool>(result));
      check_equal(static_cast<int>(result.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::CreateFailed));
      check_equal(result.error.message,
                  std::string("create rejected by test plugin"));
    }
  }

  group("service bridge") {
    it("publishes built-in and plugin services in one immutable registry") {
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_ECHO_PLUGIN))));
      flexUI::ApplicationServiceDescriptor descriptor;
      descriptor.capability = "test.builtin/1";
      descriptor.operations.push_back({"read", 16});
      check_true(static_cast<bool>(builder.register_builtin(
          std::move(descriptor), std::make_shared<BuiltinEndpoint>())));
      auto built = builder.build();
      check_true(static_cast<bool>(built));
      check_equal(built.registry->size(), std::size_t{2});
      check_true(built.registry->contains("test.echo/1"));
      check_true(built.registry->contains("test.builtin/1"));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("routes a synchronous plugin completion through the registry") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(1, "echo", "hello");
      auto resolved = resolve(built, value);
      check_true(static_cast<bool>(resolved));
      RecordingSink sink;
      auto submitted = resolved.endpoint->try_submit(value, sink);
      check_true(static_cast<bool>(submitted));
      check_true(sink.wait_for_count(1, std::chrono::milliseconds(50)));
      check_equal(sink.first().payload, std::string("hello"));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("keeps a completion active while the sink requests retry") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(2, "retry", "again");
      auto resolved = resolve(built, value);
      check_true(static_cast<bool>(resolved));
      RecordingSink sink(true);
      check_true(static_cast<bool>(resolved.endpoint->try_submit(value, sink)));
      check_true(sink.wait_for_count(1, std::chrono::seconds(1)));
      check_greater(sink.attempts(), static_cast<std::size_t>(1));
      check_equal(sink.first().payload, std::string("again"));
      const auto statistics = built.host->statistics();
      check_equal(statistics.completion_retries, std::uint64_t{1});
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("rejects capacity plus one without growing the slot table") {
      flexUI::PluginHostLimits limits;
      limits.max_in_flight_per_plugin = 2;
      auto built = build_echo_host(limits);
      check_true(static_cast<bool>(built));
      RecordingSink sink;
      auto first = request(3, "hold");
      auto second = request(4, "hold");
      auto third = request(5, "hold");
      auto endpoint = resolve(built, first).endpoint;
      check_not_null(endpoint.get());
      check_true(static_cast<bool>(endpoint->try_submit(first, sink)));
      check_true(static_cast<bool>(endpoint->try_submit(second, sink)));
      const auto rejected = endpoint->try_submit(third, sink);
      check_false(static_cast<bool>(rejected));
      check_equal(static_cast<int>(rejected.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::Busy));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      const auto statistics = built.host->statistics();
      check_equal(statistics.abandoned, std::uint64_t{2});
    }

    it("defensively rejects undeclared operations on a cached endpoint") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(10, "echo");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      value.operation = "undeclared";
      RecordingSink sink;
      const auto rejected = endpoint->try_submit(value, sink);
      check_false(static_cast<bool>(rejected));
      check_equal(static_cast<int>(rejected.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::InvalidRequest));
      value.operation = "echo";
      value.payload.assign(4097, 'x');
      const auto oversized = endpoint->try_submit(value, sink);
      check_false(static_cast<bool>(oversized));
      check_equal(static_cast<int>(oversized.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::InvalidRequest));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("restores slot invariants when a plugin throws across the C ABI") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(8, "throw");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      RecordingSink sink;
      const auto failed = endpoint->try_submit(value, sink);
      check_false(static_cast<bool>(failed));
      check_equal(static_cast<int>(failed.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::InternalFailure));
      value.operation = "echo";
      check_true(static_cast<bool>(endpoint->try_submit(value, sink)));
      check_true(sink.wait_for_count(1, std::chrono::milliseconds(50)));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("consumes a slot when the completion sink throws") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(9, "echo");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      ThrowingSink throwing_sink;
      const auto failed = endpoint->try_submit(value, throwing_sink);
      check_false(static_cast<bool>(failed));
      RecordingSink sink;
      check_true(static_cast<bool>(endpoint->try_submit(value, sink)));
      check_true(sink.wait_for_count(1, std::chrono::milliseconds(50)));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }

    it("copies request completion and error bytes across a static CRT DLL") {
      auto built = build_host(
          std::filesystem::path(FLEXUI_TEST_STATIC_CRT_PLUGIN));
      check_true(static_cast<bool>(built));

      auto value = request_for(11, "test.static-crt/1", "deferred-copy",
                               "cross-crt-payload");
      auto resolved = resolve(built, value);
      check_true(static_cast<bool>(resolved));
      RecordingSink sink;
      check_true(static_cast<bool>(resolved.endpoint->try_submit(value, sink)));
      value.payload.assign("caller-storage-was-mutated");
      check_true(sink.wait_for_count(1, std::chrono::seconds(1)));
      check_equal(sink.first().payload, std::string("cross-crt-payload"));

      auto rejected = request_for(12, "test.static-crt/1", "reject", "x");
      const auto rejection = resolved.endpoint->try_submit(rejected, sink);
      check_false(static_cast<bool>(rejection));
      check_equal(rejection.error.message,
                  std::string("request rejected by test plugin"));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
    }
  }

  group("ABI v2 cooperative cancellation") {
    it("real DSO worker observes exact scoped token then publishes terminal") {
      auto host = build_echo_host();
      check_true(static_cast<bool>(host));
      if (!host) return;

      auto completions =
          std::make_shared<std::vector<flexUI::ApplicationServiceCompletion>>();
      const std::string root =
          "<ui name=\"PluginScope\"><main id=\"root\">"
          "<section id=\"left\"/><section id=\"right\"/></main></ui>";
      const std::string panel =
          "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
          "<button id=\"action\" on:key_down=\"start_plugin_request\">Run</button></ui>";
      flexUI::DesktopApplicationBuilder builder(nullptr);
      builder.xml_entry(root)
          .scoped_xml_component("sale", "left", panel)
          .scoped_xml_component("sales", "right", panel)
          .services(host.registry, {{"test.echo/1"}, {"test.echo/1"}})
          .script("real-plugin-v2", [completions](std::string_view,
                                                  std::string_view) {
            return flexUI::ScriptModuleFactoryResult{
                std::make_unique<ScopedPluginScript>(completions), {}};
          });
      auto app_built = builder.build();
      check_true(static_cast<bool>(app_built));
      if (!app_built) {
        check_true(static_cast<bool>(host.host->stop(std::chrono::seconds(2))));
        return;
      }
      auto &app = *app_built.application;
      auto *sale = app.box().get_by_id("sale::action");
      auto *sales = app.box().get_by_id("sales::action");
      check_not_null(sale);
      check_not_null(sales);
      if (!sale || !sales) {
        check_true(static_cast<bool>(host.host->stop(std::chrono::seconds(2))));
        return;
      }
      app.box().set_focus(sale);
      auto event = flexUI::Event::key_down(flexUI::KeyCode::A);
      check_true(static_cast<bool>(app.dispatch_event(event)));
      app.box().set_focus(sales);
      event = flexUI::Event::key_down(flexUI::KeyCode::A);
      check_true(static_cast<bool>(app.dispatch_event(event)));
      check_equal(app.service_statistics().current_pending, std::size_t{2});

      flexUI::ApplicationServiceDispatcherLimits limits;
      limits.max_requests_per_pump = 1;
      auto dispatcher = flexUI::ApplicationServiceDispatcher::create(app, limits);
      check_true(static_cast<bool>(dispatcher));
      if (!dispatcher) {
        check_true(static_cast<bool>(host.host->stop(std::chrono::seconds(2))));
        return;
      }
      auto first = dispatcher.dispatcher->pump();
      check_true(static_cast<bool>(first));
      check_equal(first.submitted, std::size_t{1});
      const auto outstanding = app.inspect_scoped_service_drain("sale");
      check_true(static_cast<bool>(outstanding));
      check_true(outstanding.in_flight == std::size_t{1});
      check_false(outstanding.event_routes_retired);
      check_false(outstanding.admission_quiesced);
      check_false(outstanding.queue_drained());
      check_true(static_cast<bool>(app.retire_scoped_event_handlers("sale")));
      auto q = app.quiesce_retired_scoped_service_requests("sale");
      check_true(static_cast<bool>(q));
      check_equal(q.cancelled_queued, std::size_t{0});
      check_equal(q.newly_suppressed_in_flight, std::size_t{1});
      check_equal(app.service_statistics().current_pending, std::size_t{2});
      // The DSO may have already POSTED a true CANCELLED, but no owner has
      // claimed it yet. The queue identity remains live and must not be
      // mistaken for safe resource/DLL teardown.
      const auto before_terminal = app.inspect_scoped_service_drain("sale");
      check_true(static_cast<bool>(before_terminal));
      check_true(before_terminal.event_routes_retired);
      check_true(before_terminal.admission_quiesced);
      check_true(before_terminal.in_flight == std::size_t{1});
      check_false(before_terminal.queue_drained());
      const auto active_sibling = app.inspect_scoped_service_drain("sales");
      check_true(static_cast<bool>(active_sibling));
      check_false(active_sibling.event_routes_retired);
      check_false(active_sibling.queue_drained());

      auto second = dispatcher.dispatcher->pump();
      if (second.submitted != std::size_t{1}) {
        info("second native DSO pump: submitted=%zu failed=%zu "
             "status=%d error=%s",
             second.submitted, second.failed,
             static_cast<int>(second.status),
             second.error.message.c_str());
      }
      check_true(static_cast<bool>(second));
      check_equal(second.submitted, std::size_t{1});
      std::size_t suppressed = 0;
      std::size_t delivered = 0;
      for (int n = 0; n < 3000 && suppressed + delivered < 2; ++n) {
        auto result = app.try_dispatch_service_completion();
        check_false(static_cast<bool>(result.error));
        if (result.error) break;
        if (result.status == flexUI::ApplicationServicePollStatus::Suppressed) {
          ++suppressed;
          // Only after consuming the DSO's actual terminal record is the
          // queue token retired. Plugin worker join is still a separate step.
          const auto after_terminal = app.inspect_scoped_service_drain("sale");
          check_true(after_terminal.queue_drained());
          check_true(after_terminal.pending() == std::size_t{0});
        } else if (result.status == flexUI::ApplicationServicePollStatus::Ready) {
          check_true(result.dispatched);
          ++delivered;
        } else if (result.status == flexUI::ApplicationServicePollStatus::Empty) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
          check_true(false);
          break;
        }
      }
      check_equal(suppressed, std::size_t{1});
      check_equal(delivered, std::size_t{1});
      check_equal(completions->size(), std::size_t{1});
      if (!completions->empty()) {
        check_equal(completions->front().script_request_id, std::uint64_t{502});
        check_equal(completions->front().payload, std::string("native-payload"));
      }
      check_equal(app.service_statistics().suppressed_completions,
                  std::uint64_t{1});
      check_equal(app.service_statistics().current_pending, std::size_t{0});
      check_true(app.controller()->state() == flexUI::ControllerState::Mounted);
      const auto final_drain = app.inspect_scoped_service_drain("sale");
      check_true(final_drain.queue_drained());
      // Do not treat that queue-only status as a worker-exit proof. The real
      // PluginHost still owns the dynamic DLL and must stop/join successfully.
      check_true(static_cast<bool>(host.host->stop(std::chrono::seconds(2))));
      check_true(static_cast<bool>(app.request_close()));
      check_true(static_cast<bool>(app.shutdown()));
    }
  }

  group("shutdown") {
    it("rolls back already-started candidates when a later start fails") {
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_ECHO_PLUGIN))));
      check_true(static_cast<bool>(builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_FAIL_START_PLUGIN))));
      auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::StartFailed));
      check_null(built.host.get());
      check_null(built.registry.get());
    }

    it("keeps the DLL loaded after timeout and permits a later join") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      RecordingSink sink;
      auto value = request(6, "block");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      check_true(static_cast<bool>(endpoint->try_submit(value, sink)));
      const auto timed_out = built.host->stop(std::chrono::milliseconds(1));
      check_false(static_cast<bool>(timed_out));
      check_equal(static_cast<int>(timed_out.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::JoinTimedOut));
      check_equal(static_cast<int>(built.host->state()),
                  static_cast<int>(flexUI::PluginHostState::Stopping));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      check_equal(static_cast<int>(built.host->state()),
                  static_cast<int>(flexUI::PluginHostState::Stopped));
    }

    it("rejects submissions after stop") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(7, "echo");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      RecordingSink sink;
      const auto rejected = endpoint->try_submit(value, sink);
      check_false(static_cast<bool>(rejected));
      check_equal(static_cast<int>(rejected.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::Closed));
    }

    it("retries a failed stop and unloads only after join succeeds") {
      auto built =
          build_host(std::filesystem::path(FLEXUI_TEST_FAIL_STOP_PLUGIN));
      check_true(static_cast<bool>(built));
      const auto first = built.host->stop(std::chrono::seconds(1));
      check_false(static_cast<bool>(first));
      check_equal(static_cast<int>(first.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::StopFailed));
      check_equal(static_cast<int>(built.host->state()),
                  static_cast<int>(flexUI::PluginHostState::Stopping));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      check_equal(static_cast<int>(built.host->state()),
                  static_cast<int>(flexUI::PluginHostState::Stopped));
    }

    it("makes stop idempotent and keeps cached endpoints closed after unload") {
      auto built = build_echo_host();
      check_true(static_cast<bool>(built));
      auto value = request(13, "echo", "after-stop");
      auto endpoint = resolve(built, value).endpoint;
      check_not_null(endpoint.get());
      auto registry = built.registry;

      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      check_true(static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
      built.host.reset();

      check_true(registry->contains("test.echo/1"));
      RecordingSink sink;
      const auto rejected = endpoint->try_submit(value, sink);
      check_false(static_cast<bool>(rejected));
      check_equal(static_cast<int>(rejected.error.code),
                  static_cast<int>(
                      flexUI::ApplicationServiceSubmitErrorCode::Closed));
    }
  }
}
