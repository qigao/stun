#include "flexUI/application_service_dispatcher.h"

#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace flexUI {
namespace {

constexpr const char *kBusyCode = "service.submit.busy";
constexpr const char *kClosedCode = "service.submit.closed";
constexpr const char *kInvalidRequestCode = "service.submit.invalid_request";
constexpr const char *kRejectedCode = "service.submit.rejected";
constexpr const char *kInternalFailureCode = "service.submit.internal_failure";
constexpr const char *kExceptionCode = "service.submit.exception";

ApplicationServiceDispatcherError dispatcher_error(
    ApplicationServiceDispatcherErrorCode code, std::string message) {
  ApplicationServiceDispatcherError error;
  error.code = code;
  error.message = std::move(message);
  return error;
}

const char *submit_error_code(ApplicationServiceSubmitErrorCode code) noexcept {
  switch (code) {
  case ApplicationServiceSubmitErrorCode::Busy:
    return kBusyCode;
  case ApplicationServiceSubmitErrorCode::Closed:
    return kClosedCode;
  case ApplicationServiceSubmitErrorCode::InvalidRequest:
    return kInvalidRequestCode;
  case ApplicationServiceSubmitErrorCode::Rejected:
    return kRejectedCode;
  case ApplicationServiceSubmitErrorCode::InternalFailure:
    return kInternalFailureCode;
  case ApplicationServiceSubmitErrorCode::None:
    break;
  }
  return kInternalFailureCode;
}

std::string submit_error_message(const ApplicationServiceSubmitError &error) {
  if (!error.message.empty()) {
    return error.message;
  }
  switch (error.code) {
  case ApplicationServiceSubmitErrorCode::Busy:
    return "service endpoint is busy";
  case ApplicationServiceSubmitErrorCode::Closed:
    return "service endpoint is closed";
  case ApplicationServiceSubmitErrorCode::InvalidRequest:
    return "service endpoint rejected an invalid request";
  case ApplicationServiceSubmitErrorCode::Rejected:
    return "service endpoint rejected the request";
  case ApplicationServiceSubmitErrorCode::InternalFailure:
  case ApplicationServiceSubmitErrorCode::None:
    return "service endpoint submission failed";
  }
  return "service endpoint submission failed";
}

ApplicationCompletion failed_completion(
    const ApplicationServiceRequest &request, const char *error_code,
    std::string error_message) {
  ApplicationCompletion completion;
  completion.token = request.token;
  completion.status = ApplicationCompletionStatus::Failed;
  completion.error_code = error_code;
  completion.error_message = std::move(error_message);
  return completion;
}

} // namespace

struct ApplicationServiceDispatcher::Impl {
  Impl(DesktopApplication &configured_application,
       ApplicationServiceDispatcherLimits configured_limits)
      : application(&configured_application), limits(configured_limits) {}

  DesktopApplication *application = nullptr;
  ApplicationServiceDispatcherLimits limits;
  std::optional<ApplicationCompletion> pending_completion;
};

