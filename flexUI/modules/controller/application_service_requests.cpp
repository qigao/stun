#include "application_service_requests.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <type_traits>
#include <utility>

namespace flexUI {
namespace {

enum class RequestSlotState {
  Free,
  Reserved,
  Published,
  InFlight,
};

struct RequestSlot {
  RequestSlotState state = RequestSlotState::Free;
  ApplicationRequestToken token;
  std::uint64_t script_request_id = 0;
  std::string origin_scope;
  std::shared_ptr<ApplicationServiceCancellationLease> cancellation;
  bool suppress_completion = false;
  std::string capability;
  std::string operation;
  std::string payload;
};

static_assert(std::is_nothrow_move_assignable_v<std::string>);

ApplicationServiceError service_fail(ApplicationServiceErrorCode code, std::string message) {
  return {code, std::move(message), {}};
}

ApplicationCommandReserveResult command_fail(ApplicationCommandErrorCode code,
                                             std::size_t command_index, std::string message) {
  return {{}, {code, command_index, std::move(message)}};
}

void release_string(std::string &value) noexcept {
  std::string empty;
  value.swap(empty);
}

void clear_slot(RequestSlot &slot) noexcept {
  // A retained C++ request may outlive terminal consumption or even this
  // queue generation. Mark the SAME authoritative lease stale before removing
  // it, so a reused token cannot start a new worker after scoped JOIN.
  if (slot.cancellation) slot.cancellation->retire();
  slot.cancellation.reset();
  release_string(slot.origin_scope);
  release_string(slot.capability);
  release_string(slot.operation);
  release_string(slot.payload);
  slot.token = {};
  slot.script_request_id = 0;
  slot.suppress_completion = false;
  slot.state = RequestSlotState::Free;
}

} // namespace

struct ApplicationServiceRequestQueue::Impl {
  Impl(std::uint64_t initial_generation, ApplicationServiceRequestLimits configured_limits,
       std::shared_ptr<const ApplicationServiceRegistry> configured_registry,
       ApplicationCapabilityManifest configured_manifest)
      : limits(configured_limits), owner_thread(std::this_thread::get_id()),
        registry(std::move(configured_registry)), manifest(std::move(configured_manifest)),
        generation(initial_generation), slots(configured_limits.capacity),
        published_indices(configured_limits.capacity) {}

  ApplicationServiceRequestLimits limits;
  std::thread::id owner_thread;
  std::shared_ptr<const ApplicationServiceRegistry> registry;
  ApplicationCapabilityManifest manifest;
  std::uint64_t generation = 0;
  std::uint64_t next_token_id = 1;
  ApplicationServiceRequestState state = ApplicationServiceRequestState::Accepting;
  std::vector<RequestSlot> slots;
  std::vector<std::size_t> published_indices;
  std::size_t published_head = 0;
  std::size_t published_tail = 0;
  std::size_t published_count = 0;
  ApplicationServiceStatistics stats;
  // Bounded set of closed compiled-handler origins for this generation. It
  // retains no external host or controller ownership and is reset on reload.
  std::unordered_set<std::string> quiesced_scopes;

  bool is_quiesced(std::string_view scope) const noexcept {
    return std::any_of(quiesced_scopes.begin(), quiesced_scopes.end(),
                       [scope](const std::string &value) {
                         return std::string_view(value) == scope;
                       });
  }

  bool is_owner_thread() const noexcept { return owner_thread == std::this_thread::get_id(); }

  bool has_script_request(std::uint64_t request_id) const noexcept {
    return std::any_of(slots.begin(), slots.end(), [request_id](const RequestSlot &slot) {
      return slot.state != RequestSlotState::Free && slot.script_request_id == request_id;
    });
  }

  bool owns_in_flight_request(const ApplicationServiceRequest &request) const noexcept {
    return std::any_of(slots.begin(), slots.end(), [&request](const RequestSlot &slot) {
      return slot.state == RequestSlotState::InFlight && slot.token.id == request.token.id &&
             slot.token.generation == request.token.generation &&
             slot.script_request_id == request.script_request_id;
    });
  }

