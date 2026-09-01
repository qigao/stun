#pragma once

#include "flexUI/application.h"

#include <cstddef>
#include <memory>
#include <string>

namespace flexUI {

struct ApplicationServiceDispatcherLimits {
  static constexpr std::size_t kDefaultRequestsPerPump = 64;
  static constexpr std::size_t kMaximumRequestsPerPump = 4096;

  /// Bounds owner-thread work performed by one pump call.
  std::size_t max_requests_per_pump = kDefaultRequestsPerPump;
};

enum class ApplicationServiceDispatchStatus {
  Idle,
  Progress,
  Blocked,
  Closed,
};

enum class ApplicationServiceDispatcherErrorCode {
  None,
  InvalidLimits,
  WrongThread,
  InvalidState,
  RequestReceiveFailed,
  ResolveFailed,
  CompletionPostFailed,
  AllocationFailed,
  InternalInvariant,
};

struct ApplicationServiceDispatcherError {
  ApplicationServiceDispatcherErrorCode code =
      ApplicationServiceDispatcherErrorCode::None;
  std::string message;
  ApplicationServiceError service_error;
  ApplicationServiceRegistryError registry_error;
  ApplicationCompletionError completion_error;

  explicit operator bool() const noexcept {
    return code != ApplicationServiceDispatcherErrorCode::None;
  }
};

struct ApplicationServiceDispatchResult {
  ApplicationServiceDispatchStatus status =
      ApplicationServiceDispatchStatus::Idle;
  std::size_t submitted = 0;
  std::size_t failed = 0;
  ApplicationServiceDispatcherError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

class ApplicationServiceDispatcher;

struct ApplicationServiceDispatcherCreateResult {
  std::unique_ptr<ApplicationServiceDispatcher> dispatcher;
  ApplicationServiceDispatcherError error;

  explicit operator bool() const noexcept {
    return dispatcher != nullptr && !static_cast<bool>(error);
  }
};

/// Owner-thread bridge from application commands to registered endpoints.
///
/// Accepted endpoints own one completion attempt. Endpoint rejection or an
/// exception is converted to exactly one failed completion. If the completion
/// mailbox is full, the dispatcher retains at most that one terminal record
/// and retries it before consuming another request.
class ApplicationServiceDispatcher final {
public:
  static ApplicationServiceDispatcherCreateResult
  create(DesktopApplication &application,
         ApplicationServiceDispatcherLimits limits = {});

  ~ApplicationServiceDispatcher();

  ApplicationServiceDispatcher(const ApplicationServiceDispatcher &) = delete;
  ApplicationServiceDispatcher &
  operator=(const ApplicationServiceDispatcher &) = delete;
  ApplicationServiceDispatcher(ApplicationServiceDispatcher &&) = delete;
  ApplicationServiceDispatcher &
  operator=(ApplicationServiceDispatcher &&) = delete;

  /// Submits at most max_requests_per_pump units of owner-thread work.
  /// @return Blocked only while a synthesized terminal completion awaits
  ///         mailbox capacity; Closed after application close; or a structured
  ///         fatal boundary error that the host must consume.
  ApplicationServiceDispatchResult pump();

  /// True while mailbox backpressure prevents consuming another request.
  bool has_pending_completion() const noexcept;

private:
  struct Impl;
  explicit ApplicationServiceDispatcher(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
