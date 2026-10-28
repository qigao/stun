#include <flexUI/application_service.h>
#include <flexUI/controller.h>
#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>

#include "application_service_requests.hpp"

#include <tinytest.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

class ScopedServiceModule final : public flexUI::IScriptModule {
public:
  flexUI::ScriptResolveResult resolve_export(
      std::string_view name, flexUI::ScriptCallbackKind kind) override {
    if (name == "issue_request" && kind == flexUI::ScriptCallbackKind::Event)
      return {flexUI::ScriptExportHandle{1}, {}};
    return {};
  }

  flexUI::ScriptCallResult call(
      flexUI::ScriptExportHandle handle, const flexUI::ScriptCallContext &) override {
    if (handle.value != 1)
      return {{flexUI::ScriptModuleErrorCode::InvalidExport, "bad scoped export"}};
    flexUI::ScriptCallResult result;
    auto added = result.commands.append(
        {next_id_++, "storage/1", "read", "payload"});
    if (!added)
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
               "cannot append scoped test command"}};
    return result;
  }
private:
  std::uint64_t next_id_ = 1000;
};

class TestServiceEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult
  try_submit(const flexUI::ApplicationServiceRequest &,
             flexUI::IApplicationServiceCompletionSink &) override {
    return {};
  }
};

flexUI::ApplicationServiceRequestQueueCreateResult
registered_queue(std::uint64_t generation, flexUI::ApplicationServiceRequestLimits limits,
                 flexUI::ApplicationCapabilityManifest manifest) {
  flexUI::ApplicationServiceRegistryBuilder builder;
  flexUI::ApplicationServiceDescriptor service{"storage/1",
                                               {{"read", 1024}, {"first", 1024}, {"second", 1024}}};
  check(builder.register_service(std::move(service), std::make_shared<TestServiceEndpoint>()));
  auto registry = builder.build();
  check(registry);
  return flexUI::ApplicationServiceRequestQueue::create(
      generation, limits, std::move(registry.registry), std::move(manifest));
}

flexUI::ApplicationServiceRequestQueueCreateResult
registered_queue(std::uint64_t generation, flexUI::ApplicationServiceRequestLimits limits = {}) {
  return registered_queue(generation, limits, {{"storage/1"}, {"storage/1"}});
}

flexUI::ApplicationCommand command(std::uint64_t request_id, std::string operation = "read") {
  return {request_id, "storage/1", std::move(operation), "payload"};
}

flexUI::ApplicationCommandBatch
batch_with(std::initializer_list<flexUI::ApplicationCommand> commands) {
  flexUI::ApplicationCommandBatch batch;
  for (const auto &value : commands) {
    check(batch.append(value));
  }
  return batch;
}

flexUI::ApplicationCompletion completion(
    flexUI::ApplicationRequestToken token,
    flexUI::ApplicationCompletionStatus status = flexUI::ApplicationCompletionStatus::Succeeded) {
  flexUI::ApplicationCompletion value;
  value.token = token;
  value.status = status;
  value.payload = status == flexUI::ApplicationCompletionStatus::Succeeded ? "result" : "";
  if (status == flexUI::ApplicationCompletionStatus::Failed) {
    value.error_code = "service-failed";
    value.error_message = "service operation failed";
  }
  return value;
}

} // namespace

