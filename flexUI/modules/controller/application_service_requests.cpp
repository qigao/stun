#include "application_service_requests.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
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
  release_string(slot.capability);
  release_string(slot.operation);
  release_string(slot.payload);
  slot.token = {};
  slot.script_request_id = 0;
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
      reservation.emplace_back(free_indices[index], token_id);
    }

    auto prepared = std::make_unique<PreparedApplicationServiceRequests>(*this, reservation);
    for (std::size_t index = 0; index < count; ++index) {
      auto &slot = impl_->slots[free_indices[index]];
      auto &request = requests[index];
      slot.state = RequestSlotState::Reserved;
      slot.token = request.token;
      slot.script_request_id = request.script_request_id;
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
  static_cast<void>(impl_->cancel_all());
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
