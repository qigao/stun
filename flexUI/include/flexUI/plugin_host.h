#pragma once

#include "flexUI/service_registry.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace flexUI {

struct PluginHostLimits {
  static constexpr std::size_t kDefaultMaxPlugins = 16;
  static constexpr std::size_t kDefaultMaxInFlightPerPlugin = 256;
  static constexpr std::size_t kMaximumMaxInFlightPerPlugin = 1024 * 1024;
  static constexpr std::size_t kDefaultMaxPluginIdentifierBytes = 256;
  static constexpr std::size_t kDefaultMaxPluginVersionBytes = 256;
  static constexpr std::size_t kDefaultMaxErrorMessageBytes = 1024;
  static constexpr std::size_t kDefaultMaxManifestBytes = 64 * 1024;
  static constexpr std::size_t kMaximumMaxManifestBytes = 1024 * 1024;
  static constexpr std::size_t kDefaultMaxDependenciesPerPlugin = 64;
  static constexpr std::size_t kDefaultMaxLibraryPathBytes = 1024;

  std::size_t max_plugins = kDefaultMaxPlugins;
  std::size_t max_in_flight_per_plugin = kDefaultMaxInFlightPerPlugin;
  std::size_t max_plugin_identifier_bytes = kDefaultMaxPluginIdentifierBytes;
  std::size_t max_plugin_version_bytes = kDefaultMaxPluginVersionBytes;
  std::size_t max_error_message_bytes = kDefaultMaxErrorMessageBytes;
  std::size_t max_manifest_bytes = kDefaultMaxManifestBytes;
  std::size_t max_dependencies_per_plugin =
      kDefaultMaxDependenciesPerPlugin;
  std::size_t max_library_path_bytes = kDefaultMaxLibraryPathBytes;
  ApplicationServiceRegistryLimits registry;
  ApplicationCompletionLimits completion;
};

enum class PluginPermission : std::uint32_t {
  File = 1U << 0U,
  Network = 1U << 1U,
  Process = 1U << 2U,
  Device = 1U << 3U,
};

using PluginPermissionMask = std::uint32_t;

constexpr PluginPermissionMask
plugin_permission_mask(PluginPermission permission) noexcept {
  return static_cast<PluginPermissionMask>(permission);
}

struct PluginHostPolicy {
  /// Admission mask for manifest-declared native access. The default denies
  /// every permission. This is not an OS sandbox for trusted in-process DLLs.
  PluginPermissionMask granted_permissions = 0;

  constexpr bool grants(PluginPermission permission) const noexcept {
    const auto bit = plugin_permission_mask(permission);
    return (granted_permissions & bit) == bit;
  }
};

enum class PluginHostErrorCode {
  None,
  InvalidLimits,
  WrongThread,
  InvalidState,
  InvalidPath,
  ManifestReadFailed,
  InvalidManifest,
  PermissionDenied,
  DependencyMissing,
  DependencyVersionMismatch,
  DependencyCycle,
  CapabilityMissing,
  ManifestMismatch,
  LoadFailed,
  EntryPointMissing,
  UnsupportedAbi,
  InvalidDescriptor,
  ResourceLimitExceeded,
  CreateFailed,
  StartFailed,
  StopFailed,
  JoinTimedOut,
  JoinFailed,
  RegistryFailed,
  AllocationFailed,
  InternalInvariant,
};

struct PluginHostError {
  PluginHostErrorCode code = PluginHostErrorCode::None;
  std::string stage;
  std::string plugin_id;
  std::filesystem::path path;
  std::string message;
  std::uint32_t plugin_status = 0;
  ApplicationServiceRegistryError registry_error;

  explicit operator bool() const noexcept { return code != PluginHostErrorCode::None; }
};

struct PluginHostResult {
  PluginHostError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

enum class PluginHostState {
  Started,
  Stopping,
  Stopped,
};

struct PluginHostStatistics {
  std::size_t plugin_count = 0;
  std::size_t current_in_flight = 0;
  std::size_t peak_in_flight = 0;
  std::uint64_t submitted = 0;
  std::uint64_t completed = 0;
  std::uint64_t rejected = 0;
  std::uint64_t completion_retries = 0;
  std::uint64_t abandoned = 0;
};

class PluginHost;

struct PluginHostBuildResult {
  std::unique_ptr<PluginHost> host;
  std::shared_ptr<const ApplicationServiceRegistry> registry;
  PluginHostError error;

