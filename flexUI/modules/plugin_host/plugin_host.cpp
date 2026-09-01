#include "flexUI/plugin_host.h"

#include "plugin_dependency_resolver.hpp"
#include "plugin_manifest.hpp"
#include "plugin_module.hpp"
#include "plugin_validation.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace flexUI {
namespace {

using plugin_host_detail::PluginModule;
using plugin_host_detail::PluginManifest;

struct BuiltinService {
  ApplicationServiceDescriptor descriptor;
  std::shared_ptr<IApplicationServiceEndpoint> endpoint;
};

PluginHostError make_error(PluginHostErrorCode code, std::string stage,
                           std::string message) {
  PluginHostError error;
  error.code = code;
  error.stage = std::move(stage);
  error.message = std::move(message);
  return error;
}

PluginHostError registry_error(ApplicationServiceRegistryError error) {
  PluginHostError result;
  result.code = PluginHostErrorCode::RegistryFailed;
  result.stage = "registry";
  result.message = error.message;
  result.registry_error = std::move(error);
  return result;
}

constexpr PluginPermissionMask kAllPluginPermissions =
    plugin_permission_mask(PluginPermission::File) |
    plugin_permission_mask(PluginPermission::Network) |
    plugin_permission_mask(PluginPermission::Process) |
    plugin_permission_mask(PluginPermission::Device);

bool valid_policy(const PluginHostPolicy &policy) noexcept {
  return (policy.granted_permissions & ~kAllPluginPermissions) == 0;
}

bool manifest_matches_module(const PluginManifest &manifest,
                             const PluginModule &module,
                             PluginHostError &error) {
  if (manifest.name != module.plugin_id() ||
      manifest.version != module.plugin_version() ||
      manifest.required_host_abi_minor != module.required_host_minor()) {
    error = make_error(PluginHostErrorCode::ManifestMismatch,
                       "manifest_descriptor",
                       "manifest identity, version, or ABI does not match the DLL descriptor");
    error.plugin_id = manifest.name;
    error.path = manifest.path;
    return false;
  }
  std::vector<std::string> descriptor_services;
  descriptor_services.reserve(module.services().size());
  for (const auto &service : module.services()) {
    descriptor_services.push_back(service.capability);
  }
  auto manifest_services = manifest.services;
  std::sort(descriptor_services.begin(), descriptor_services.end());
  std::sort(manifest_services.begin(), manifest_services.end());
  if (descriptor_services != manifest_services) {
    error = make_error(PluginHostErrorCode::ManifestMismatch,
                       "manifest_descriptor",
                       "manifest services do not match the DLL descriptor");
    error.plugin_id = manifest.name;
    error.path = manifest.path;
    return false;
  }
  return true;
}

} // namespace

struct PluginHostBuilder::Impl {
  Impl(PluginHostLimits configured_limits, PluginHostPolicy configured_policy)
      : limits(std::move(configured_limits)),
        policy(configured_policy),
        owner_thread(std::this_thread::get_id()) {}

  PluginHostLimits limits;
  PluginHostPolicy policy;
  std::thread::id owner_thread;
  bool consumed = false;
  std::vector<std::shared_ptr<PluginModule>> modules;
  std::vector<PluginManifest> manifests;
  std::vector<BuiltinService> builtins;
};

struct PluginHost::Impl {
  Impl(std::thread::id configured_owner_thread,
       std::vector<std::shared_ptr<PluginModule>> configured_modules,
       std::shared_ptr<const ApplicationServiceRegistry> configured_registry)
      : owner_thread(configured_owner_thread), modules(std::move(configured_modules)),
        registry(std::move(configured_registry)) {}

  std::thread::id owner_thread;
  std::vector<std::shared_ptr<PluginModule>> modules;
  std::shared_ptr<const ApplicationServiceRegistry> registry;
  std::atomic<PluginHostState> state{PluginHostState::Started};
};

PluginHostBuilder::PluginHostBuilder(PluginHostLimits limits)
    : PluginHostBuilder(std::move(limits), {}) {}

PluginHostBuilder::PluginHostBuilder(PluginHostLimits limits,
                                     PluginHostPolicy policy)
    : impl_(std::make_unique<Impl>(std::move(limits), policy)) {}

