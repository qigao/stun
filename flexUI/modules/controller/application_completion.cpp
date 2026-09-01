#include "flexUI/application_completion.h"

#include <turbo/disruptor.h>

#include <atomic>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <thread>
#include <utility>

namespace flexUI {
namespace {

struct CompletionEntry {
  ApplicationCompletion *value = nullptr;
};

ApplicationCompletionError fail(ApplicationCompletionErrorCode code, std::string message) {
  return {code, std::move(message)};
}

bool is_power_of_two(std::uint64_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

ApplicationCompletionError validate_limits(const ApplicationCompletionLimits &limits) {
  if (!is_power_of_two(limits.capacity)) {
    return fail(ApplicationCompletionErrorCode::InvalidCapacity,
                "completion mailbox capacity must be a non-zero power of two");
  }
  constexpr auto kMaxSlotCount = std::numeric_limits<std::size_t>::max() / sizeof(CompletionEntry);
  if (limits.capacity > kMaxSlotCount) {
    return fail(ApplicationCompletionErrorCode::InvalidCapacity,
                "completion mailbox slot storage size would overflow");
  }
  return {};
}

ApplicationCompletionError validate_completion(const ApplicationCompletion &completion,
                                               const ApplicationCompletionLimits &limits) {
  if (!completion.token) {
    return fail(ApplicationCompletionErrorCode::InvalidToken,
                "completion token id and generation must be non-zero");
  }

  switch (completion.status) {
  case ApplicationCompletionStatus::Succeeded:
    if (!completion.error_code.empty() || !completion.error_message.empty()) {
      return fail(ApplicationCompletionErrorCode::InvalidCompletion,
                  "successful completion cannot carry error fields");
    }
    break;
  case ApplicationCompletionStatus::Failed:
    if (completion.error_code.empty()) {
      return fail(ApplicationCompletionErrorCode::InvalidCompletion,
                  "failed completion requires a non-empty error code");
    }
    break;
  case ApplicationCompletionStatus::Cancelled:
    if (!completion.payload.empty() || !completion.error_code.empty()) {
      return fail(ApplicationCompletionErrorCode::InvalidCompletion,
                  "cancelled completion cannot carry payload or error code");
    }
    break;
  default:
    return fail(ApplicationCompletionErrorCode::InvalidCompletion, "completion status is invalid");
  }

  if (completion.payload.size() > limits.max_payload_bytes ||
      completion.error_code.size() > limits.max_error_code_bytes ||
      completion.error_message.size() > limits.max_error_message_bytes) {
    return fail(ApplicationCompletionErrorCode::StringLimitExceeded,
                "completion string exceeds its configured byte limit");
  }

  std::size_t total = completion.payload.size();
  if (completion.error_code.size() > limits.max_total_string_bytes ||
      total > limits.max_total_string_bytes - completion.error_code.size()) {
    return fail(ApplicationCompletionErrorCode::StringLimitExceeded,
                "completion strings exceed the configured total byte limit");
  }
  total += completion.error_code.size();
  if (completion.error_message.size() > limits.max_total_string_bytes ||
      total > limits.max_total_string_bytes - completion.error_message.size()) {
    return fail(ApplicationCompletionErrorCode::StringLimitExceeded,
                "completion strings exceed the configured total byte limit");
  }
  return {};
}

class ActivePublisher final {
public:
  explicit ActivePublisher(std::atomic<std::uint64_t> &active) noexcept : active_(active) {
    active_.fetch_add(1, std::memory_order_seq_cst);
  }

  ~ActivePublisher() { active_.fetch_sub(1, std::memory_order_seq_cst); }

  ActivePublisher(const ActivePublisher &) = delete;
  ActivePublisher &operator=(const ActivePublisher &) = delete;

private:
  std::atomic<std::uint64_t> &active_;
};

} // namespace

struct ApplicationCompletionMailbox::Impl {
  explicit Impl(disruptor_t *configured_queue, ApplicationCompletionLimits configured_limits,
                std::uint64_t initial_generation,
                ApplicationCompletionWakeup configured_wakeup)
      : queue(configured_queue), limits(configured_limits),
        wakeup(configured_wakeup), owner_thread(std::this_thread::get_id()),
        generation(initial_generation) {}

