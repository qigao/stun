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

  it("invalidates retained cancellation leases after normal terminal consumption") {
    auto created = registered_queue(17);
    check(created);
    if (!created) return;
    flexUI::ApplicationCommandEngine engine(*created.queue);
    auto queued = engine.reserve(batch_with({command(170)}));
    check(queued);
    if (!queued) return;
    queued.prepared->publish();
    auto received = created.queue->try_receive_request();
    check(received);
    if (!received) return;
    auto retained = received.request->cancellation;
    check_not_null(retained.get());
    if (!retained) return;
    check_false(retained->requested());
    check_false(retained->retired());
    const auto completed = created.queue->resolve_completion(
        completion(received.request->token));
    check(completed);
    check(completed.status == flexUI::ApplicationServicePollStatus::Ready);
    check_false(retained->requested());
    check(retained->retired());
    check_equal(created.queue->statistics().current_pending, std::size_t{0});
    check(created.queue->state() == flexUI::ApplicationServiceRequestState::Accepting);
    // Retiring the token does not invalidate the typed request values held
    // by the application, but must prevent reusing its lifetime authority.
    check_equal(retained->token().id, received.request->token.id);
    check_equal(retained->token().generation, std::uint64_t{17});
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
    auto old_request = created.queue->try_receive_request();
    check(old_request);
    if (!old_request) return;
    const auto old_lease = old_request.request->cancellation;
    check_not_null(old_lease.get());
    if (!old_lease) return;
    check_equal(old_lease->token().id, old_request.request->token.id);
    check_equal(old_lease->token().generation, std::uint64_t{7});
    check_false(old_lease->requested());

    auto validated = created.queue->validate_generation_reset(8);
    check(validated);
    created.queue->reset_generation(8);
    // Generation replacement signals cooperative stop to live native owners.
    // No terminal completion is synthesized; the old token remains stale.
    check(old_lease->requested());
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
    if (!request) return;
    check_equal(request.request->token.generation, std::uint64_t{8});
    const auto current_lease = request.request->cancellation;
    check_not_null(current_lease.get());
    if (!current_lease) return;
    check_false(current_lease->requested());

    auto closed = created.queue->close();
    check(closed);
    check(current_lease->requested());
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

  it("quiesces queued scope commands and suppresses real in-flight completions") {
    flexUI::ApplicationServiceRequestLimits limits;
    limits.capacity = 4;
    auto created = registered_queue(30, limits);
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
    const auto dispatch_scope = [&](std::string scope) {
      flexUI::ScriptEventSnapshot event;
      event.event = flexUI::UiEventKind::Click;
      event.target = {std::move(scope) + "::action", 1};
      event.current_target = event.target;
      return controller.dispatch(event);
    };
    check(dispatch_scope("sale")); // token 1, later in flight
    check(dispatch_scope("sales")); // token 2, completed to open a ring slot
    check(dispatch_scope("sale")); // token 3, queued and cancellable
    check(dispatch_scope("sales")); // token 4, must remain in FIFO
    auto first = created.queue->try_receive_request();
    auto second = created.queue->try_receive_request();
    check(first);
    check(second);
    if (!first || !second) return;
    check_equal(first.request->token.id, std::uint64_t{1});
    check_equal(second.request->token.id, std::uint64_t{2});
    const auto sale_lease = first.request->cancellation;
    const auto other_lease = second.request->cancellation;
    check_not_null(sale_lease.get());
    check_not_null(other_lease.get());
    if (!sale_lease || !other_lease) return;
    check_equal(sale_lease->token().id, first.request->token.id);
    check_equal(sale_lease->token().generation, first.request->token.generation);
    check_false(sale_lease->requested());
    check_false(other_lease->requested());
    check(created.queue->resolve_completion(completion(second.request->token)));
    check_false(other_lease->requested()); // Normal success is not cancellation.
    check(dispatch_scope("sale")); // token 5, published after ring wrap
    const auto before = created.queue->inspect_scope_leases("sale");
    check_equal(before.published, std::size_t{2});
    check_equal(before.in_flight, std::size_t{1});

    flexUI::ApplicationServiceScopeQuiesceResult foreign;
    std::thread worker([&] { foreign = created.queue->quiesce_scope_requests("sale"); });
    worker.join();
    check(foreign.error.code == flexUI::ApplicationServiceErrorCode::WrongThread);
    for (const char *invalid_scope : {"", "sale::action", "sale.", "2sale"}) {
      const auto invalid = created.queue->quiesce_scope_requests(invalid_scope);
      check_false(static_cast<bool>(invalid));
      check(invalid.error.code == flexUI::ApplicationServiceErrorCode::InvalidRequest);
    }
    check_equal(created.queue->inspect_scope_leases("sale").total(), std::size_t{3});

    check(controller.retire_scoped_event_handlers("sale"));
    const auto quiesced = created.queue->quiesce_scope_requests("sale");
    check(quiesced);
    check_equal(quiesced.cancelled_published, std::size_t{2});
    check_equal(quiesced.suppressed_in_flight, std::size_t{1});
    check(sale_lease->requested());
    check_false(other_lease->requested());
    // The signal is cross-thread observable but the worker decides when its
    // own operation actually ends and whether to post Cancelled or Succeeded.
    bool worker_observed = false;
    std::thread worker_probe([&] { worker_observed = sale_lease->requested(); });
    worker_probe.join();
    check(worker_observed);
    const auto repeated = created.queue->quiesce_scope_requests("sale");
    check(repeated);
    check_equal(repeated.cancelled_published, std::size_t{0});
    check_equal(repeated.suppressed_in_flight, std::size_t{0});
    const auto sale = created.queue->inspect_scope_leases("sale");
    check(sale.quiesced);
    check_equal(sale.published, std::size_t{0});
    check_equal(sale.in_flight, std::size_t{1});
    check_equal(created.queue->inspect_scope_leases("sales").published, std::size_t{1});
    check_equal(created.queue->statistics().current_pending, std::size_t{2});
    check_equal(created.queue->statistics().cancelled, std::uint64_t{2});
    check(dispatch_scope("sale")); // retired route cannot produce new command
    check_equal(created.queue->statistics().current_pending, std::size_t{2});

    auto sibling = created.queue->try_receive_request();
    check(sibling);
    if (!sibling) return;
    check_equal(sibling.request->token.id, std::uint64_t{4});
    check_not_null(sibling.request->cancellation.get());
    if (!sibling.request->cancellation) return;
    check_false(sibling.request->cancellation->requested());
    check(created.queue->try_receive_request().status ==
          flexUI::ApplicationServicePollStatus::Empty);
    auto suppressed = created.queue->resolve_completion(completion(first.request->token));
    check_false(static_cast<bool>(suppressed));
    check_false(static_cast<bool>(suppressed.error));
    check(suppressed.status == flexUI::ApplicationServicePollStatus::Suppressed);
    check_false(suppressed.completion.has_value());
    check_equal(created.queue->inspect_scope_leases("sale").total(), std::size_t{0});
    auto delivered = created.queue->resolve_completion(completion(sibling.request->token));
    check(delivered);
    check_equal(delivered.completion->script_request_id, std::uint64_t{1003});
    check_equal(created.queue->statistics().suppressed_completions, std::uint64_t{1});
    check_equal(created.queue->statistics().current_pending, std::size_t{0});
    check_equal(created.queue->statistics().completed, std::uint64_t{3});

    // A new application generation must clear the scoped admission tombstone.
    created.queue->reset_generation(31);
    check_false(created.queue->inspect_scope_leases("sale").quiesced);
    check(controller.unmount());
    check(controller.load(std::make_unique<ScopedServiceModule>(), combined.program));
    check(controller.mount());
    check(dispatch_scope("sale"));
    check_equal(created.queue->inspect_scope_leases("sale").published, std::size_t{1});
  }

  it("rejects new commands from a quiesced scope but preserves global commands") {
    auto created = registered_queue(40);
    check(created);
    if (!created) return;
    flexUI::ApplicationCommandEngine engine(*created.queue);
    auto quiesced = created.queue->quiesce_scope_requests("sale");
    check(quiesced);
    check(created.queue->inspect_scope_leases("sale").quiesced);
    auto global = engine.reserve(batch_with({command(9001)}));
    check(global);
    if (!global) return;
    check_equal(created.queue->inspect_scope_leases("sale").total(), std::size_t{0});
    global.prepared->publish();
    auto queued = created.queue->try_receive_request();
    check(queued);
    if (!queued) return;
    check_equal(queued.request->script_request_id, std::uint64_t{9001});
    auto normal = created.queue->resolve_completion(completion(queued.request->token));
    check(normal);
    check_equal(normal.completion->script_request_id, std::uint64_t{9001});
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
