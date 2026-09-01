#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace flexUI {

/// Host-issued identity echoed by a service completion. A token belongs to
/// exactly one published application generation.
struct ApplicationRequestToken {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;

  explicit operator bool() const noexcept { return id != 0 && generation != 0; }
};

enum class ApplicationCompletionStatus {
  Succeeded,
  Failed,
  Cancelled,
};

/// Owning value copied or moved across the worker-to-UI boundary.
struct ApplicationCompletion {
  ApplicationRequestToken token;
  ApplicationCompletionStatus status = ApplicationCompletionStatus::Succeeded;
  std::string payload;
  std::string error_code;
  std::string error_message;
};

struct ApplicationCompletionLimits {
  static constexpr std::uint64_t kDefaultCapacity = 256;
  static constexpr std::size_t kDefaultMaxPayloadBytes = 64 * 1024;
  static constexpr std::size_t kDefaultMaxErrorCodeBytes = 256;
  static constexpr std::size_t kDefaultMaxErrorMessageBytes = 16 * 1024;
  static constexpr std::size_t kDefaultMaxTotalStringBytes = 64 * 1024;

  std::uint64_t capacity = kDefaultCapacity;
  std::size_t max_payload_bytes = kDefaultMaxPayloadBytes;
  std::size_t max_error_code_bytes = kDefaultMaxErrorCodeBytes;
  std::size_t max_error_message_bytes = kDefaultMaxErrorMessageBytes;
  std::size_t max_total_string_bytes = kDefaultMaxTotalStringBytes;
};

using ApplicationCompletionWakeupFn = void (*)(void *context) noexcept;

/// Allocation-free notification invoked after a completion is published.
///
/// The callback and context are borrowed until close() has quiesced all
/// producers. Notifications carry no data; consumers must still poll the
/// mailbox on its owner thread.
struct ApplicationCompletionWakeup {
  ApplicationCompletionWakeupFn callback = nullptr;
  void *context = nullptr;

  explicit operator bool() const noexcept { return callback != nullptr; }
};

enum class ApplicationCompletionErrorCode {
  None,
  InvalidCapacity,
  InvalidGeneration,
  InvalidToken,
  InvalidCompletion,
  StringLimitExceeded,
  QueueFull,
  Closed,
  StaleGeneration,
  WrongThread,
  AllocationFailed,
  InternalInvariant,
  InvalidWakeup,
};

struct ApplicationCompletionError {
  ApplicationCompletionErrorCode code = ApplicationCompletionErrorCode::None;
  std::string message;

  explicit operator bool() const noexcept { return code != ApplicationCompletionErrorCode::None; }
};

struct ApplicationCompletionPostResult {
  ApplicationCompletionError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Non-owning completion boundary supplied to an accepted service request.
/// Implementations define their own thread-safety and lifetime contract.
class IApplicationServiceCompletionSink {
public:
  virtual ~IApplicationServiceCompletionSink() = default;
  virtual ApplicationCompletionPostResult
  try_post(const ApplicationCompletion &completion) = 0;
};

enum class ApplicationCompletionReceiveStatus {
  Ready,
  Empty,
  Closed,
};

struct ApplicationCompletionReceiveResult {
  ApplicationCompletionReceiveStatus status = ApplicationCompletionReceiveStatus::Empty;
  std::optional<ApplicationCompletion> completion;
  ApplicationCompletionError error;

  explicit operator bool() const noexcept {
    return status == ApplicationCompletionReceiveStatus::Ready && completion.has_value() &&
           !static_cast<bool>(error);
  }
};

struct ApplicationCompletionControlResult {
  std::uint64_t cancelled = 0;
  ApplicationCompletionError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

enum class ApplicationCompletionMailboxState {
  Accepting,
  AdvancingGeneration,
  Closing,
  Closed,
};

struct ApplicationCompletionStatistics {
  std::uint64_t current_depth = 0;
  std::uint64_t peak_depth = 0;
  std::uint64_t published = 0;
  std::uint64_t consumed = 0;
  std::uint64_t cancelled = 0;
  std::uint64_t queue_full = 0;
  std::uint64_t rejected_closed = 0;
  std::uint64_t rejected_stale = 0;
  std::uint64_t rejected_invalid = 0;
};

struct ApplicationCompletionMailboxCreateResult;

/// Fixed-capacity multi-producer/single-consumer completion queue.
///
/// try_post() may run on service worker threads and never blocks on capacity.
/// try_receive(), close(), and advance_generation() belong to the thread that
/// created the mailbox. The owner must stop and join all producers before
/// destroying the mailbox object.
class ApplicationCompletionMailbox final
    : public IApplicationServiceCompletionSink {
public:
  static ApplicationCompletionMailboxCreateResult create(std::uint64_t initial_generation,
                                                         ApplicationCompletionLimits limits = {},
                                                         ApplicationCompletionWakeup wakeup = {});

  ~ApplicationCompletionMailbox();

  ApplicationCompletionMailbox(const ApplicationCompletionMailbox &) = delete;
  ApplicationCompletionMailbox &operator=(const ApplicationCompletionMailbox &) = delete;
  ApplicationCompletionMailbox(ApplicationCompletionMailbox &&) = delete;
  ApplicationCompletionMailbox &operator=(ApplicationCompletionMailbox &&) = delete;

  /// Copies a complete record into queue ownership on success. Queue-full,
  /// closed, stale, allocation, and validation failures leave the source
  /// unchanged and retain no mailbox object.
  ApplicationCompletionPostResult
  try_post(const ApplicationCompletion &completion) override;

  /// Claims at most one completion on the owner thread.
  ApplicationCompletionReceiveResult try_receive();

  /// Cancels the current generation and resumes with a strictly newer one.
  ApplicationCompletionControlResult advance_generation(std::uint64_t next_generation);

  /// Stops producers and cancels all published records. Owner-thread and
  /// idempotent; destruction still requires producers to be joined.
  ApplicationCompletionControlResult close();

  std::uint64_t generation() const noexcept;
  std::uint64_t capacity() const noexcept;
  ApplicationCompletionMailboxState state() const noexcept;
  ApplicationCompletionStatistics statistics() const noexcept;

private:
  struct Impl;
  explicit ApplicationCompletionMailbox(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

struct ApplicationCompletionMailboxCreateResult {
  std::unique_ptr<ApplicationCompletionMailbox> mailbox;
  ApplicationCompletionError error;

  explicit operator bool() const noexcept {
    return mailbox != nullptr && !static_cast<bool>(error);
  }
};

} // namespace flexUI