  void observe_pending() noexcept {
    stats.peak_pending = std::max(stats.peak_pending, stats.current_pending);
  }

  std::size_t cancel_all() noexcept {
    const auto cancelled = stats.current_pending;
    for (auto &slot : slots) {
      if (slot.state != RequestSlotState::Free) {
        clear_slot(slot);
      }
    }
    published_head = 0;
    published_tail = 0;
    published_count = 0;
    stats.current_pending = 0;
    stats.cancelled += cancelled;
    return cancelled;
  }
};

class PreparedApplicationServiceRequests final : public IPreparedApplicationCommands {
public:
  PreparedApplicationServiceRequests(
      ApplicationServiceRequestQueue &queue,
      std::vector<ApplicationServiceRequestQueue::ReservationKey> reservation)
      : queue_(queue), reservation_(std::move(reservation)) {}

  ~PreparedApplicationServiceRequests() override {
    if (!published_) {
      queue_.discard_reservation(reservation_);
    }
  }

  void publish() noexcept override {
    if (published_) {
      return;
    }
    queue_.publish_reservation(reservation_);
    published_ = true;
  }

private:
  ApplicationServiceRequestQueue &queue_;
  std::vector<ApplicationServiceRequestQueue::ReservationKey> reservation_;
  bool published_ = false;
};

ApplicationServiceRequestQueue::ApplicationServiceRequestQueue(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

ApplicationServiceRequestQueue::~ApplicationServiceRequestQueue() = default;

ApplicationServiceRequestQueueCreateResult
ApplicationServiceRequestQueue::create(std::uint64_t initial_generation,
                                       ApplicationServiceRequestLimits limits,
                                       std::shared_ptr<const ApplicationServiceRegistry> registry,
                                       ApplicationCapabilityManifest manifest) {
  if (initial_generation == 0) {
    return {{},
            service_fail(ApplicationServiceErrorCode::InvalidGeneration,
                         "service request generation must be non-zero")};
  }
  constexpr auto kMaxSlotCount = std::numeric_limits<std::size_t>::max() / sizeof(RequestSlot);
  constexpr auto kMaxIndexCount = std::numeric_limits<std::size_t>::max() / sizeof(std::size_t);
  if (limits.capacity == 0 || limits.capacity > kMaxSlotCount || limits.capacity > kMaxIndexCount) {
    return {{},
            service_fail(ApplicationServiceErrorCode::InvalidCapacity,
                         "service request capacity is invalid")};
  }
  if (!registry) {
    try {
      ApplicationServiceRegistryBuilder empty_builder;
      auto empty_registry = empty_builder.build();
      if (!empty_registry) {
        return {{},
                service_fail(ApplicationServiceErrorCode::AllocationFailed,
                             empty_registry.error.message)};
      }
      registry = std::move(empty_registry.registry);
    } catch (const std::bad_alloc &) {
      return {{},
              service_fail(ApplicationServiceErrorCode::AllocationFailed,
                           "empty service registry allocation failed")};
    } catch (...) {
      return {{},
              service_fail(ApplicationServiceErrorCode::InternalInvariant,
                           "empty service registry construction failed")};
    }
  }
  const auto manifest_result = registry->validate_manifest(manifest);
  if (!manifest_result) {
    return {
        {},
        service_fail(ApplicationServiceErrorCode::InvalidRequest, manifest_result.error.message)};
  }

  try {
    auto impl = std::make_unique<Impl>(initial_generation, limits, std::move(registry),
                                       std::move(manifest));
    return {std::unique_ptr<ApplicationServiceRequestQueue>(
                new ApplicationServiceRequestQueue(std::move(impl))),
            {}};
  } catch (const std::bad_alloc &) {
    return {{},
            service_fail(ApplicationServiceErrorCode::AllocationFailed,
                         "service request storage allocation failed")};
  } catch (const std::length_error &) {
    return {{},
            service_fail(ApplicationServiceErrorCode::InvalidCapacity,
                         "service request capacity exceeds container limits")};
  } catch (...) {
    return {{},
            service_fail(ApplicationServiceErrorCode::InternalInvariant,
                         "service request queue construction failed")};
  }
}

ApplicationCommandReserveResult
ApplicationServiceRequestQueue::reserve(const ApplicationCommandBatch &batch) {
  if (!impl_->is_owner_thread()) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        "service request reservation must run on its owner thread");
  }
  if (impl_->state != ApplicationServiceRequestState::Accepting) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        "service request queue is closed");
  }
  if (batch.empty()) {
    return command_fail(ApplicationCommandErrorCode::EmptyBatch, 0,
                        "service request batch must not be empty");
  }
  if (!batch.origin_scope().empty() && impl_->is_quiesced(batch.origin_scope())) {
    return command_fail(ApplicationCommandErrorCode::ScopeQuiesced, 0,
                        "scoped component service requests are quiesced");
  }

  for (std::size_t index = 0; index < batch.commands().size(); ++index) {
    auto validation = impl_->registry->validate_command(batch.commands()[index], impl_->manifest);
    if (validation) {
      validation.command_index = index;
      return {{}, std::move(validation)};
    }
  }

  if (batch.size() > impl_->limits.capacity - impl_->stats.current_pending) {
    ++impl_->stats.queue_full;
    return command_fail(ApplicationCommandErrorCode::QueueFull, 0, "service request queue is full");
  }

  for (std::size_t index = 0; index < batch.commands().size(); ++index) {
    const auto request_id = batch.commands()[index].request_id;
    bool duplicate = impl_->has_script_request(request_id);
    for (std::size_t earlier = 0; earlier < index && !duplicate; ++earlier) {
      duplicate = batch.commands()[earlier].request_id == request_id;
    }
    if (duplicate) {
      ++impl_->stats.rejected_duplicate;
      return command_fail(ApplicationCommandErrorCode::InvalidRequestId, index,
                          "script request id is already pending");
    }
  }

  const auto count = batch.size();
  const auto max_token = std::numeric_limits<std::uint64_t>::max();
  if (impl_->next_token_id == 0 || count - 1 > max_token - impl_->next_token_id) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        "service request token space is exhausted");
  }

  try {
    std::vector<std::size_t> free_indices;
    free_indices.reserve(count);
    for (std::size_t index = 0; index < impl_->slots.size() && free_indices.size() < count;
         ++index) {
      if (impl_->slots[index].state == RequestSlotState::Free) {
        free_indices.push_back(index);
      }
    }
    if (free_indices.size() != count) {
      return command_fail(ApplicationCommandErrorCode::InternalInvariant, 0,
                          "service request capacity accounting is inconsistent");
    }

    std::vector<ApplicationServiceRequest> requests;
    requests.reserve(count);
    // Stage every provenance allocation before publishing ANY live slot.
    std::vector<std::string> scoped_owners;
    scoped_owners.reserve(count);
    // Every fallible lease allocation occurs before any slot is published.
    std::vector<std::shared_ptr<ApplicationServiceCancellationLease>> cancel_leases;
    cancel_leases.reserve(count);
    std::vector<ReservationKey> reservation;
    reservation.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const auto &command = batch.commands()[index];
      const auto token_id = impl_->next_token_id + index;
      requests.push_back({{token_id, impl_->generation},
                          command.request_id,
                          command.capability,
                          command.operation,
                          command.payload});
      scoped_owners.emplace_back(batch.origin_scope());
      cancel_leases.emplace_back(new ApplicationServiceCancellationLease(
          {token_id, impl_->generation}, scoped_owners.back()));
      reservation.emplace_back(free_indices[index], token_id);
    }

    auto prepared = std::make_unique<PreparedApplicationServiceRequests>(*this, reservation);
    for (std::size_t index = 0; index < count; ++index) {
      auto &slot = impl_->slots[free_indices[index]];
      auto &request = requests[index];
      slot.state = RequestSlotState::Reserved;
      slot.token = request.token;
      slot.script_request_id = request.script_request_id;
      slot.origin_scope = std::move(scoped_owners[index]);
      slot.cancellation = std::move(cancel_leases[index]);
      slot.capability = std::move(request.capability);
      slot.operation = std::move(request.operation);
      slot.payload = std::move(request.payload);
    }
    impl_->next_token_id += count;
    impl_->stats.current_pending += count;
    impl_->observe_pending();
    return {std::move(prepared), {}};
  } catch (const std::bad_alloc &) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        "service request reservation allocation failed");
  } catch (const std::exception &error) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0, error.what());
  } catch (...) {
    return command_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        "service request reservation raised an unknown exception");
  }
}

