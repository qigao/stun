#include "plugin_dependency_resolver.hpp"

#include <functional>
#include <new>
#include <queue>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace flexUI::plugin_host_detail {
namespace {

struct PluginLocation {
  std::string_view version;
  std::size_t manifest_index = 0;
  bool is_manifest = false;
};

PluginHostError dependency_error(PluginHostErrorCode code,
                                 const PluginManifest &manifest,
                                 std::string message) {
  PluginHostError error;
  error.code = code;
  error.stage = "manifest_dependencies";
  error.path = manifest.path;
  error.plugin_id = manifest.name;
  error.message = std::move(message);
  return error;
}

} // namespace

PluginDependencyResolveResult resolve_plugin_dependencies(
    const std::vector<PluginManifest> &manifests,
    const std::vector<AvailablePluginIdentity> &preloaded_plugins) {
  try {
    std::unordered_map<std::string_view, PluginLocation> locations;
    locations.reserve(manifests.size() + preloaded_plugins.size());
    for (const auto &plugin : preloaded_plugins) {
      const auto inserted = locations.emplace(
          plugin.name, PluginLocation{plugin.version, 0, false});
      if (!inserted.second) {
        PluginHostError error;
        error.code = PluginHostErrorCode::InvalidDescriptor;
        error.stage = "manifest_dependencies";
        error.path = plugin.path;
        error.plugin_id = plugin.name;
        error.message = "preloaded plugin id is duplicated";
        return {{}, std::move(error)};
      }
    }
    for (std::size_t index = 0; index < manifests.size(); ++index) {
      const auto &manifest = manifests[index];
      const auto inserted = locations.emplace(
          manifest.name, PluginLocation{manifest.version, index, true});
      if (!inserted.second) {
        return {{}, dependency_error(PluginHostErrorCode::InvalidManifest,
                                     manifest,
                                     "plugin id is duplicated: " +
                                         manifest.name)};
      }
    }

    std::vector<std::vector<std::size_t>> dependents(manifests.size());
    std::vector<std::size_t> indegree(manifests.size(), 0);
    for (std::size_t index = 0; index < manifests.size(); ++index) {
      const auto &manifest = manifests[index];
      for (const auto &dependency : manifest.dependencies) {
        const auto found = locations.find(dependency.name);
        if (found == locations.end()) {
          if (dependency.optional) {
            continue;
          }
          return {{}, dependency_error(
                           PluginHostErrorCode::DependencyMissing, manifest,
                           "required dependency is missing: " +
                               dependency.name)};
        }
        if (found->second.version != dependency.version) {
          return {{}, dependency_error(
                           PluginHostErrorCode::DependencyVersionMismatch,
                           manifest,
                           "dependency version mismatch for " +
                               dependency.name + ": required " +
                               dependency.version + ", available " +
                               std::string(found->second.version))};
        }
        if (found->second.is_manifest) {
          dependents[found->second.manifest_index].push_back(index);
          ++indegree[index];
        }
      }
    }

    std::priority_queue<std::size_t, std::vector<std::size_t>,
                        std::greater<std::size_t>>
        ready;
    for (std::size_t index = 0; index < indegree.size(); ++index) {
      if (indegree[index] == 0) {
        ready.push(index);
      }
    }

    PluginDependencyResolveResult result;
    result.manifest_order.reserve(manifests.size());
    while (!ready.empty()) {
      const auto current = ready.top();
      ready.pop();
      result.manifest_order.push_back(current);
      for (const auto dependent : dependents[current]) {
        --indegree[dependent];
        if (indegree[dependent] == 0) {
          ready.push(dependent);
        }
      }
    }
    if (result.manifest_order.size() != manifests.size()) {
      for (std::size_t index = 0; index < indegree.size(); ++index) {
        if (indegree[index] != 0) {
          result.error = dependency_error(
              PluginHostErrorCode::DependencyCycle, manifests[index],
              "plugin dependency graph contains a cycle involving " +
                  manifests[index].name);
          break;
        }
      }
    }
    return result;
  } catch (const std::bad_alloc &) {
    PluginHostError error;
    error.code = PluginHostErrorCode::AllocationFailed;
    error.stage = "manifest_dependencies";
    error.message = "plugin dependency graph allocation failed";
    return {{}, std::move(error)};
  } catch (...) {
    PluginHostError error;
    error.code = PluginHostErrorCode::InternalInvariant;
    error.stage = "manifest_dependencies";
    error.message = "plugin dependency resolution failed unexpectedly";
    return {{}, std::move(error)};
  }
}

} // namespace flexUI::plugin_host_detail
