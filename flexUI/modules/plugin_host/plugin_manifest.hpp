#pragma once

#include "flexUI/plugin_host.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace flexUI::plugin_host_detail {

struct PluginManifestDependency {
  std::string name;
  std::string version;
  bool optional = false;
};

struct PluginManifest {
  std::filesystem::path path;
  std::filesystem::path library_path;
  std::string name;
  std::string version;
  std::uint32_t required_host_abi_major = 0;
  std::uint32_t required_host_abi_minor = 0;
  std::vector<std::string> services;
  std::vector<std::string> capabilities;
  PluginPermissionMask permissions = 0;
  std::vector<PluginManifestDependency> dependencies;
};

struct PluginManifestParseResult {
  std::unique_ptr<PluginManifest> manifest;
  PluginHostError error;

  explicit operator bool() const noexcept {
    return manifest != nullptr && !static_cast<bool>(error);
  }
};

PluginManifestParseResult
parse_plugin_manifest(const std::filesystem::path &absolute_manifest_path,
                      const PluginHostLimits &limits,
                      const PluginHostPolicy &policy);

bool is_canonical_semantic_version(const std::string &value) noexcept;

} // namespace flexUI::plugin_host_detail