ApplicationServiceResolveResult
ApplicationServiceRequestQueue::resolve_service(const ApplicationServiceRequest &request) const {
  if (!impl_->is_owner_thread()) {
    return {{},
            {ApplicationServiceRegistryErrorCode::WrongThread, request.capability,
             request.operation, "service resolution must run on the queue owner thread"}};
  }
  if (impl_->state != ApplicationServiceRequestState::Accepting) {
    return {{},
            {ApplicationServiceRegistryErrorCode::InvalidState, request.capability,
             request.operation, "closed service request queue cannot resolve endpoints"}};
  }
  if (!impl_->owns_in_flight_request(request)) {
    return {{},
            {ApplicationServiceRegistryErrorCode::InvalidRequest, request.capability,
             request.operation,
             "service request is not in flight for this application generation"}};
  }
  return impl_->registry->resolve(request, impl_->manifest);
}

void ApplicationServiceRequestQueue::publish_reservation(
    const std::vector<ReservationKey> &reservation) noexcept {
  if (!impl_->is_owner_thread() || impl_->state != ApplicationServiceRequestState::Accepting ||
      reservation.size() > impl_->limits.capacity - impl_->published_count) {
    std::terminate();
  }
  for (const auto &[index, token_id] : reservation) {
    if (index >= impl_->slots.size()) {
      std::terminate();
    }
    const auto &slot = impl_->slots[index];
    if (slot.state != RequestSlotState::Reserved || slot.token.id != token_id) {
      std::terminate();
    }
  }
  for (const auto &[index, token_id] : reservation) {
    static_cast<void>(token_id);
    auto &slot = impl_->slots[index];
    slot.state = RequestSlotState::Published;
    impl_->published_indices[impl_->published_tail] = index;
    impl_->published_tail = (impl_->published_tail + 1) % impl_->limits.capacity;
    ++impl_->published_count;
    ++impl_->stats.published;
  }
}

