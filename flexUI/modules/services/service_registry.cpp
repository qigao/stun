#include "flexUI/service_registry.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace flexUI {
namespace {

struct ServiceEntry {
  ApplicationServiceDescriptor descriptor;
  std::shared_ptr<IApplicationServiceEndpoint> endpoint;
};

ApplicationServiceRegistryError registry_error(ApplicationServiceRegistryErrorCode code,
                                               std::string message, std::string capability = {},
                                               std::string operation = {}) {
  return {code, std::move(capability), std::move(operation), std::move(message)};
}

bool valid_limits(const ApplicationServiceRegistryLimits &limits) noexcept {
  return limits.max_services != 0 && limits.max_operations_per_service != 0 &&
         limits.max_identifier_bytes != 0 && limits.max_payload_bytes != 0;
}

bool is_namespace_character(char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_' ||
         value == '-';
}

bool is_canonical_capability(std::string_view capability) noexcept {
  const auto slash = capability.find('/');
  if (slash == std::string_view::npos || slash == 0 || slash + 1 >= capability.size() ||
      capability.find('/', slash + 1) != std::string_view::npos) {
    return false;
  }

  bool segment_has_character = false;
  for (std::size_t index = 0; index < slash; ++index) {
    const char value = capability[index];
    if (value == '.') {
      if (!segment_has_character) {
        return false;
      }
      segment_has_character = false;
      continue;
    }
    if (!is_namespace_character(value)) {
      return false;
    }
    segment_has_character = true;
  }
  if (!segment_has_character) {
    return false;
  }

  const auto major = capability.substr(slash + 1);
  if (major.front() < '1' || major.front() > '9') {
    return false;
  }
  std::uint32_t parsed = 0;
  for (const char value : major) {
    if (value < '0' || value > '9') {
      return false;
    }
    const auto digit = static_cast<std::uint32_t>(value - '0');
    if (parsed > (std::numeric_limits<std::uint32_t>::max() - digit) / std::uint32_t{10}) {
      return false;
    }
    parsed = parsed * std::uint32_t{10} + digit;
  }
  return parsed != 0;
}

bool is_canonical_operation(std::string_view operation) noexcept {
  if (operation.empty() || operation.front() < 'a' || operation.front() > 'z') {
    return false;
  }
  return std::all_of(operation.begin() + 1, operation.end(), [](char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '.' ||
           value == '_' || value == '-';
  });
}

const ServiceEntry *find_service(const std::vector<ServiceEntry> &entries,
                                 std::string_view capability) noexcept {
  const auto found =
      std::find_if(entries.begin(), entries.end(), [capability](const ServiceEntry &entry) {
        return entry.descriptor.capability == capability;
      });
  return found == entries.end() ? nullptr : &*found;
}

const ApplicationServiceOperationDescriptor *find_operation(const ServiceEntry &entry,
                                                            std::string_view operation) noexcept {
  const auto found =
      std::find_if(entry.descriptor.operations.begin(), entry.descriptor.operations.end(),
                   [operation](const ApplicationServiceOperationDescriptor &candidate) {
                     return candidate.name == operation;
                   });
  return found == entry.descriptor.operations.end() ? nullptr : &*found;
}

bool contains_capability(const std::vector<std::string> &capabilities,
                         std::string_view capability) noexcept {
  return std::any_of(
      capabilities.begin(), capabilities.end(),
      [capability](const std::string &candidate) { return candidate == capability; });
}

ApplicationCommandError command_error(ApplicationCommandErrorCode code, std::string message) {
  return {code, 0, std::move(message)};
}

} // namespace

struct ApplicationServiceRegistryBuilder::Impl {
  explicit Impl(ApplicationServiceRegistryLimits configured_limits)
      : limits(configured_limits), owner_thread(std::this_thread::get_id()) {}

  ApplicationServiceRegistryLimits limits;
  std::thread::id owner_thread;
  std::vector<ServiceEntry> entries;
};

struct ApplicationServiceRegistry::Impl {
  Impl(ApplicationServiceRegistryLimits configured_limits,
       std::vector<ServiceEntry> configured_entries)
      : limits(configured_limits), entries(std::move(configured_entries)) {}

  ApplicationServiceRegistryLimits limits;
  std::vector<ServiceEntry> entries;
};

ApplicationServiceRegistryBuilder::ApplicationServiceRegistryBuilder(
    ApplicationServiceRegistryLimits limits)
    : impl_(std::make_unique<Impl>(limits)) {}

ApplicationServiceRegistryBuilder::~ApplicationServiceRegistryBuilder() = default;