ApplicationServiceDispatcher::ApplicationServiceDispatcher(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

ApplicationServiceDispatcher::~ApplicationServiceDispatcher() = default;

ApplicationServiceDispatcherCreateResult
ApplicationServiceDispatcher::create(
    DesktopApplication &application,
    ApplicationServiceDispatcherLimits limits) {
  if (limits.max_requests_per_pump == 0 ||
      limits.max_requests_per_pump >
          ApplicationServiceDispatcherLimits::kMaximumRequestsPerPump) {
    return {{},
            dispatcher_error(
                ApplicationServiceDispatcherErrorCode::InvalidLimits,
                "service dispatcher request bound is outside the supported range")};
  }
  if (!application.is_owner_thread()) {
    return {{}, dispatcher_error(
                    ApplicationServiceDispatcherErrorCode::WrongThread,
                    "service dispatcher creation must run on the application owner thread")};
  }
  if (application.state() != DesktopApplicationState::Ready) {
    return {{}, dispatcher_error(
                    ApplicationServiceDispatcherErrorCode::InvalidState,
                    "service dispatcher requires a ready application")};
  }

  try {
    auto impl = std::make_unique<Impl>(application, limits);
    return {std::unique_ptr<ApplicationServiceDispatcher>(
                new ApplicationServiceDispatcher(std::move(impl))),
            {}};
  } catch (const std::bad_alloc &) {
    return {{}, dispatcher_error(
                    ApplicationServiceDispatcherErrorCode::AllocationFailed,
                    "service dispatcher allocation failed")};
  } catch (...) {
    return {{}, dispatcher_error(
                    ApplicationServiceDispatcherErrorCode::InternalInvariant,
                    "service dispatcher creation failed unexpectedly")};
  }
}

ApplicationServiceDispatchResult ApplicationServiceDispatcher::pump() {
  ApplicationServiceDispatchResult result;
  if (impl_->application == nullptr) {
    result.error = dispatcher_error(
        ApplicationServiceDispatcherErrorCode::InternalInvariant,
        "service dispatcher lost its application");
    return result;
  }
  if (!impl_->application->is_owner_thread()) {
    result.error = dispatcher_error(
        ApplicationServiceDispatcherErrorCode::WrongThread,
        "service dispatcher pump must run on the application owner thread");
    return result;
  }
  if (impl_->application->state() != DesktopApplicationState::Ready) {
    impl_->pending_completion.reset();
    result.status = ApplicationServiceDispatchStatus::Closed;
    return result;
  }

  std::size_t processed = 0;
  const auto post_pending = [&]() -> bool {
    const auto posted = impl_->application->completion_mailbox().try_post(
        *impl_->pending_completion);
    if (posted) {
      impl_->pending_completion.reset();
      ++processed;
      ++result.failed;
      result.status = ApplicationServiceDispatchStatus::Progress;
      return true;
    }
    if (posted.error.code == ApplicationCompletionErrorCode::QueueFull) {
      result.status = ApplicationServiceDispatchStatus::Blocked;
      return false;
    }
    result.error = dispatcher_error(
        ApplicationServiceDispatcherErrorCode::CompletionPostFailed,
        "service dispatcher could not publish a terminal completion");
    result.error.completion_error = posted.error;
    return false;
  };

  if (impl_->pending_completion && !post_pending()) {
    return result;
  }

  while (processed < impl_->limits.max_requests_per_pump) {
    auto received = impl_->application->try_receive_service_request();
    if (received.error) {
      result.error = dispatcher_error(
          ApplicationServiceDispatcherErrorCode::RequestReceiveFailed,
          "service dispatcher could not receive an application request");
      result.error.service_error = std::move(received.error);
      return result;
    }
    if (received.status == ApplicationServicePollStatus::Empty) {
      if (processed != 0) {
        result.status = ApplicationServiceDispatchStatus::Progress;
      }
      return result;
    }
    if (received.status == ApplicationServicePollStatus::Closed) {
      result.status = ApplicationServiceDispatchStatus::Closed;
      return result;
    }
    if (!received.request) {
      result.error = dispatcher_error(
          ApplicationServiceDispatcherErrorCode::InternalInvariant,
          "ready service request result did not contain a request");
      return result;
    }

    auto resolved =
        impl_->application->resolve_service_request(*received.request);
    if (!resolved) {
      result.error = dispatcher_error(
          ApplicationServiceDispatcherErrorCode::ResolveFailed,
          "service dispatcher could not resolve an application request");
      result.error.registry_error = std::move(resolved.error);
      return result;
    }

    ApplicationServiceSubmitResult submitted;
    bool endpoint_threw = false;
    try {
      submitted = resolved.endpoint->try_submit(
          *received.request, impl_->application->completion_mailbox());
    } catch (...) {
      endpoint_threw = true;
    }

    if (submitted && !endpoint_threw) {
      ++processed;
      ++result.submitted;
      result.status = ApplicationServiceDispatchStatus::Progress;
      continue;
    }

    try {
      if (endpoint_threw) {
        impl_->pending_completion = failed_completion(
            *received.request, kExceptionCode,
            "service endpoint threw while accepting the request");
      } else {
        impl_->pending_completion = failed_completion(
            *received.request, submit_error_code(submitted.error.code),
            submit_error_message(submitted.error));
      }
    } catch (const std::bad_alloc &) {
      result.error = dispatcher_error(
          ApplicationServiceDispatcherErrorCode::AllocationFailed,
          "service dispatcher could not allocate a terminal completion");
      return result;
    } catch (...) {
      result.error = dispatcher_error(
          ApplicationServiceDispatcherErrorCode::InternalInvariant,
          "service dispatcher could not construct a terminal completion");
      return result;
    }

    if (!post_pending()) {
      return result;
    }
  }

  result.status = ApplicationServiceDispatchStatus::Progress;
  return result;
}

bool ApplicationServiceDispatcher::has_pending_completion() const noexcept {
  return impl_->pending_completion.has_value();
}

} // namespace flexUI