void ApplicationServiceRequestQueue::discard_reservation(
    const std::vector<ReservationKey> &reservation) noexcept {
  if (!impl_->is_owner_thread()) {
    std::terminate();
  }
  for (const auto &[index, token_id] : reservation) {
    if (index >= impl_->slots.size()) {
      std::terminate();
    }
    auto &slot = impl_->slots[index];
    if (slot.state == RequestSlotState::Reserved && slot.token.id == token_id) {
      clear_slot(slot);
      --impl_->stats.current_pending;
      ++impl_->stats.discarded;
    }
  }
}

ApplicationServiceRequestResult ApplicationServiceRequestQueue::try_receive_request() {
  if (!impl_->is_owner_thread()) {
    return {ApplicationServicePollStatus::Empty,
            {},
            service_fail(ApplicationServiceErrorCode::WrongThread,
                         "service request receive must run on its owner thread")};
  }
  if (impl_->state == ApplicationServiceRequestState::Closed) {
    return {ApplicationServicePollStatus::Closed, {}, {}};
  }
  if (impl_->published_count == 0) {
    return {};
  }

  const auto index = impl_->published_indices[impl_->published_head];
  impl_->published_head = (impl_->published_head + 1) % impl_->limits.capacity;
  --impl_->published_count;
  if (index >= impl_->slots.size()) {
    std::terminate();
  }
  auto &slot = impl_->slots[index];
  if (slot.state != RequestSlotState::Published) {
    std::terminate();
  }

  ApplicationServiceRequest request;
  request.token = slot.token;
  request.script_request_id = slot.script_request_id;
  // Nonallocating shared owner handoff. The queue retains its lease until a
  // REAL terminal completion; the worker may retain a copy independently.
  request.cancellation = slot.cancellation;
  request.capability = std::move(slot.capability);
  request.operation = std::move(slot.operation);
  request.payload = std::move(slot.payload);
  slot.state = RequestSlotState::InFlight;
  ++impl_->stats.dispatched;
  return {ApplicationServicePollStatus::Ready,
          std::optional<ApplicationServiceRequest>(std::move(request)),
          {}};
}