  explicit operator bool() const noexcept {
    return host != nullptr && registry != nullptr && !static_cast<bool>(error);
  }
};

/// Owner-thread-only candidate builder. Loaded plugins are not started or
/// published until build() finishes registry validation.
class PluginHostBuilder final {
public:
  /// Creates an owner-thread-only candidate builder.
  ///
  /// @param limits Bounded host, registry, completion and manifest limits.
  /// @throws std::bad_alloc if builder state cannot be allocated.
  explicit PluginHostBuilder(PluginHostLimits limits = {});

  /// Creates a builder with an explicit manifest permission policy.
  ///
  /// @param limits Bounded host, registry, completion and manifest limits.
  /// @param policy Default-deny admission policy for manifest permissions.
  /// @throws std::bad_alloc if builder state cannot be allocated.
  ///
  /// Example: `PluginHostBuilder builder({}, PluginHostPolicy{
  /// plugin_permission_mask(PluginPermission::File)});`
  PluginHostBuilder(PluginHostLimits limits, PluginHostPolicy policy);
  ~PluginHostBuilder();

  PluginHostBuilder(const PluginHostBuilder &) = delete;
  PluginHostBuilder &operator=(const PluginHostBuilder &) = delete;
  PluginHostBuilder(PluginHostBuilder &&) = delete;
  PluginHostBuilder &operator=(PluginHostBuilder &&) = delete;

  /// Loads, validates and creates one candidate without starting it.
  /// Returns InvalidPath/LoadFailed/EntryPointMissing/UnsupportedAbi,
  /// InvalidDescriptor/CreateFailed or a resource error; the path must be
  /// absolute and identify an existing regular file.
  PluginHostResult load_plugin(const std::filesystem::path &absolute_path);

  /// Parses and stages one `plugin.toml` v1 candidate without loading its DLL.
  ///
  /// @param absolute_manifest_path Absolute path to an existing regular TOML
  /// file. Its relative `library` must resolve inside the manifest directory.
  /// @return Success when the bounded manifest is staged. Returns a manifest,
  /// path, permission, duplicate-ID, thread, state or resource error otherwise.
  /// Dependency, capability and DLL descriptor checks complete in build().
  ///
  /// Example: `builder.load_manifest("C:/app/plugins/echo/plugin.toml");`
  PluginHostResult
  load_manifest(const std::filesystem::path &absolute_manifest_path);

  /// Stages one host-owned endpoint in the same registry transaction.
  /// The endpoint must be non-null and remains shared by the built snapshot.
  PluginHostResult
  register_builtin(ApplicationServiceDescriptor descriptor,
                   std::shared_ptr<IApplicationServiceEndpoint> endpoint);

  /// Consumes the builder, validates the complete registry, starts legacy
  /// candidates first and manifest candidates in dependency order, then
  /// publishes both host and immutable registry on success.
  /// Failure publishes neither and unwinds started candidates in reverse.
  PluginHostBuildResult build();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Lifecycle Facade for trusted in-process service plugins.
///
/// The application completion sink supplied to endpoint submissions must
/// outlive stop(). A timed-out stop is retryable and keeps every DLL loaded.
class PluginHost final {
public:
  ~PluginHost();

  PluginHost(const PluginHost &) = delete;
  PluginHost &operator=(const PluginHost &) = delete;
  PluginHost(PluginHost &&) = delete;
  PluginHost &operator=(PluginHost &&) = delete;

  /// Rejects new submits, stops and joins plugins in reverse load order.
  /// JoinTimedOut leaves the host in Stopping and may be retried. A negative
  /// timeout is invalid; milliseconds::max() waits without a deadline.
  PluginHostResult stop(std::chrono::milliseconds timeout);
  PluginHostState state() const noexcept;
  PluginHostStatistics statistics() const noexcept;
  std::shared_ptr<const ApplicationServiceRegistry> registry() const noexcept;

private:
  friend class PluginHostBuilder;
  struct Impl;
  explicit PluginHost(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