ApplicationServiceRegistryResult ApplicationServiceRegistryBuilder::register_service(
    ApplicationServiceDescriptor descriptor,
    std::shared_ptr<IApplicationServiceEndpoint> endpoint) {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {registry_error(ApplicationServiceRegistryErrorCode::WrongThread,
                           "service registration belongs to the builder owner thread")};
  }
  if (!valid_limits(impl_->limits)) {
    return {registry_error(ApplicationServiceRegistryErrorCode::InvalidLimits,
                           "all service registry limits must be non-zero")};
  }
  if (!endpoint) {
    return {registry_error(ApplicationServiceRegistryErrorCode::MissingEndpoint,
                           "service endpoint is required", descriptor.capability)};
  }
  if (descriptor.capability.size() > impl_->limits.max_identifier_bytes) {
    return {registry_error(ApplicationServiceRegistryErrorCode::StringLimitExceeded,
                           "capability exceeds the configured identifier limit",
                           descriptor.capability)};
  }
  if (!is_canonical_capability(descriptor.capability)) {
    return {registry_error(ApplicationServiceRegistryErrorCode::InvalidCapability,
                           "capability must use canonical <namespace>/<major> syntax",
                           descriptor.capability)};
  }
  if (descriptor.operations.empty()) {
    return {registry_error(ApplicationServiceRegistryErrorCode::InvalidOperation,
                           "a service must declare at least one operation", descriptor.capability)};
  }
  if (descriptor.operations.size() > impl_->limits.max_operations_per_service) {
    return {registry_error(ApplicationServiceRegistryErrorCode::OperationLimitExceeded,
                           "service exceeds the configured operation limit",
                           descriptor.capability)};
  }

  for (std::size_t index = 0; index < descriptor.operations.size(); ++index) {
    const auto &operation = descriptor.operations[index];
    if (operation.name.size() > impl_->limits.max_identifier_bytes) {
      return {registry_error(ApplicationServiceRegistryErrorCode::StringLimitExceeded,
                             "operation exceeds the configured identifier limit",
                             descriptor.capability, operation.name)};
    }
    if (!is_canonical_operation(operation.name)) {
      return {registry_error(ApplicationServiceRegistryErrorCode::InvalidOperation,
                             "operation name is not canonical", descriptor.capability,
                             operation.name)};
    }
    if (operation.max_payload_bytes == 0 ||
        operation.max_payload_bytes > impl_->limits.max_payload_bytes) {
      return {registry_error(ApplicationServiceRegistryErrorCode::InvalidPayloadLimit,
                             "operation payload limit is outside the registry bounds",
                             descriptor.capability, operation.name)};
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (descriptor.operations[prior].name == operation.name) {
        return {registry_error(ApplicationServiceRegistryErrorCode::DuplicateOperation,
                               "service declares the operation more than once",
                               descriptor.capability, operation.name)};
      }
    }
  }

  if (find_service(impl_->entries, descriptor.capability) != nullptr) {
    return {registry_error(ApplicationServiceRegistryErrorCode::DuplicateCapability,
                           "capability is already registered", descriptor.capability)};
  }
  if (impl_->entries.size() >= impl_->limits.max_services) {
    return {registry_error(ApplicationServiceRegistryErrorCode::ServiceLimitExceeded,
                           "service registry has reached its configured capacity",
                           descriptor.capability)};
  }

  try {
    impl_->entries.push_back({std::move(descriptor), std::move(endpoint)});
  } catch (const std::bad_alloc &) {
    return {registry_error(ApplicationServiceRegistryErrorCode::AllocationFailed,
                           "service registration allocation failed")};
  } catch (...) {
    return {registry_error(ApplicationServiceRegistryErrorCode::InternalInvariant,
                           "service registration failed unexpectedly")};
  }
  return {};
}

ApplicationServiceRegistryBuildResult ApplicationServiceRegistryBuilder::build() const {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::WrongThread,
                           "registry build belongs to the builder owner thread")};
  }
  if (!valid_limits(impl_->limits)) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InvalidLimits,
                           "all service registry limits must be non-zero")};
  }

  try {
    auto snapshot_impl =
        std::make_unique<ApplicationServiceRegistry::Impl>(impl_->limits, impl_->entries);
    auto *snapshot = new ApplicationServiceRegistry(std::move(snapshot_impl));
    return {std::shared_ptr<const ApplicationServiceRegistry>(snapshot), {}};
  } catch (const std::bad_alloc &) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::AllocationFailed,
                           "service registry snapshot allocation failed")};
  } catch (...) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InternalInvariant,
                           "service registry snapshot failed unexpectedly")};
  }
}