ApplicationServiceCompletionResult
ApplicationServiceRequestQueue::resolve_completion(ApplicationCompletion completion) {
  if (!impl_->is_owner_thread()) {
    return {ApplicationServicePollStatus::Empty,
            {},
            service_fail(ApplicationServiceErrorCode::WrongThread,
                         "service completion resolution must run on its owner thread")};
  }
  if (impl_->state == ApplicationServiceRequestState::Closed) {
    return {ApplicationServicePollStatus::Closed,
            {},
            service_fail(ApplicationServiceErrorCode::Closed, "service request queue is closed")};
  }
  if (!completion.token) {
    return {ApplicationServicePollStatus::Empty,
            {},
            service_fail(ApplicationServiceErrorCode::InvalidRequest,
                         "service completion token is invalid")};
  }

  auto found = std::find_if(
      impl_->slots.begin(), impl_->slots.end(), [&completion](const RequestSlot &slot) {
        return slot.state != RequestSlotState::Free && slot.token.id == completion.token.id &&
               slot.token.generation == completion.token.generation;
      });
  if (found == impl_->slots.end()) {
    ++impl_->stats.rejected_unknown;
    return {ApplicationServicePollStatus::Empty,
            {},
            service_fail(ApplicationServiceErrorCode::UnknownToken,
                         "service completion token is not pending")};
  }
  if (found->state != RequestSlotState::InFlight) {
    return {ApplicationServicePollStatus::Empty,
            {},
            service_fail(ApplicationServiceErrorCode::InvalidState,
                         "service completion arrived before request dispatch")};
  }

  // Preserve the host token until its real completion is received. Suppressed
  // is distinct from Empty: callers must keep draining already-woken mailboxes
  // so later, unrelated completions are not stranded.
  if (found->suppress_completion) {
    clear_slot(*found);
    --impl_->stats.current_pending;
    ++impl_->stats.completed;
    ++impl_->stats.suppressed_completions;
    return {ApplicationServicePollStatus::Suppressed, {}, {}};
  }

  ApplicationServiceCompletion resolved;
  resolved.script_request_id = found->script_request_id;
  resolved.status = completion.status;
  resolved.payload = std::move(completion.payload);
  resolved.error_code = std::move(completion.error_code);
  resolved.error_message = std::move(completion.error_message);
  clear_slot(*found);
  --impl_->stats.current_pending;
  ++impl_->stats.completed;
  return {ApplicationServicePollStatus::Ready,
          std::optional<ApplicationServiceCompletion>(std::move(resolved)),
          {}};
}

ApplicationServiceScopeLeases ApplicationServiceRequestQueue::inspect_scope_leases(
    std::string_view scope) const {
  ApplicationServiceScopeLeases leases;
  if (!impl_->is_owner_thread()) {
    leases.error = service_fail(ApplicationServiceErrorCode::WrongThread,
                                "scope lease inspection requires the owner thread");
    return leases;
  }
  if (impl_->state != ApplicationServiceRequestState::Accepting) {
    leases.error = service_fail(ApplicationServiceErrorCode::Closed,
                                "closed service queue cannot inspect scope leases");
    return leases;
  }
  if (scope.empty()) {
    leases.error = service_fail(ApplicationServiceErrorCode::InvalidRequest,
                                "scope lease inspection requires a nonempty scope");
    return leases;
  }
  leases.quiesced = impl_->is_quiesced(scope);
  for (const auto &slot : impl_->slots) {
    if (std::string_view(slot.origin_scope) != scope) continue;
    switch (slot.state) {
    case RequestSlotState::Reserved: ++leases.reserved; break;
    case RequestSlotState::Published: ++leases.published; break;
    case RequestSlotState::InFlight: ++leases.in_flight; break;
    case RequestSlotState::Free: break;
    }
  }
  return leases;
}