  disruptor_t *queue = nullptr;
  ApplicationCompletionLimits limits;
  ApplicationCompletionWakeup wakeup;
  std::thread::id owner_thread;
  std::atomic<ApplicationCompletionMailboxState> state{
      ApplicationCompletionMailboxState::Accepting};
  std::atomic<std::uint64_t> generation{0};
  std::atomic<std::uint64_t> active_publishers{0};
  std::atomic<std::uint64_t> current_depth{0};
  std::atomic<std::uint64_t> peak_depth{0};
  std::atomic<std::uint64_t> published{0};
  std::atomic<std::uint64_t> consumed{0};
  std::atomic<std::uint64_t> cancelled{0};
  std::atomic<std::uint64_t> queue_full{0};
  std::atomic<std::uint64_t> rejected_closed{0};
  std::atomic<std::uint64_t> rejected_stale{0};
  std::atomic<std::uint64_t> rejected_invalid{0};

  bool is_owner_thread() const noexcept { return owner_thread == std::this_thread::get_id(); }

  void wait_for_publishers() const noexcept {
    while (active_publishers.load(std::memory_order_seq_cst) != 0) {
      std::this_thread::yield();
    }
  }

  void observe_depth(std::uint64_t depth) noexcept {
    auto peak = peak_depth.load(std::memory_order_relaxed);
    while (peak < depth && !peak_depth.compare_exchange_weak(peak, depth, std::memory_order_relaxed,
                                                             std::memory_order_relaxed)) {
    }
  }