PluginHostBuilder::~PluginHostBuilder() = default;

PluginHostResult
PluginHostBuilder::load_plugin(const std::filesystem::path &absolute_path) {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {make_error(PluginHostErrorCode::WrongThread, "load",
                       "plugin loading belongs to the builder owner thread")};
  }
  if (impl_->consumed) {
    return {make_error(PluginHostErrorCode::InvalidState, "load",
                       "plugin builder has already been consumed")};
  }
  if (!plugin_host_detail::valid_plugin_host_limits(impl_->limits) ||
      !valid_policy(impl_->policy)) {
    return {make_error(PluginHostErrorCode::InvalidLimits, "validate",
                       "plugin host limits are invalid")};
  }
  if (impl_->modules.size() + impl_->manifests.size() >=
      impl_->limits.max_plugins) {
    return {make_error(PluginHostErrorCode::ResourceLimitExceeded, "load",
                       "plugin count exceeds the configured limit")};
  }

  auto loaded = PluginModule::load(absolute_path, impl_->limits);
  if (!loaded) {
    return {std::move(loaded.error)};
  }
  const auto duplicate =
      std::find_if(impl_->modules.begin(), impl_->modules.end(),
                   [&loaded](const std::shared_ptr<PluginModule> &candidate) {
                     return candidate->plugin_id() == loaded.module->plugin_id();
                   });
  if (duplicate != impl_->modules.end()) {
    auto error = make_error(PluginHostErrorCode::InvalidDescriptor,
                            "validate_descriptor",
                            "plugin id is already loaded");
    error.plugin_id = loaded.module->plugin_id();
    error.path = absolute_path;
    return {std::move(error)};
  }
  const auto staged_duplicate = std::find_if(
      impl_->manifests.begin(), impl_->manifests.end(),
      [&loaded](const PluginManifest &candidate) {
        return candidate.name == loaded.module->plugin_id();
      });
  if (staged_duplicate != impl_->manifests.end()) {
    auto error = make_error(PluginHostErrorCode::InvalidDescriptor,
                            "validate_descriptor",
                            "plugin id is already staged by a manifest");
    error.plugin_id = loaded.module->plugin_id();
    error.path = absolute_path;
    return {std::move(error)};
  }
  try {
    impl_->modules.push_back(std::move(loaded.module));
  } catch (const std::bad_alloc &) {
    return {make_error(PluginHostErrorCode::AllocationFailed, "load",
                       "plugin list allocation failed")};
  }
  return {};
}

PluginHostResult PluginHostBuilder::load_manifest(
    const std::filesystem::path &absolute_manifest_path) {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {make_error(PluginHostErrorCode::WrongThread, "manifest",
                       "manifest loading belongs to the builder owner thread")};
  }
  if (impl_->consumed) {
    return {make_error(PluginHostErrorCode::InvalidState, "manifest",
                       "plugin builder has already been consumed")};
  }
  if (!plugin_host_detail::valid_plugin_host_limits(impl_->limits) ||
      !valid_policy(impl_->policy)) {
    return {make_error(PluginHostErrorCode::InvalidLimits, "manifest",
                       "plugin host limits or policy are invalid")};
  }
  if (impl_->modules.size() + impl_->manifests.size() >=
      impl_->limits.max_plugins) {
    return {make_error(PluginHostErrorCode::ResourceLimitExceeded, "manifest",
                       "plugin count exceeds the configured limit")};
  }

  auto parsed = plugin_host_detail::parse_plugin_manifest(
      absolute_manifest_path, impl_->limits, impl_->policy);
  if (!parsed) {
    return {std::move(parsed.error)};
  }
  const auto loaded_duplicate = std::find_if(
      impl_->modules.begin(), impl_->modules.end(),
      [&parsed](const std::shared_ptr<PluginModule> &candidate) {
        return candidate->plugin_id() == parsed.manifest->name;
      });
  const auto staged_duplicate = std::find_if(
      impl_->manifests.begin(), impl_->manifests.end(),
      [&parsed](const PluginManifest &candidate) {
        return candidate.name == parsed.manifest->name;
      });
  if (loaded_duplicate != impl_->modules.end() ||
      staged_duplicate != impl_->manifests.end()) {
    auto error = make_error(PluginHostErrorCode::InvalidManifest,
                            "manifest_schema",
                            "plugin id is already loaded or staged");
    error.plugin_id = parsed.manifest->name;
    error.path = parsed.manifest->path;
    return {std::move(error)};
  }
  try {
    impl_->manifests.push_back(std::move(*parsed.manifest));
  } catch (const std::bad_alloc &) {
    return {make_error(PluginHostErrorCode::AllocationFailed, "manifest",
                       "plugin manifest list allocation failed")};
  }
  return {};
}

