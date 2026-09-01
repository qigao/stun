#pragma once

#include "flexUI/application_command.h"
#include "flexUI/application_service.h"
#include "flexUI/service_registry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace flexUI {

enum class ApplicationServiceRequestState {
  Accepting,
  Closed,
};

struct ApplicationServiceControlResult {
  std::size_t cancelled = 0;
  ApplicationServiceError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

struct ApplicationServiceRequestQueueCreateResult;
class PreparedApplicationServiceRequests;

/// Owner-thread command queue and pending-token fact source.
class ApplicationServiceRequestQueue final : public IApplicationCommandQueue {
public:
  static ApplicationServiceRequestQueueCreateResult
  create(std::uint64_t initial_generation,
         ApplicationServiceRequestLimits limits = {},
         std::shared_ptr<const ApplicationServiceRegistry> registry = {},
         ApplicationCapabilityManifest manifest = {});

  ~ApplicationServiceRequestQueue() override;

  ApplicationServiceRequestQueue(const ApplicationServiceRequestQueue &) = delete;
  ApplicationServiceRequestQueue &operator=(const ApplicationServiceRequestQueue &) = delete;
  ApplicationServiceRequestQueue(ApplicationServiceRequestQueue &&) = delete;
  ApplicationServiceRequestQueue &operator=(ApplicationServiceRequestQueue &&) = delete;

  ApplicationCommandReserveResult reserve(const ApplicationCommandBatch &batch) override;

  ApplicationServiceRequestResult try_receive_request();
  ApplicationServiceResolveResult
  resolve_service(const ApplicationServiceRequest &request) const;
  ApplicationServiceCompletionResult resolve_completion(ApplicationCompletion completion);

  /// Validates the fallible part before the mailbox generation changes.
  ApplicationServiceControlResult validate_generation_reset(std::uint64_t next_generation) const;
  /// Commits a previously validated reset without allocation or failure.
  void reset_generation(std::uint64_t next_generation) noexcept;
  ApplicationServiceControlResult close();

  std::uint64_t generation() const noexcept;
  std::size_t capacity() const noexcept;
  ApplicationServiceRequestState state() const noexcept;
  ApplicationServiceStatistics statistics() const noexcept;

private:
  using ReservationKey = std::pair<std::size_t, std::uint64_t>;

  struct Impl;
  explicit ApplicationServiceRequestQueue(std::unique_ptr<Impl> impl);

  void publish_reservation(const std::vector<ReservationKey> &reservation) noexcept;
  void discard_reservation(const std::vector<ReservationKey> &reservation) noexcept;

  std::unique_ptr<Impl> impl_;

  friend class PreparedApplicationServiceRequests;
};

struct ApplicationServiceRequestQueueCreateResult {
  std::unique_ptr<ApplicationServiceRequestQueue> queue;
  ApplicationServiceError error;

  explicit operator bool() const noexcept { return queue != nullptr && !static_cast<bool>(error); }
};

} // namespace flexUI