  std::uint64_t drain() noexcept {
    std::uint64_t drained = 0;
    disruptor_cursor_t cursor{};
    while (disruptor_worker_try_claim(queue, &cursor) != 0) {
      auto *entry = static_cast<const CompletionEntry *>(disruptor_show_entry(queue, &cursor));
      std::unique_ptr<ApplicationCompletion> completion(entry == nullptr ? nullptr : entry->value);
      if (entry == nullptr || completion == nullptr) {
        std::terminate();
      }
      const_cast<CompletionEntry *>(entry)->value = nullptr;
      disruptor_worker_release_entry(queue, &cursor);
      ++drained;
      current_depth.fetch_sub(1, std::memory_order_relaxed);
    }
    cancelled.fetch_add(drained, std::memory_order_relaxed);
    return drained;
  }
};

ApplicationCompletionMailbox::ApplicationCompletionMailbox(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

ApplicationCompletionMailbox::~ApplicationCompletionMailbox() {
  if (impl_ == nullptr) {
    return;
  }
  impl_->state.store(ApplicationCompletionMailboxState::Closing, std::memory_order_seq_cst);
  impl_->wait_for_publishers();
  static_cast<void>(impl_->drain());
  impl_->state.store(ApplicationCompletionMailboxState::Closed, std::memory_order_seq_cst);
  disruptor_destroy(impl_->queue);
  impl_->queue = nullptr;
}

ApplicationCompletionMailboxCreateResult
ApplicationCompletionMailbox::create(std::uint64_t initial_generation,
                                     ApplicationCompletionLimits limits,
                                     ApplicationCompletionWakeup wakeup) {
  if (initial_generation == 0) {
    return {{},
            fail(ApplicationCompletionErrorCode::InvalidGeneration,
                 "completion mailbox generation must be non-zero")};
  }
  if (auto error = validate_limits(limits)) {
    return {{}, std::move(error)};
  }
  if (wakeup.callback == nullptr && wakeup.context != nullptr) {
    return {{},
            fail(ApplicationCompletionErrorCode::InvalidWakeup,
                 "completion wakeup context requires a callback")};
  }

  const disruptor_config_t config = {sizeof(CompletionEntry), limits.capacity, 1,
                                     DISRUPTOR_MODE_WORKER_POOL};
  disruptor_t *queue = disruptor_create(&config);
  if (queue == nullptr) {
    return {{},
            fail(ApplicationCompletionErrorCode::AllocationFailed,
                 "completion mailbox allocation failed")};
  }

  try {
    auto impl = std::make_unique<Impl>(queue, limits, initial_generation,
                                      wakeup);
    return {std::unique_ptr<ApplicationCompletionMailbox>(
                new ApplicationCompletionMailbox(std::move(impl))),
            {}};
  } catch (const std::bad_alloc &) {
    disruptor_destroy(queue);
    return {{},
            fail(ApplicationCompletionErrorCode::AllocationFailed,
                 "completion mailbox object allocation failed")};
  } catch (...) {
    disruptor_destroy(queue);
    return {{},
            fail(ApplicationCompletionErrorCode::InternalInvariant,
                 "completion mailbox construction failed")};
  }
}

ApplicationCompletionPostResult
ApplicationCompletionMailbox::try_post(const ApplicationCompletion &completion) {
  ActivePublisher publishing(impl_->active_publishers);
  const auto state = impl_->state.load(std::memory_order_seq_cst);
  if (state == ApplicationCompletionMailboxState::Closing ||
      state == ApplicationCompletionMailboxState::Closed) {
    impl_->rejected_closed.fetch_add(1, std::memory_order_relaxed);
    return {fail(ApplicationCompletionErrorCode::Closed, "completion mailbox is closed")};
  }
  if (state == ApplicationCompletionMailboxState::AdvancingGeneration) {
    impl_->rejected_stale.fetch_add(1, std::memory_order_relaxed);
    return {fail(ApplicationCompletionErrorCode::StaleGeneration,
                 "completion generation is being retired")};
  }

  const auto current_generation = impl_->generation.load(std::memory_order_acquire);
  if (!completion.token) {
    impl_->rejected_invalid.fetch_add(1, std::memory_order_relaxed);
    return {fail(ApplicationCompletionErrorCode::InvalidToken,
                 "completion token id and generation must be non-zero")};
  }
  if (completion.token.generation != current_generation) {
    impl_->rejected_stale.fetch_add(1, std::memory_order_relaxed);
    return {fail(ApplicationCompletionErrorCode::StaleGeneration,
                 "completion token does not belong to the active generation")};
  }
  if (auto error = validate_completion(completion, impl_->limits)) {
    impl_->rejected_invalid.fetch_add(1, std::memory_order_relaxed);
    return {std::move(error)};
  }

  std::unique_ptr<ApplicationCompletion> owned;
  try {
    owned = std::make_unique<ApplicationCompletion>(completion);
  } catch (const std::bad_alloc &) {
    return {fail(ApplicationCompletionErrorCode::AllocationFailed,
                 "completion record allocation failed")};
  }

  disruptor_cursor_t cursor{};
  if (disruptor_publisher_try_claim(impl_->queue, &cursor) == 0) {
    impl_->queue_full.fetch_add(1, std::memory_order_relaxed);
    return {fail(ApplicationCompletionErrorCode::QueueFull, "completion mailbox is full")};
  }

  auto *entry = static_cast<CompletionEntry *>(disruptor_acquire_entry(impl_->queue, &cursor));
  if (entry == nullptr) {
    std::terminate();
  }
  entry->value = owned.release();
  const auto depth = impl_->current_depth.fetch_add(1, std::memory_order_relaxed) + 1;
  impl_->published.fetch_add(1, std::memory_order_relaxed);
  impl_->observe_depth(depth);
  if (disruptor_publisher_publish(impl_->queue, &cursor) == 0) {
    std::terminate();
  }
  if (impl_->wakeup) {
    impl_->wakeup.callback(impl_->wakeup.context);
  }
  return {};
}

ApplicationCompletionReceiveResult ApplicationCompletionMailbox::try_receive() {
  if (!impl_->is_owner_thread()) {
    return {ApplicationCompletionReceiveStatus::Empty,
            {},
            fail(ApplicationCompletionErrorCode::WrongThread,
                 "completion mailbox receive must run on its owner thread")};
  }
  if (impl_->state.load(std::memory_order_acquire) !=
      ApplicationCompletionMailboxState::Accepting) {
    return {ApplicationCompletionReceiveStatus::Closed, {}, {}};
  }

  disruptor_cursor_t cursor{};
  if (disruptor_worker_try_claim(impl_->queue, &cursor) == 0) {
    return {};
  }
  auto *entry = const_cast<CompletionEntry *>(
      static_cast<const CompletionEntry *>(disruptor_show_entry(impl_->queue, &cursor)));
  if (entry == nullptr || entry->value == nullptr) {
    disruptor_worker_release_entry(impl_->queue, &cursor);
    return {ApplicationCompletionReceiveStatus::Empty,
            {},
            fail(ApplicationCompletionErrorCode::InternalInvariant,
                 "completion mailbox exposed an empty published slot")};
  }

  std::unique_ptr<ApplicationCompletion> owned(entry->value);
  entry->value = nullptr;
  disruptor_worker_release_entry(impl_->queue, &cursor);
  impl_->current_depth.fetch_sub(1, std::memory_order_relaxed);
  impl_->consumed.fetch_add(1, std::memory_order_relaxed);
  return {ApplicationCompletionReceiveStatus::Ready,
          std::optional<ApplicationCompletion>(std::move(*owned)),
          {}};
}

ApplicationCompletionControlResult
ApplicationCompletionMailbox::advance_generation(std::uint64_t next_generation) {
  if (!impl_->is_owner_thread()) {
    return {0, fail(ApplicationCompletionErrorCode::WrongThread,
                    "completion generation advance must run on its owner thread")};
  }
  const auto state = impl_->state.load(std::memory_order_acquire);
  if (state != ApplicationCompletionMailboxState::Accepting) {
    return {0, fail(ApplicationCompletionErrorCode::Closed,
                    "closed completion mailbox cannot advance generation")};
  }
  const auto current_generation = impl_->generation.load(std::memory_order_acquire);
  if (next_generation == 0 || next_generation <= current_generation) {
    return {0, fail(ApplicationCompletionErrorCode::InvalidGeneration,
                    "completion generation must increase monotonically")};
  }

  impl_->state.store(ApplicationCompletionMailboxState::AdvancingGeneration,
                     std::memory_order_seq_cst);
  impl_->wait_for_publishers();
  const auto drained = impl_->drain();
  impl_->generation.store(next_generation, std::memory_order_release);
  impl_->state.store(ApplicationCompletionMailboxState::Accepting, std::memory_order_seq_cst);
  return {drained, {}};
}

ApplicationCompletionControlResult ApplicationCompletionMailbox::close() {
  if (!impl_->is_owner_thread()) {
    return {0, fail(ApplicationCompletionErrorCode::WrongThread,
                    "completion mailbox close must run on its owner thread")};
  }
  if (impl_->state.load(std::memory_order_acquire) == ApplicationCompletionMailboxState::Closed) {
    return {};
  }

  impl_->state.store(ApplicationCompletionMailboxState::Closing, std::memory_order_seq_cst);
  impl_->wait_for_publishers();
  const auto drained = impl_->drain();
  impl_->state.store(ApplicationCompletionMailboxState::Closed, std::memory_order_seq_cst);
  return {drained, {}};
}

std::uint64_t ApplicationCompletionMailbox::generation() const noexcept {
  return impl_->generation.load(std::memory_order_acquire);
}

std::uint64_t ApplicationCompletionMailbox::capacity() const noexcept {
  return impl_->limits.capacity;
}

ApplicationCompletionMailboxState ApplicationCompletionMailbox::state() const noexcept {
  return impl_->state.load(std::memory_order_acquire);
}

ApplicationCompletionStatistics ApplicationCompletionMailbox::statistics() const noexcept {
  return {
      impl_->current_depth.load(std::memory_order_relaxed),
      impl_->peak_depth.load(std::memory_order_relaxed),
      impl_->published.load(std::memory_order_relaxed),
      impl_->consumed.load(std::memory_order_relaxed),
      impl_->cancelled.load(std::memory_order_relaxed),
      impl_->queue_full.load(std::memory_order_relaxed),
      impl_->rejected_closed.load(std::memory_order_relaxed),
      impl_->rejected_stale.load(std::memory_order_relaxed),
      impl_->rejected_invalid.load(std::memory_order_relaxed),
  };
}

} // namespace flexUI
