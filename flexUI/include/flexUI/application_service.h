#pragma once

#include "flexUI/application_completion.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace flexUI {

/// Exact-token cancellation signal for a native C++ service request.
/// The owner queue creates/signals the lease; endpoints receive only const
/// shared ownership and MAY stop cooperatively after checking requested().
/// This is a request, NEVER a terminal completion or worker-join proof.
/// A worker must still post its actual terminal status and release resources.
class ApplicationServiceCancellationLease final {
public:
  ~ApplicationServiceCancellationLease() = default;
  ApplicationServiceCancellationLease(const ApplicationServiceCancellationLease &) = delete;
  ApplicationServiceCancellationLease &operator=(const ApplicationServiceCancellationLease &) = delete;

  ApplicationRequestToken token() const noexcept { return token_; }
  /// Trusted immutable compiler scope from the one owning request queue.
  std::string_view origin_scope() const noexcept { return origin_scope_; }
  bool requested() const noexcept {
    return requested_.load(std::memory_order_acquire);
  }
  /// True once the sole queue has discarded or CONSUMED this exact token.
  /// This is not a cancellation request and may coexist with requested()==false.
  bool retired() const noexcept {
    return retired_.load(std::memory_order_acquire);
  }
  /// Queue-owned terminal/discard publication; no external C++ endpoint has
  /// mutable access because it receives shared_ptr<const ...> only.
  void retire() noexcept {
    retired_.store(true, std::memory_order_release);
  }

private:
  explicit ApplicationServiceCancellationLease(ApplicationRequestToken token,
                                               std::string scope)
      : token_(token), origin_scope_(std::move(scope)) {}
  void request_cancel() noexcept {
    requested_.store(true, std::memory_order_release);
  }
  ApplicationRequestToken token_;
  const std::string origin_scope_;
  std::atomic<bool> requested_{false};
  std::atomic<bool> retired_{false};
  friend class ApplicationServiceRequestQueue;
};

/// Owning service command transferred from the application owner to the host.
struct ApplicationServiceRequest {
  ApplicationRequestToken token;
  std::uint64_t script_request_id = 0;
  std::string capability;
  std::string operation;
  std::string payload;
  /// Per-request, generation-bound cooperative stop request. Native endpoints
  /// may retain this const lease only while their own work is live. The sole
  /// C plugin ABI v2 exposes a token-scoped stop QUERY from PluginHost: no C++
  /// shared_ptr or native lease pointer crosses a DLL boundary.
  std::shared_ptr<const ApplicationServiceCancellationLease> cancellation{};
};

/// Owning completion restored to the script request identity on the UI thread.
struct ApplicationServiceCompletion {
  std::uint64_t script_request_id = 0;
  ApplicationCompletionStatus status = ApplicationCompletionStatus::Succeeded;
  std::string payload;
  std::string error_code;
  std::string error_message;
};

struct ApplicationServiceRequestLimits {
  static constexpr std::size_t kDefaultCapacity = 256;

  /// Counts reserved, published, and in-flight requests together.
  std::size_t capacity = kDefaultCapacity;
};

enum class ApplicationServicePollStatus {
  Ready,
  Empty,
  Closed,
  /// One accepted host completion was consumed but intentionally not delivered
  /// to an application callback after scoped request quiescence.
  Suppressed,
};

enum class ApplicationServiceErrorCode {
  None,
  InvalidCapacity,
  InvalidGeneration,
  InvalidRequest,
  DuplicateRequestId,
  QueueFull,
  UnknownToken,
  InvalidState,
  Closed,
  WrongThread,
  GenerationExhausted,
  AllocationFailed,
  CompletionMailboxFailed,
  InternalInvariant,
};

struct ApplicationServiceError {
  ApplicationServiceErrorCode code = ApplicationServiceErrorCode::None;
  std::string message;
  ApplicationCompletionError completion_error;

  explicit operator bool() const noexcept { return code != ApplicationServiceErrorCode::None; }
};

struct ApplicationServiceStatistics {
  std::size_t current_pending = 0;
  std::size_t peak_pending = 0;
  std::uint64_t published = 0;
  std::uint64_t dispatched = 0;
  std::uint64_t completed = 0;
  std::uint64_t cancelled = 0;
  std::uint64_t discarded = 0;
  std::uint64_t queue_full = 0;
  std::uint64_t rejected_duplicate = 0;
  std::uint64_t rejected_unknown = 0;
  std::uint64_t suppressed_completions = 0;
};

struct ApplicationServiceRequestResult {
  ApplicationServicePollStatus status = ApplicationServicePollStatus::Empty;
  std::optional<ApplicationServiceRequest> request;
  ApplicationServiceError error;

  explicit operator bool() const noexcept {
    return status == ApplicationServicePollStatus::Ready && request.has_value() &&
           !static_cast<bool>(error);
  }
};

struct ApplicationServiceCompletionResult {
  ApplicationServicePollStatus status = ApplicationServicePollStatus::Empty;
  std::optional<ApplicationServiceCompletion> completion;
  ApplicationServiceError error;

  explicit operator bool() const noexcept {
    return status == ApplicationServicePollStatus::Ready && completion.has_value() &&
           !static_cast<bool>(error);
  }
};

} // namespace flexUI