ApplicationServiceScopeQuiesceResult ApplicationServiceRequestQueue::quiesce_scope_requests(
    std::string_view scope) {
  ApplicationServiceScopeQuiesceResult result;
  if (!impl_->is_owner_thread()) {
    result.error = service_fail(ApplicationServiceErrorCode::WrongThread,
                                "scope quiescence requires the owner thread");
    return result;
  }
  if (impl_->state != ApplicationServiceRequestState::Accepting) {
    result.error = service_fail(ApplicationServiceErrorCode::Closed,
                                "closed service queue cannot quiesce a scope");
    return result;
  }
  // Match the compiler's public component-scope grammar exactly, instead
  // of letting an internal caller create impossible admission tombstones.
  const auto is_start = [](char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') || ch == '_';
  };
  const auto is_rest = [&](char ch) noexcept {
    return is_start(ch) || (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-';
  };
  if (scope.empty() || scope.size() > 253 || !is_start(scope.front()) ||
      !std::all_of(scope.begin() + 1, scope.end(), is_rest)) {
    result.error = service_fail(ApplicationServiceErrorCode::InvalidRequest,
                                "scope quiescence requires a compiler-valid ASCII scope");
    return result;
  }
  if (impl_->is_quiesced(scope)) return result; // Stable and idempotent.

  // PreparedApplicationServiceRequests may still own a reservation. Never
  // detach those slots: its later noexcept publish/discard contract depends on
  // their identity. Reject without changing routes, queue or admission gate.
  for (const auto &slot : impl_->slots) {
    if (slot.state == RequestSlotState::Reserved &&
        std::string_view(slot.origin_scope) == scope) {
      result.error = service_fail(ApplicationServiceErrorCode::InvalidState,
                                  "scope still has a prepared command reservation");
      return result;
    }
  }
  // Every in-flight slot must retain its generation-exact cancellation lease.
  // Fail before any staged scope publication on a corrupted lease invariant.
  for (const auto &slot : impl_->slots) {
    if (slot.state == RequestSlotState::InFlight &&
        std::string_view(slot.origin_scope) == scope &&
        (!slot.cancellation ||
         slot.cancellation->token().id != slot.token.id ||
         slot.cancellation->token().generation != slot.token.generation)) {
      result.error = service_fail(ApplicationServiceErrorCode::InternalInvariant,
                                  "in-flight scoped request lost its cancellation lease");
      return result;
    }
  }
  // Validate the ENTIRE published FIFO before making any change; the commit
  // below only performs noexcept slot clear, flag writes and ring compaction.
  for (std::size_t n = 0; n < impl_->published_count; ++n) {
    const auto index = impl_->published_indices[
        (impl_->published_head + n) % impl_->limits.capacity];
    if (index >= impl_->slots.size() ||
        impl_->slots[index].state != RequestSlotState::Published) {
      result.error = service_fail(ApplicationServiceErrorCode::InternalInvariant,
                                  "published FIFO is inconsistent");
      return result;
    }
  }
  try {
    // Stage every fallible allocation before publishing the scoped gate.
    // The bound prevents untrusted callers growing the origin tombstone set
    // indefinitely in a single generation (there is no secondary table).
    if (impl_->quiesced_scopes.size() >= 256) {
      result.error = service_fail(ApplicationServiceErrorCode::InvalidState,
                                  "scoped quiescence capacity is exhausted");
      return result;
    }
    auto next_quiesced = impl_->quiesced_scopes;
    next_quiesced.emplace(scope);
    impl_->quiesced_scopes.swap(next_quiesced);
  } catch (const std::exception &) {
    result.error = service_fail(ApplicationServiceErrorCode::AllocationFailed,
                                "failed to stage scope quiescence");
    return result;
  } catch (...) {
    result.error = service_fail(ApplicationServiceErrorCode::AllocationFailed,
                                "unexpected scope quiescence staging failure");
    return result;
  }

  for (auto &slot : impl_->slots) {
    if (slot.state == RequestSlotState::InFlight &&
        std::string_view(slot.origin_scope) == scope) {
      slot.suppress_completion = true;
      // This sets a cooperative stop signal only. Do NOT claim worker
      // termination, release the queue token, or synthesize a completion.
      slot.cancellation->request_cancel();
      ++result.suppressed_in_flight;
    }
  }

  const std::size_t old_count = impl_->published_count;
  std::size_t kept = 0;
  for (std::size_t n = 0; n < old_count; ++n) {
    const auto read_index = (impl_->published_head + n) % impl_->limits.capacity;
    const auto slot_index = impl_->published_indices[read_index];
    auto &slot = impl_->slots[slot_index];
    if (std::string_view(slot.origin_scope) == scope) {
      clear_slot(slot);
      --impl_->stats.current_pending;
      ++impl_->stats.cancelled;
      ++result.cancelled_published;
    } else {
      // Reading forward and writing only into earlier FIFO positions makes
      // this stable in-place compaction safe even across ring wraparound.
      impl_->published_indices[(impl_->published_head + kept) % impl_->limits.capacity] =
          slot_index;
      ++kept;
    }
  }
  impl_->published_count = kept;
  impl_->published_tail = (impl_->published_head + kept) % impl_->limits.capacity;
  return result;
}

ApplicationScopedRegistryJoinResult
ApplicationServiceRequestQueue::join_scoped_resources(
    std::string_view scope, std::chrono::milliseconds timeout) {
  const auto leases = inspect_scope_leases(scope);
  if (!leases || !leases.quiesced || leases.total() != 0) {
    return {0, {ApplicationServiceRegistryErrorCode::InvalidState, {}, {},
                 "scope has not quiesced or still owns service tokens"}};
  }
  if (timeout.count() < 0) {
    return {0, {ApplicationServiceRegistryErrorCode::InvalidState, {}, {},
                 "scope endpoint join timeout must be nonnegative"}};
  }
  return impl_->registry->join_scoped_resources(scope, timeout);
}

ApplicationServiceControlResult
ApplicationServiceRequestQueue::validate_generation_reset(std::uint64_t next_generation) const {
  if (!impl_->is_owner_thread()) {
    return {0, service_fail(ApplicationServiceErrorCode::WrongThread,
                            "service generation reset must run on its owner thread")};
  }
  if (impl_->state != ApplicationServiceRequestState::Accepting) {
    return {0, service_fail(ApplicationServiceErrorCode::Closed,
                            "closed service request queue cannot reset generation")};
  }
  if (next_generation == 0 || next_generation <= impl_->generation) {
    return {0, service_fail(ApplicationServiceErrorCode::InvalidGeneration,
                            "service generation must increase monotonically")};
  }
  return {};
}

void ApplicationServiceRequestQueue::reset_generation(std::uint64_t next_generation) noexcept {
  if (!validate_generation_reset(next_generation)) {
    std::terminate();
  }
  for (auto &slot : impl_->slots) {
    if (slot.cancellation) slot.cancellation->request_cancel();
  }
  static_cast<void>(impl_->cancel_all());
  impl_->quiesced_scopes.clear();
  impl_->generation = next_generation;
}

ApplicationServiceControlResult ApplicationServiceRequestQueue::close() {
  if (!impl_->is_owner_thread()) {
    return {0, service_fail(ApplicationServiceErrorCode::WrongThread,
                            "service request close must run on its owner thread")};
  }
  if (impl_->state == ApplicationServiceRequestState::Closed) {
    return {};
  }
  for (auto &slot : impl_->slots) {
    if (slot.cancellation) slot.cancellation->request_cancel();
  }
  impl_->state = ApplicationServiceRequestState::Closed;
  return {impl_->cancel_all(), {}};
}

std::uint64_t ApplicationServiceRequestQueue::generation() const noexcept {
  return impl_->generation;
}

std::size_t ApplicationServiceRequestQueue::capacity() const noexcept {
  return impl_->limits.capacity;
}

ApplicationServiceRequestState ApplicationServiceRequestQueue::state() const noexcept {
  return impl_->state;
}

ApplicationServiceStatistics ApplicationServiceRequestQueue::statistics() const noexcept {
  return impl_->stats;
}

} // namespace flexUI