ApplicationServiceRegistry::ApplicationServiceRegistry(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

ApplicationServiceRegistry::~ApplicationServiceRegistry() = default;

ApplicationServiceRegistryResult
ApplicationServiceRegistry::validate_manifest(const ApplicationCapabilityManifest &manifest) const {
  if (manifest.allowed.size() > impl_->limits.max_services ||
      manifest.required.size() > impl_->limits.max_services) {
    return {registry_error(ApplicationServiceRegistryErrorCode::InvalidManifest,
                           "manifest exceeds the configured service limit")};
  }

  const auto validate_list =
      [this](const std::vector<std::string> &values) -> ApplicationServiceRegistryResult {
    for (std::size_t index = 0; index < values.size(); ++index) {
      const auto &capability = values[index];
      if (capability.size() > impl_->limits.max_identifier_bytes ||
          !is_canonical_capability(capability)) {
        return {registry_error(ApplicationServiceRegistryErrorCode::InvalidManifest,
                               "manifest contains a non-canonical capability", capability)};
      }
      for (std::size_t prior = 0; prior < index; ++prior) {
        if (values[prior] == capability) {
          return {registry_error(ApplicationServiceRegistryErrorCode::InvalidManifest,
                                 "manifest contains a duplicate capability", capability)};
        }
      }
    }
    return {};
  };

  auto allowed_valid = validate_list(manifest.allowed);
  if (!allowed_valid) {
    return allowed_valid;
  }
  auto required_valid = validate_list(manifest.required);
  if (!required_valid) {
    return required_valid;
  }

  for (const auto &capability : manifest.required) {
    if (!contains_capability(manifest.allowed, capability)) {
      return {registry_error(ApplicationServiceRegistryErrorCode::InvalidManifest,
                             "required capability is not allowed", capability)};
    }
    if (!contains(capability)) {
      return {registry_error(ApplicationServiceRegistryErrorCode::MissingRequiredCapability,
                             "required capability is not registered", capability)};
    }
  }
  return {};
}

ApplicationCommandError
ApplicationServiceRegistry::validate_command(const ApplicationCommand &command,
                                             const ApplicationCapabilityManifest &manifest) const {
  if (command.capability.size() > impl_->limits.max_identifier_bytes ||
      !is_canonical_capability(command.capability)) {
    return command_error(ApplicationCommandErrorCode::InvalidCapability,
                         "service capability is not canonical");
  }
  if (command.operation.size() > impl_->limits.max_identifier_bytes ||
      !is_canonical_operation(command.operation)) {
    return command_error(ApplicationCommandErrorCode::InvalidOperation,
                         "service operation is not canonical");
  }
  if (!validate_manifest(manifest)) {
    return command_error(ApplicationCommandErrorCode::InvalidCapability,
                         "application capability manifest is invalid");
  }
  if (!contains_capability(manifest.allowed, command.capability)) {
    return command_error(ApplicationCommandErrorCode::UnknownCapability,
                         "service capability is not allowed");
  }
  const auto *service = find_service(impl_->entries, command.capability);
  if (service == nullptr) {
    return command_error(ApplicationCommandErrorCode::UnknownCapability,
                         "service capability is not registered");
  }
  const auto *operation = find_operation(*service, command.operation);
  if (operation == nullptr) {
    return command_error(ApplicationCommandErrorCode::UnsupportedOperation,
                         "service operation is not supported");
  }
  if (command.payload.size() > operation->max_payload_bytes) {
    return command_error(ApplicationCommandErrorCode::InvalidPayload,
                         "service payload exceeds the operation limit");
  }
  return {};
}

ApplicationServiceResolveResult
ApplicationServiceRegistry::resolve(const ApplicationServiceRequest &request,
                                    const ApplicationCapabilityManifest &manifest) const {
  if (!request.token || request.script_request_id == 0) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InvalidRequest,
                           "service request identity is invalid")};
  }
  auto manifest_result = validate_manifest(manifest);
  if (!manifest_result) {
    return {{}, std::move(manifest_result.error)};
  }
  if (request.capability.size() > impl_->limits.max_identifier_bytes ||
      !is_canonical_capability(request.capability)) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InvalidCapability,
                           "service capability is not canonical", request.capability)};
  }
  if (request.operation.size() > impl_->limits.max_identifier_bytes ||
      !is_canonical_operation(request.operation)) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InvalidOperation,
                           "service operation is not canonical", request.capability,
                           request.operation)};
  }
  if (!contains_capability(manifest.allowed, request.capability)) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::UnauthorizedCapability,
                           "service capability is not allowed", request.capability)};
  }
  const auto *service = find_service(impl_->entries, request.capability);
  if (service == nullptr) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::UnknownCapability,
                           "service capability is not registered", request.capability)};
  }
  const auto *operation = find_operation(*service, request.operation);
  if (operation == nullptr) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::UnsupportedOperation,
                           "service operation is not supported", request.capability,
                           request.operation)};
  }
  if (request.payload.size() > operation->max_payload_bytes) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InvalidPayload,
                           "service payload exceeds the operation limit", request.capability,
                           request.operation)};
  }
  if (!service->endpoint) {
    return {{},
            registry_error(ApplicationServiceRegistryErrorCode::InternalInvariant,
                           "registered service endpoint is missing", request.capability)};
  }
  return {service->endpoint, {}};
}

bool ApplicationServiceRegistry::contains(std::string_view capability) const noexcept {
  return find_service(impl_->entries, capability) != nullptr;
}

std::size_t ApplicationServiceRegistry::size() const noexcept { return impl_->entries.size(); }

} // namespace flexUI