spec("FlexUI application service requests") {
  it("rejects zero and overflowing capacities") {
    auto invalid_generation = flexUI::ApplicationServiceRequestQueue::create(0);
    check_false(static_cast<bool>(invalid_generation));
    check(invalid_generation.error.code == flexUI::ApplicationServiceErrorCode::InvalidGeneration);

    flexUI::ApplicationServiceRequestLimits limits;
    limits.capacity = 0;
    auto zero = flexUI::ApplicationServiceRequestQueue::create(1, limits);
    check_false(static_cast<bool>(zero));
    check(zero.error.code == flexUI::ApplicationServiceErrorCode::InvalidCapacity);

    limits.capacity = std::numeric_limits<std::size_t>::max();
    auto overflowing = flexUI::ApplicationServiceRequestQueue::create(1, limits);
    check_false(static_cast<bool>(overflowing));
    check(overflowing.error.code == flexUI::ApplicationServiceErrorCode::InvalidCapacity);
  }

  it("reserves exact capacity and releases discarded reservations") {
    flexUI::ApplicationServiceRequestLimits limits;
    limits.capacity = 2;
    auto created = registered_queue(3, limits);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);

    auto first = engine.reserve(batch_with({command(10), command(11)}));
    check(first);
    check_equal(created.queue->statistics().current_pending, std::size_t{2});

    auto full = engine.reserve(batch_with({command(12)}));
    check_false(static_cast<bool>(full));
    check(full.error.code == flexUI::ApplicationCommandErrorCode::QueueFull);

    first.prepared.reset();
    const auto released = created.queue->statistics();
    check_equal(released.current_pending, std::size_t{0});
    check_equal(released.discarded, std::uint64_t{2});
    check_equal(released.queue_full, std::uint64_t{1});
  }

  it("rejects unauthorized unknown unsupported and oversized commands before reservation") {
    auto restricted = registered_queue(11);
    check(restricted);
    flexUI::ApplicationCommandEngine restricted_engine(*restricted.queue);

    auto unauthorized =
        restricted_engine.reserve(batch_with({{1, "document.print/1", "read", "payload"}}));
    check_false(static_cast<bool>(unauthorized));
    check(unauthorized.error.code == flexUI::ApplicationCommandErrorCode::UnknownCapability);

    auto unsupported =
        restricted_engine.reserve(batch_with({{2, "storage/1", "remove", "payload"}}));
    check_false(static_cast<bool>(unsupported));
    check(unsupported.error.code == flexUI::ApplicationCommandErrorCode::UnsupportedOperation);

    auto oversized =
        restricted_engine.reserve(batch_with({{3, "storage/1", "read", std::string(1025, 'x')}}));
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code == flexUI::ApplicationCommandErrorCode::InvalidPayload);
    check_equal(restricted.queue->statistics().current_pending, std::size_t{0});

    auto permits_missing =
        registered_queue(12, {}, {{"storage/1", "document.print/1"}, {"storage/1"}});
    check(permits_missing);
    flexUI::ApplicationCommandEngine missing_engine(*permits_missing.queue);
    auto unknown = missing_engine.reserve(batch_with({{4, "document.print/1", "read", "payload"}}));
    check_false(static_cast<bool>(unknown));
    check(unknown.error.code == flexUI::ApplicationCommandErrorCode::UnknownCapability);
    check_equal(permits_missing.queue->statistics().current_pending, std::size_t{0});
  }

  it("publishes once and transfers requests in global FIFO order") {
    flexUI::ApplicationServiceRequestLimits limits;
    limits.capacity = 2;
    auto created = registered_queue(4, limits);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);

    auto prepared = engine.reserve(batch_with({command(21, "first"), command(22, "second")}));
    check(prepared);
    prepared.prepared->publish();
    prepared.prepared->publish();

    auto first = created.queue->try_receive_request();
    check(first);
    check_equal(first.request->script_request_id, std::uint64_t{21});
    check_equal(first.request->operation, std::string("first"));
    check_equal(first.request->token.generation, std::uint64_t{4});

    auto second = created.queue->try_receive_request();
    check(second);
    check_equal(second.request->script_request_id, std::uint64_t{22});
    check_equal(second.request->operation, std::string("second"));
    check_true(second.request->token.id > first.request->token.id);

    auto empty = created.queue->try_receive_request();
    check(empty.status == flexUI::ApplicationServicePollStatus::Empty);
    const auto stats = created.queue->statistics();
    check_equal(stats.published, std::uint64_t{2});
    check_equal(stats.dispatched, std::uint64_t{2});
  }

  it("rejects duplicate script request ids while either request is pending") {
    auto created = registered_queue(5);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);

    auto duplicate_batch = engine.reserve(batch_with({command(31), command(31)}));
    check_false(static_cast<bool>(duplicate_batch));
    check(duplicate_batch.error.code == flexUI::ApplicationCommandErrorCode::InvalidRequestId);

    auto pending = engine.reserve(batch_with({command(32)}));
    check(pending);
    pending.prepared->publish();
    check(created.queue->try_receive_request());

    auto duplicate_pending = engine.reserve(batch_with({command(32)}));
    check_false(static_cast<bool>(duplicate_pending));
    check(duplicate_pending.error.code == flexUI::ApplicationCommandErrorCode::InvalidRequestId);
    check_equal(created.queue->statistics().rejected_duplicate, std::uint64_t{2});
  }

  it("resolves only dispatched tokens and releases script identity once") {
    auto created = registered_queue(6);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);
    auto prepared = engine.reserve(batch_with({command(41)}));
    check(prepared);
    prepared.prepared->publish();

    const auto published_token = flexUI::ApplicationRequestToken{1, created.queue->generation()};
    auto too_early = created.queue->resolve_completion(completion(published_token));
    check_false(static_cast<bool>(too_early));
    check(too_early.error.code == flexUI::ApplicationServiceErrorCode::InvalidState);

    auto request = created.queue->try_receive_request();
    check(request);
    auto resolved = created.queue->resolve_completion(completion(request.request->token));
    check(resolved);
    check_equal(resolved.completion->script_request_id, std::uint64_t{41});
    check_equal(resolved.completion->payload, std::string("result"));

    auto duplicate = created.queue->resolve_completion(completion(request.request->token));
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == flexUI::ApplicationServiceErrorCode::UnknownToken);
    check_equal(created.queue->statistics().current_pending, std::size_t{0});
  }

  it("cancels old requests across generation reset and close") {
    flexUI::ApplicationServiceRequestLimits limits;
    limits.capacity = 3;
    auto created = registered_queue(7, limits);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);

    auto old = engine.reserve(batch_with({command(51), command(52)}));
    check(old);
    old.prepared->publish();
    check(created.queue->try_receive_request());

    auto validated = created.queue->validate_generation_reset(8);
    check(validated);
    created.queue->reset_generation(8);
    check_equal(created.queue->generation(), std::uint64_t{8});
    check_equal(created.queue->statistics().cancelled, std::uint64_t{2});
    auto repeated_generation = created.queue->validate_generation_reset(8);
    check_false(static_cast<bool>(repeated_generation));
    check(repeated_generation.error.code == flexUI::ApplicationServiceErrorCode::InvalidGeneration);

    auto current = engine.reserve(batch_with({command(51)}));
    check(current);
    current.prepared->publish();
    auto request = created.queue->try_receive_request();
    check(request);
    check_equal(request.request->token.generation, std::uint64_t{8});

    auto closed = created.queue->close();
    check(closed);
    check_equal(closed.cancelled, std::size_t{1});
    check(created.queue->try_receive_request().status ==
          flexUI::ApplicationServicePollStatus::Closed);
    auto repeated = created.queue->close();
    check(repeated);
    check_equal(repeated.cancelled, std::size_t{0});
  }

  it("preserves structured service failures while restoring script identity") {
    auto created = registered_queue(9);
    check(created);
    flexUI::ApplicationCommandEngine engine(*created.queue);
    auto prepared = engine.reserve(batch_with({command(61)}));
    check(prepared);
    prepared.prepared->publish();
    auto request = created.queue->try_receive_request();
    check(request);

    auto resolved = created.queue->resolve_completion(
        completion(request.request->token, flexUI::ApplicationCompletionStatus::Failed));
    check(resolved);
    check_equal(resolved.completion->script_request_id, std::uint64_t{61});
    check(resolved.completion->status == flexUI::ApplicationCompletionStatus::Failed);
    check_equal(resolved.completion->error_code, std::string("service-failed"));
    check_equal(resolved.completion->error_message, std::string("service operation failed"));
  }

  it("retains real compiled scope origins through published and in-flight leases") {
    auto created = registered_queue(20);
    check(created);
    if (!created) return;
    flexUI::ApplicationCommandEngine engine(*created.queue);
    const auto root = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\">"
        "<section id=\"sale_host\"/><section id=\"sales_host\"/></main></ui>");
    const auto panel = flexUI::compile_ui_xml(
        "<ui name=\"Panel\" xmlns:on=\"urn:flexui:event\">"
        "<button id=\"action\" on:click=\"issue_request\">Go</button></ui>");
    check(static_cast<bool>(root));
    check(static_cast<bool>(panel));
    if (!root || !panel) return;
    const auto combined = flexUI::compile_scoped_ui_components(
        *root.program,
        {{"sale", "sale_host", panel.program},
         {"sales", "sales_host", panel.program}},
        flexUI::WidgetRegistry::builtins());
    check(static_cast<bool>(combined));
    if (!combined) return;
    flexUI::ScriptController controller(flexUI::ControllerEffects{nullptr, &engine});
    check(controller.load(std::make_unique<ScopedServiceModule>(), combined.program));
    check(controller.mount());

    const auto dispatch_from = [&](std::string id) {
      flexUI::ScriptEventSnapshot event;
      event.event = flexUI::UiEventKind::Click;
      event.target = {std::move(id), 1};
      event.current_target = event.target;
      return controller.dispatch(event);
    };
    check(dispatch_from("sale::action"));
    check(dispatch_from("sales::action"));
    auto sale = created.queue->inspect_scope_leases("sale");
    auto sales = created.queue->inspect_scope_leases("sales");
    check(sale);
    check(sales);
    check_equal(sale.published, std::size_t{1});
    check_equal(sales.published, std::size_t{1});
    check_equal(sale.in_flight, std::size_t{0});
    check_equal(created.queue->inspect_scope_leases("sal").total(), std::size_t{0});

    auto received_sale = created.queue->try_receive_request();
    check(received_sale);
    sale = created.queue->inspect_scope_leases("sale");
    check_equal(sale.in_flight, std::size_t{1});
    check_equal(sale.published, std::size_t{0});
    check_equal(created.queue->inspect_scope_leases("sales").published, std::size_t{1});
    auto received_sales = created.queue->try_receive_request();
    check(received_sales);
    if (!received_sale || !received_sales) return;
    check(created.queue->resolve_completion(completion(received_sale.request->token)));
    check_equal(created.queue->inspect_scope_leases("sale").total(), std::size_t{0});
    check_equal(created.queue->inspect_scope_leases("sales").in_flight, std::size_t{1});

    check(controller.retire_scoped_event_handlers("sales"));
    check(dispatch_from("sales::action")); // route no-op must not issue a new command
    check_equal(created.queue->inspect_scope_leases("sales").in_flight, std::size_t{1});
    check(created.queue->resolve_completion(completion(received_sales.request->token)));
    check_equal(created.queue->inspect_scope_leases("sales").total(), std::size_t{0});

    check(dispatch_from("sale::action"));
    check_equal(created.queue->inspect_scope_leases("sale").published, std::size_t{1});
    created.queue->reset_generation(21);
    check_equal(created.queue->inspect_scope_leases("sale").total(), std::size_t{0});
    check_equal(created.queue->inspect_scope_leases("sales").total(), std::size_t{0});
    flexUI::ApplicationServiceScopeLeases foreign_result;
    std::thread worker([&] { foreign_result = created.queue->inspect_scope_leases("sale"); });
    worker.join();
    check(foreign_result.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(created.queue->inspect_scope_leases("").error.code ==
          flexUI::ApplicationServiceErrorCode::InvalidRequest);
  }

  it("restricts request control and consumption to the owner thread") {
    auto created = registered_queue(10);
    check(created);

    flexUI::ApplicationServiceRequestResult received;
    flexUI::ApplicationServiceControlResult closed;
    std::thread foreign([&] {
      received = created.queue->try_receive_request();
      closed = created.queue->close();
    });
    foreign.join();

    check(received.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(closed.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    check(created.queue->state() == flexUI::ApplicationServiceRequestState::Accepting);
  }
}
