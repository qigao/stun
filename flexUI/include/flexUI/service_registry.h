#pragma once

#include "flexUI/application_command.h"
#include "flexUI/application_completion.h"
#include "flexUI/application_service.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flexUI {

enum class ApplicationServiceSubmitErrorCode {
  None,
  Busy,
  Closed,
  InvalidRequest,
  Rejected,
  InternalFailure,
};

struct ApplicationServiceSubmitError {
  ApplicationServiceSubmitErrorCode code = ApplicationServiceSubmitErrorCode::None;
  std::string message;

  explicit operator bool() const noexcept {
    return code != ApplicationServiceSubmitErrorCode::None;
  }
};

struct ApplicationServiceSubmitResult {
  ApplicationServiceSubmitError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Lifetime-safe C++ endpoint. On success the endpoint has copied every value
/// it retains and owns exactly one completion attempt for request.token. On
/// failure it retains no request/sink reference and posts no completion.
class IApplicationServiceEndpoint {
public:
  virtual ~IApplicationServiceEndpoint() = default;
  virtual ApplicationServiceSubmitResult
  try_submit(const ApplicationServiceRequest &request,
             IApplicationServiceCompletionSink &completion_sink) = 0;
};

struct ApplicationServiceOperationDescriptor {
  static constexpr std::size_t kDefaultMaxPayloadBytes =
      ApplicationCommandLimits::kDefaultMaxStringBytes;

  std::string name;
  std::size_t max_payload_bytes = kDefaultMaxPayloadBytes;
};

struct ApplicationServiceDescriptor {
  std::string capability;
  std::vector<ApplicationServiceOperationDescriptor> operations;
};

struct ApplicationCapabilityManifest {
  std::vector<std::string> allowed;
  std::vector<std::string> required;
};

struct ApplicationServiceRegistryLimits {
  static constexpr std::size_t kDefaultMaxServices = 64;
  static constexpr std::size_t kDefaultMaxOperationsPerService = 64;
  static constexpr std::size_t kDefaultMaxIdentifierBytes = 256;
  static constexpr std::size_t kDefaultMaxPayloadBytes =
      ApplicationCommandLimits::kDefaultMaxTotalStringBytes;

  std::size_t max_services = kDefaultMaxServices;
  std::size_t max_operations_per_service = kDefaultMaxOperationsPerService;
  std::size_t max_identifier_bytes = kDefaultMaxIdentifierBytes;
  std::size_t max_payload_bytes = kDefaultMaxPayloadBytes;
};

enum class ApplicationServiceRegistryErrorCode {
  None,
  InvalidLimits,
  InvalidCapability,
  InvalidOperation,
  InvalidPayloadLimit,
  ServiceLimitExceeded,
  OperationLimitExceeded,
  StringLimitExceeded,
  DuplicateCapability,
  DuplicateOperation,
  MissingEndpoint,
  InvalidManifest,
  MissingRequiredCapability,
  UnauthorizedCapability,
  UnknownCapability,
  UnsupportedOperation,
  InvalidPayload,
  InvalidRequest,
  InvalidState,
  WrongThread,
  AllocationFailed,
  InternalInvariant,
};

struct ApplicationServiceRegistryError {
  ApplicationServiceRegistryErrorCode code = ApplicationServiceRegistryErrorCode::None;
  std::string capability;
  std::string operation;
  std::string message;

  explicit operator bool() const noexcept {
    return code != ApplicationServiceRegistryErrorCode::None;
  }
};

struct ApplicationServiceRegistryResult {
  ApplicationServiceRegistryError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

class ApplicationServiceRegistry;

struct ApplicationServiceRegistryBuildResult {
  std::shared_ptr<const ApplicationServiceRegistry> registry;
  ApplicationServiceRegistryError error;

  explicit operator bool() const noexcept {
    return registry != nullptr && !static_cast<bool>(error);
  }
};

struct ApplicationServiceResolveResult {
  std::shared_ptr<IApplicationServiceEndpoint> endpoint;
  ApplicationServiceRegistryError error;

  explicit operator bool() const noexcept {
    return endpoint != nullptr && !static_cast<bool>(error);
  }
};

/// Mutable, owner-thread-only registry construction surface.
class ApplicationServiceRegistryBuilder final {
public:
  explicit ApplicationServiceRegistryBuilder(ApplicationServiceRegistryLimits limits = {});
  ~ApplicationServiceRegistryBuilder();

  ApplicationServiceRegistryBuilder(const ApplicationServiceRegistryBuilder &) = delete;
  ApplicationServiceRegistryBuilder &operator=(const ApplicationServiceRegistryBuilder &) = delete;
  ApplicationServiceRegistryBuilder(ApplicationServiceRegistryBuilder &&) = delete;
  ApplicationServiceRegistryBuilder &operator=(ApplicationServiceRegistryBuilder &&) = delete;

  ApplicationServiceRegistryResult
  register_service(ApplicationServiceDescriptor descriptor,
                   std::shared_ptr<IApplicationServiceEndpoint> endpoint);
  ApplicationServiceRegistryBuildResult build() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Immutable application-scoped service descriptor and endpoint snapshot.
class ApplicationServiceRegistry final {
public:
  ~ApplicationServiceRegistry();

  ApplicationServiceRegistry(const ApplicationServiceRegistry &) = delete;
  ApplicationServiceRegistry &operator=(const ApplicationServiceRegistry &) = delete;
  ApplicationServiceRegistry(ApplicationServiceRegistry &&) = delete;
  ApplicationServiceRegistry &operator=(ApplicationServiceRegistry &&) = delete;

  ApplicationServiceRegistryResult
  validate_manifest(const ApplicationCapabilityManifest &manifest) const;
  ApplicationCommandError validate_command(const ApplicationCommand &command,
                                           const ApplicationCapabilityManifest &manifest) const;
  ApplicationServiceResolveResult resolve(const ApplicationServiceRequest &request,
                                          const ApplicationCapabilityManifest &manifest) const;
  bool contains(std::string_view capability) const noexcept;
  std::size_t size() const noexcept;

private:
  friend class ApplicationServiceRegistryBuilder;
  struct Impl;
  explicit ApplicationServiceRegistry(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