PluginHostResult PluginHostBuilder::register_builtin(
    ApplicationServiceDescriptor descriptor,
    std::shared_ptr<IApplicationServiceEndpoint> endpoint) {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {make_error(PluginHostErrorCode::WrongThread, "register_builtin",
                       "built-in registration belongs to the builder owner thread")};
  }
  if (impl_->consumed) {
    return {make_error(PluginHostErrorCode::InvalidState, "register_builtin",
                       "plugin builder has already been consumed")};
  }
  if (!endpoint) {
    return {make_error(PluginHostErrorCode::RegistryFailed, "register_builtin",
                       "built-in endpoint is required")};
  }
  try {
    impl_->builtins.push_back({std::move(descriptor), std::move(endpoint)});
  } catch (const std::bad_alloc &) {
    return {make_error(PluginHostErrorCode::AllocationFailed,
                       "register_builtin",
                       "built-in registration allocation failed")};
  }
  return {};
}

PluginHostBuildResult PluginHostBuilder::build() {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {{}, {}, make_error(PluginHostErrorCode::WrongThread, "build",
                               "plugin build belongs to the builder owner thread")};
  }
  if (impl_->consumed) {
    return {{}, {}, make_error(PluginHostErrorCode::InvalidState, "build",
                               "plugin builder has already been consumed")};
  }
  impl_->consumed = true;
  if (!plugin_host_detail::valid_plugin_host_limits(impl_->limits) ||
      !valid_policy(impl_->policy)) {
    return {{}, {}, make_error(PluginHostErrorCode::InvalidLimits, "validate",
                               "plugin host limits are invalid")};
  }

  try {
    auto modules = std::move(impl_->modules);
    std::vector<plugin_host_detail::AvailablePluginIdentity> preloaded;
    preloaded.reserve(modules.size());
    for (const auto &module : modules) {
      preloaded.push_back(
          {module->plugin_id(), module->plugin_version(), module->path()});
    }
    const auto dependency_order = plugin_host_detail::resolve_plugin_dependencies(
        impl_->manifests, preloaded);
    if (!dependency_order) {
      return {{}, {}, dependency_order.error};
    }

    modules.reserve(modules.size() + impl_->manifests.size());
    for (const auto manifest_index : dependency_order.manifest_order) {
      const auto &manifest = impl_->manifests[manifest_index];
      auto loaded = PluginModule::load(manifest.library_path, impl_->limits);
      if (!loaded) {
        return {{}, {}, std::move(loaded.error)};
      }
      PluginHostError mismatch;
      if (!manifest_matches_module(manifest, *loaded.module, mismatch)) {
        return {{}, {}, std::move(mismatch)};
      }
      modules.push_back(std::move(loaded.module));
    }

    ApplicationServiceRegistryBuilder registry_builder(impl_->limits.registry);
    for (const auto &builtin : impl_->builtins) {
      auto registered = registry_builder.register_service(builtin.descriptor,
                                                          builtin.endpoint);
      if (!registered) {
        return {{}, {}, registry_error(std::move(registered.error))};
      }
    }
    for (const auto &module : modules) {
      for (const auto &service : module->services()) {
        auto registered = registry_builder.register_service(service, module);
        if (!registered) {
          auto error = registry_error(std::move(registered.error));
          error.plugin_id = module->plugin_id();
          error.path = module->path();
          return {{}, {}, std::move(error)};
        }
      }
    }
    auto registry_result = registry_builder.build();
    if (!registry_result) {
      return {{}, {}, registry_error(std::move(registry_result.error))};
    }

    for (const auto &manifest : impl_->manifests) {
      for (const auto &capability : manifest.capabilities) {
        if (!registry_result.registry->contains(capability)) {
          auto error = make_error(
              PluginHostErrorCode::CapabilityMissing, "manifest_capabilities",
              "required plugin capability is not registered: " + capability);
          error.plugin_id = manifest.name;
          error.path = manifest.path;
          return {{}, {}, std::move(error)};
        }
      }
    }

    auto host_impl = std::make_unique<PluginHost::Impl>(
        impl_->owner_thread, std::vector<std::shared_ptr<PluginModule>>{},
        registry_result.registry);
    auto host =
        std::unique_ptr<PluginHost>(new PluginHost(std::move(host_impl)));

    std::size_t started_count = 0;
    for (; started_count < modules.size(); ++started_count) {
      auto started = modules[started_count]->start();
      if (!started) {
        for (std::size_t index = started_count; index > 0; --index) {
          modules[index - 1]->disable_submissions();
          (void)modules[index - 1]->stop(
              std::chrono::milliseconds::max());
        }
        return {{}, {}, std::move(started.error)};
      }
    }
    host->impl_->modules = std::move(modules);
    return {std::move(host), std::move(registry_result.registry), {}};
  } catch (const std::bad_alloc &) {
    return {{}, {}, make_error(PluginHostErrorCode::AllocationFailed, "build",
                               "plugin host build allocation failed")};
  } catch (const std::exception &error) {
    return {{}, {}, make_error(PluginHostErrorCode::InternalInvariant, "build",
                               error.what())};
  } catch (...) {
    return {{}, {}, make_error(PluginHostErrorCode::InternalInvariant, "build",
                               "plugin host build failed unexpectedly")};
  }
}

