#pragma once

#include "plugin_manifest.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace flexUI::plugin_host_detail {

struct AvailablePluginIdentity {
  std::string name;
  std::string version;
  std::filesystem::path path;
};

struct PluginDependencyResolveResult {
  std::vector<std::size_t> manifest_order;
  PluginHostError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

PluginDependencyResolveResult resolve_plugin_dependencies(
    const std::vector<PluginManifest> &manifests,
    const std::vector<AvailablePluginIdentity> &preloaded_plugins);

} // namespace flexUI::plugin_host_detail
