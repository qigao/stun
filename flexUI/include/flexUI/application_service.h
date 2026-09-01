#pragma once

#include "flexUI/application_completion.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace flexUI {

/// Owning service command transferred from the application owner to the host.
struct ApplicationServiceRequest {
  ApplicationRequestToken token;
  std::uint64_t script_request_id = 0;
  std::string capability;
  std::string operation;
  std::string payload;
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