PluginHost::PluginHost(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

PluginHost::~PluginHost() {
  if (!impl_ || impl_->state.load() == PluginHostState::Stopped) {
    return;
  }
  for (const auto &module : impl_->modules) {
    module->disable_submissions();
  }
  for (auto module = impl_->modules.rbegin(); module != impl_->modules.rend();
       ++module) {
    (void)(*module)->stop(std::chrono::milliseconds::max());
  }
  impl_->state.store(PluginHostState::Stopped);
}

PluginHostResult PluginHost::stop(std::chrono::milliseconds timeout) {
  if (std::this_thread::get_id() != impl_->owner_thread) {
    return {make_error(PluginHostErrorCode::WrongThread, "stop",
                       "plugin shutdown belongs to the host owner thread")};
  }
  if (timeout.count() < 0) {
    return {make_error(PluginHostErrorCode::InvalidLimits, "stop",
                       "plugin shutdown timeout must not be negative")};
  }
  if (impl_->state.load() == PluginHostState::Stopped) {
    return {};
  }
  impl_->state.store(PluginHostState::Stopping);
  for (const auto &module : impl_->modules) {
    module->disable_submissions();
  }

  const bool infinite = timeout == std::chrono::milliseconds::max();
  const auto deadline = infinite ? std::chrono::steady_clock::time_point::max()
                                 : std::chrono::steady_clock::now() + timeout;
  for (auto module = impl_->modules.rbegin(); module != impl_->modules.rend();
       ++module) {
    std::chrono::milliseconds remaining = std::chrono::milliseconds::max();
    if (!infinite) {
      const auto now = std::chrono::steady_clock::now();
      remaining = now >= deadline
                      ? std::chrono::milliseconds(0)
                      : std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - now);
    }
    auto stopped = (*module)->stop(remaining);
    if (!stopped) {
      return stopped;
    }
  }
  impl_->state.store(PluginHostState::Stopped);
  return {};
}

PluginHostState PluginHost::state() const noexcept { return impl_->state.load(); }

PluginHostStatistics PluginHost::statistics() const noexcept {
  PluginHostStatistics total;
  total.plugin_count = impl_->modules.size();
  for (const auto &module : impl_->modules) {
    const auto current = module->statistics();
    total.current_in_flight += current.current_in_flight;
    total.peak_in_flight += current.peak_in_flight;
    total.submitted += current.submitted;
    total.completed += current.completed;
    total.rejected += current.rejected;
    total.completion_retries += current.completion_retries;
    total.abandoned += current.abandoned;
  }
  return total;
}

std::shared_ptr<const ApplicationServiceRegistry>
PluginHost::registry() const noexcept {
  return impl_->registry;
}

} // namespace flexUI
