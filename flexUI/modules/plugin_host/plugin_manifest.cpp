#include "plugin_manifest.hpp"

#include "plugin_validation.hpp"

#include "flexUI/plugin_abi.h"

#include <toml.h>
#include <turbo_fs.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace flexUI::plugin_host_detail {
namespace {

constexpr std::int64_t kManifestVersion = 1;
struct FileHandle {
  turbo_file_t value = TURBO_INVALID_FILE;
  ~FileHandle() {
    if (value != TURBO_INVALID_FILE) {
      (void)turbo_fs_close(value);
    }
  }
};

struct TomlDocument {
  toml_table_t *value = nullptr;
  ~TomlDocument() { toml_free(value); }
};

PluginHostError manifest_error(PluginHostErrorCode code, std::string stage,
                               const std::filesystem::path &path,
                               std::string message,
                               std::string plugin_id = {}) {
  PluginHostError error;
  error.code = code;
  error.stage = std::move(stage);
  error.path = path;
  error.plugin_id = std::move(plugin_id);
  error.message = std::move(message);
  return error;
}

bool table_has_key(const toml_table_t *table, std::string_view expected) {
  const int count = toml_table_len(table);
  for (int index = 0; index < count; ++index) {
    int length = 0;
    const char *key = toml_table_key(table, index, &length);
    if (key != nullptr && length >= 0 &&
        std::string_view(key, static_cast<std::size_t>(length)) == expected) {
      return true;
    }
  }
  return false;
}

bool table_has_only(const toml_table_t *table,
                    std::initializer_list<std::string_view> allowed,
                    std::string &message) {
  const int count = toml_table_len(table);
  for (int index = 0; index < count; ++index) {
    int length = 0;
    const char *key = toml_table_key(table, index, &length);
    if (key == nullptr || length < 0) {
      message = "TOML table contains an invalid key";
      return false;
    }
    const std::string_view candidate(key, static_cast<std::size_t>(length));
    if (std::find(allowed.begin(), allowed.end(), candidate) == allowed.end()) {
      message = "unknown manifest key: " + std::string(candidate);
      return false;
    }
  }
  return true;
}

bool read_string(const toml_table_t *table, const char *key,
                 std::size_t limit, bool require_nonempty, std::string &output,
                 std::string &message) {
  if (!table_has_key(table, key)) {
    message = std::string("missing required string: ") + key;
    return false;
  }
  toml_value_t value = toml_table_string(table, key);
  std::unique_ptr<char, decltype(&std::free)> storage(value.ok ? value.u.s : nullptr,
                                                       &std::free);
  if (!value.ok || value.u.sl < 0 || value.u.s == nullptr) {
    message = std::string("manifest field must be a string: ") + key;
    return false;
  }
  const auto size = static_cast<std::size_t>(value.u.sl);
  if (size > limit || (require_nonempty && size == 0)) {
    message = std::string("manifest string is empty or exceeds its limit: ") + key;
    return false;
  }
  const std::string_view borrowed(value.u.s, size);
  if (!valid_utf8_text(borrowed)) {
    message = std::string("manifest string must be valid UTF-8 without NUL: ") + key;
    return false;
  }
  output.assign(borrowed);
  return true;
}

bool read_integer(const toml_table_t *table, const char *key,
                  std::int64_t minimum, std::int64_t maximum,
                  std::int64_t &output, std::string &message) {
  if (!table_has_key(table, key)) {
    message = std::string("missing required integer: ") + key;
    return false;
  }
  const toml_value_t value = toml_table_int(table, key);
  if (!value.ok || value.u.i < minimum || value.u.i > maximum) {
    message = std::string("manifest integer has an invalid value: ") + key;
    return false;
  }
  output = value.u.i;
  return true;
}

bool read_boolean(const toml_table_t *table, const char *key, bool &output,
                  std::string &message) {
  if (!table_has_key(table, key)) {
    message = std::string("missing required boolean: ") + key;
    return false;
  }
  const toml_value_t value = toml_table_bool(table, key);
  if (!value.ok) {
    message = std::string("manifest field must be a boolean: ") + key;
    return false;
  }
  output = value.u.b;
  return true;
}

bool valid_capability(std::string_view capability) noexcept {
  const auto slash = capability.find('/');
  if (slash == std::string_view::npos || slash == 0 ||
      slash + 1 >= capability.size() ||
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
    if (!((value >= 'a' && value <= 'z') ||
          (value >= '0' && value <= '9') || value == '_' || value == '-')) {
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
    if (parsed >
        (std::numeric_limits<std::uint32_t>::max() - digit) / std::uint32_t{10}) {
      return false;
    }
    parsed = parsed * std::uint32_t{10} + digit;
  }
  return parsed != 0;
}

bool read_string_array(const toml_table_t *table, const char *key,
                       std::size_t count_limit, std::size_t string_limit,
                       bool require_nonempty, bool require_capability,
                       std::vector<std::string> &output,
                       std::string &message) {
  if (!table_has_key(table, key)) {
    message = std::string("missing required array: ") + key;
    return false;
  }
  toml_array_t *array = toml_table_array(table, key);
  if (array == nullptr) {
    message = std::string("manifest field must be an array: ") + key;
    return false;
  }
  const int length = toml_array_len(array);
  if (length < 0 || static_cast<std::size_t>(length) > count_limit ||
      (require_nonempty && length == 0)) {
    message = std::string("manifest array is empty or exceeds its limit: ") + key;
    return false;
  }
  output.reserve(static_cast<std::size_t>(length));
  for (int index = 0; index < length; ++index) {
    toml_value_t value = toml_array_string(array, index);
    std::unique_ptr<char, decltype(&std::free)> storage(
        value.ok ? value.u.s : nullptr, &std::free);
    if (!value.ok || value.u.s == nullptr || value.u.sl <= 0 ||
        static_cast<std::size_t>(value.u.sl) > string_limit) {
      message = std::string("manifest array must contain bounded strings: ") + key;
      return false;
    }
    std::string candidate(value.u.s, static_cast<std::size_t>(value.u.sl));
    if (!valid_utf8_text(candidate) ||
        (require_capability && !valid_capability(candidate))) {
      message = std::string("manifest array contains an invalid identifier: ") + key;
      return false;
    }
    if (std::find(output.begin(), output.end(), candidate) != output.end()) {
      message = std::string("manifest array contains a duplicate value: ") + key;
      return false;
    }
    output.push_back(std::move(candidate));
  }
  return true;
}

bool parse_numeric_identifier(std::string_view value, std::size_t &position,
                              char delimiter) noexcept {
  const std::size_t begin = position;
  while (position < value.size() && value[position] >= '0' &&
         value[position] <= '9') {
    ++position;
  }
  if (position == begin || (position - begin > 1 && value[begin] == '0')) {
    return false;
  }
  if (delimiter == '\0') {
    return position == value.size() || value[position] == '-' ||
           value[position] == '+';
  }
  if (position >= value.size() || value[position] != delimiter) {
    return false;
  }
  ++position;
  return true;
}

bool parse_identifier_list(std::string_view value, std::size_t &position,
                           bool prerelease) noexcept {
  while (true) {
    const std::size_t begin = position;
    bool numeric = true;
    while (position < value.size() && value[position] != '.' &&
           value[position] != '+') {
      const char current = value[position];
      if (!((current >= '0' && current <= '9') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= 'a' && current <= 'z') || current == '-')) {
        return false;
      }
      numeric = numeric && current >= '0' && current <= '9';
      ++position;
    }
    if (position == begin ||
        (prerelease && numeric && position - begin > 1 && value[begin] == '0')) {
      return false;
    }
    if (position == value.size() || value[position] == '+') {
      return true;
    }
    ++position;
  }
}

PluginPermissionMask permission_from_name(std::string_view name) noexcept {
  if (name == "file") {
    return plugin_permission_mask(PluginPermission::File);
  }
  if (name == "network") {
    return plugin_permission_mask(PluginPermission::Network);
  }
  if (name == "process") {
    return plugin_permission_mask(PluginPermission::Process);
  }
  if (name == "device") {
    return plugin_permission_mask(PluginPermission::Device);
  }
  return 0;
}

bool parse_permissions(const toml_table_t *root,
                       const PluginHostPolicy &policy,
                       PluginPermissionMask &permissions,
                       bool &permission_denied,
                       std::string &message) {
  std::vector<std::string> names;
  if (!read_string_array(root, "permissions", 4, 16, false, false, names,
                         message)) {
    return false;
  }
  for (const auto &name : names) {
    const auto permission = permission_from_name(name);
    if (permission == 0) {
      message = "manifest requests an unknown permission: " + name;
      return false;
    }
    if ((policy.granted_permissions & permission) != permission) {
      permission_denied = true;
      message = "manifest permission is denied by host policy: " + name;
      return false;
    }
    permissions |= permission;
  }
  return true;
}

bool parse_dependencies(const toml_table_t *root,
                        const PluginHostLimits &limits,
                        std::vector<PluginManifestDependency> &dependencies,
                        std::string &message) {
  if (!table_has_key(root, "dependencies")) {
    message = "missing required array: dependencies";
    return false;
  }
  toml_array_t *array = toml_table_array(root, "dependencies");
  if (array == nullptr) {
    message = "manifest field must be an array: dependencies";
    return false;
  }
  const int length = toml_array_len(array);
  if (length < 0 || static_cast<std::size_t>(length) >
                        limits.max_dependencies_per_plugin) {
    message = "dependency array exceeds its configured limit";
    return false;
  }
  dependencies.reserve(static_cast<std::size_t>(length));
  for (int index = 0; index < length; ++index) {
    toml_table_t *entry = toml_array_table(array, index);
    if (entry == nullptr ||
        !table_has_only(entry, {"name", "version", "optional"}, message)) {
      if (entry == nullptr) {
        message = "dependencies must contain tables";
      }
      return false;
    }
    PluginManifestDependency dependency;
    if (!read_string(entry, "name", limits.max_plugin_identifier_bytes, true,
                     dependency.name, message)) {
      return false;
    }
    if (!valid_plugin_identifier(dependency.name)) {
      message = "dependency name must use canonical lowercase identifier syntax";
      return false;
    }
    if (!read_string(entry, "version", limits.max_plugin_version_bytes, true,
                     dependency.version, message)) {
      return false;
    }
    if (!is_canonical_semantic_version(dependency.version)) {
      message = "dependency version must be canonical SemVer 2.0.0";
      return false;
    }
    if (!read_boolean(entry, "optional", dependency.optional, message)) {
      return false;
    }
    const auto duplicate = std::find_if(
        dependencies.begin(), dependencies.end(),
        [&dependency](const PluginManifestDependency &candidate) {
          return candidate.name == dependency.name;
        });
    if (duplicate != dependencies.end()) {
      message = "dependency name is duplicated: " + dependency.name;
      return false;
    }
    dependencies.push_back(std::move(dependency));
  }
  return true;
}

bool path_is_inside(const std::filesystem::path &directory,
                    const std::filesystem::path &candidate) {
  const auto relative = candidate.lexically_relative(directory);
  if (relative.empty() || relative.is_absolute()) {
    return false;
  }
  for (const auto &part : relative) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

} // namespace

bool is_canonical_semantic_version(const std::string &value) noexcept {
  if (value.empty()) {
    return false;
  }
  std::size_t position = 0;
  if (!parse_numeric_identifier(value, position, '.') ||
      !parse_numeric_identifier(value, position, '.') ||
      !parse_numeric_identifier(value, position, '\0')) {
    return false;
  }
  if (position == value.size()) {
    return true;
  }
  if (value[position] == '-') {
    ++position;
    if (!parse_identifier_list(value, position, true)) {
      return false;
    }
  }
  if (position == value.size()) {
    return true;
  }
  if (value[position] != '+') {
    return false;
  }
  ++position;
  return parse_identifier_list(value, position, false) && position == value.size();
}

PluginManifestParseResult
parse_plugin_manifest(const std::filesystem::path &absolute_manifest_path,
                      const PluginHostLimits &limits,
                      const PluginHostPolicy &policy) {
  if (!valid_plugin_host_limits(limits)) {
    return {{}, manifest_error(PluginHostErrorCode::InvalidLimits, "manifest",
                               absolute_manifest_path,
                               "plugin host limits are invalid")};
  }
  if (!absolute_manifest_path.is_absolute()) {
    return {{}, manifest_error(PluginHostErrorCode::InvalidPath, "manifest_path",
                               absolute_manifest_path,
                               "plugin manifest path must be absolute")};
  }

  try {
    std::error_code path_error;
    const auto canonical_manifest =
        std::filesystem::canonical(absolute_manifest_path, path_error);
    if (path_error ||
        !std::filesystem::is_regular_file(canonical_manifest, path_error) ||
        path_error) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidPath,
                                 "manifest_path", absolute_manifest_path,
                                 "plugin manifest must identify an existing regular file")};
    }

    const std::string native_manifest_path = canonical_manifest.string();
    turbo_fs_stat_t file_stat{};
    if (turbo_fs_stat(native_manifest_path.c_str(), &file_stat) != 0 ||
        !file_stat.is_file) {
      return {{}, manifest_error(PluginHostErrorCode::ManifestReadFailed,
                                 "manifest_read", canonical_manifest,
                                 "failed to inspect plugin manifest")};
    }
    if (file_stat.size > limits.max_manifest_bytes) {
      return {{}, manifest_error(PluginHostErrorCode::ResourceLimitExceeded,
                                 "manifest_read", canonical_manifest,
                                 "plugin manifest exceeds the configured byte limit")};
    }

    FileHandle file;
    file.value = turbo_fs_open(native_manifest_path.c_str(),
                               TURBO_FS_O_RDONLY, 0);
    if (file.value == TURBO_INVALID_FILE) {
      return {{}, manifest_error(PluginHostErrorCode::ManifestReadFailed,
                                 "manifest_read", canonical_manifest,
                                 "failed to read plugin manifest")};
    }
    std::vector<std::uint8_t> buffer(limits.max_manifest_bytes + 1);
    std::size_t bytes_read = 0;
    while (bytes_read < buffer.size()) {
      const int current = turbo_fs_read(
          file.value, reinterpret_cast<char *>(buffer.data() + bytes_read),
          buffer.size() - bytes_read);
      if (current < 0) {
        return {{}, manifest_error(PluginHostErrorCode::ManifestReadFailed,
                                   "manifest_read", canonical_manifest,
                                   "failed while reading plugin manifest")};
      }
      if (current == 0) {
        break;
      }
      bytes_read += static_cast<std::size_t>(current);
    }
    if (bytes_read > limits.max_manifest_bytes) {
      return {{}, manifest_error(PluginHostErrorCode::ResourceLimitExceeded,
                                 "manifest_read", canonical_manifest,
                                 "plugin manifest grew beyond the configured byte limit")};
    }
    if (turbo_fs_close(file.value) != 0) {
      file.value = TURBO_INVALID_FILE;
      return {{}, manifest_error(PluginHostErrorCode::ManifestReadFailed,
                                 "manifest_read", canonical_manifest,
                                 "failed to close plugin manifest after reading")};
    }
    file.value = TURBO_INVALID_FILE;

    if (std::find(buffer.begin(), buffer.begin() + bytes_read,
                  static_cast<std::uint8_t>(0)) != buffer.begin() + bytes_read) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_parse", canonical_manifest,
                                 "plugin manifest must not contain embedded NUL")};
    }
    buffer[bytes_read] = 0;

    TomlDocument document;
    char toml_error[256] = {};
    document.value = toml_parse(reinterpret_cast<char *>(buffer.data()),
                                toml_error, static_cast<int>(sizeof(toml_error)));
    if (document.value == nullptr) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_parse", canonical_manifest,
                                 "plugin manifest contains invalid TOML")};
    }

    std::string message;
    if (!table_has_only(document.value,
                        {"manifest_version", "name", "version", "library",
                         "services", "capabilities", "permissions", "abi",
                         "dependencies"},
                        message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message))};
    }

    auto manifest = std::make_unique<PluginManifest>();
    manifest->path = canonical_manifest;
    std::int64_t manifest_version = 0;
    if (!read_integer(document.value, "manifest_version", kManifestVersion,
                      kManifestVersion, manifest_version, message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message))};
    }
    if (!read_string(document.value, "name",
                     limits.max_plugin_identifier_bytes, true, manifest->name,
                     message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message))};
    }
    if (!valid_plugin_identifier(manifest->name)) {
      message = "plugin name must use canonical lowercase identifier syntax";
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message), manifest->name)};
    }
    if (!read_string(document.value, "version",
                     limits.max_plugin_version_bytes, true, manifest->version,
                     message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message), manifest->name)};
    }
    if (!is_canonical_semantic_version(manifest->version)) {
      message = "plugin version must be canonical SemVer 2.0.0";
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message), manifest->name)};
    }

    std::string library;
    if (!read_string(document.value, "library", limits.max_library_path_bytes,
                     true, library, message) ||
        !read_string_array(document.value, "services",
                           limits.registry.max_services,
                           limits.registry.max_identifier_bytes, true, true,
                           manifest->services, message) ||
        !read_string_array(document.value, "capabilities",
                           limits.registry.max_services,
                           limits.registry.max_identifier_bytes, false, true,
                           manifest->capabilities, message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_schema", canonical_manifest,
                                 std::move(message), manifest->name)};
    }

    toml_table_t *abi = toml_table_table(document.value, "abi");
    std::int64_t abi_major = 0;
    std::int64_t abi_minor = 0;
    if (abi == nullptr) {
      message = "manifest requires an [abi] table";
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_abi", canonical_manifest,
                                 std::move(message), manifest->name)};
    }
    if (!table_has_only(abi, {"major", "minor"}, message) ||
        !read_integer(abi, "major", 0,
                      std::numeric_limits<std::uint32_t>::max(), abi_major,
                      message) ||
        !read_integer(abi, "minor", 0,
                      std::numeric_limits<std::uint32_t>::max(), abi_minor,
                      message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_abi", canonical_manifest,
                                 std::move(message), manifest->name)};
    }
    if (abi_major != FLEXUI_PLUGIN_ABI_MAJOR ||
        abi_minor > FLEXUI_PLUGIN_ABI_MINOR) {
      message = "manifest requires an unsupported FlexUI plugin ABI";
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_abi", canonical_manifest,
                                 std::move(message), manifest->name)};
    }
    manifest->required_host_abi_major = static_cast<std::uint32_t>(abi_major);
    manifest->required_host_abi_minor = static_cast<std::uint32_t>(abi_minor);

    bool permission_denied = false;
    if (!parse_permissions(document.value, policy, manifest->permissions,
                           permission_denied, message)) {
      const auto code = permission_denied
                            ? PluginHostErrorCode::PermissionDenied
                            : PluginHostErrorCode::InvalidManifest;
      return {{}, manifest_error(code, "manifest_permissions",
                                 canonical_manifest, std::move(message),
                                 manifest->name)};
    }
    if (!parse_dependencies(document.value, limits, manifest->dependencies,
                            message)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                                 "manifest_dependencies", canonical_manifest,
                                 std::move(message), manifest->name)};
    }

    const auto relative_library = std::filesystem::u8path(library);
    if (relative_library.is_absolute() || relative_library.has_root_name() ||
        relative_library.has_root_directory()) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidPath,
                                 "manifest_library", canonical_manifest,
                                 "manifest library path must be relative",
                                 manifest->name)};
    }
    const auto manifest_directory = canonical_manifest.parent_path();
    const auto canonical_library = std::filesystem::canonical(
        manifest_directory / relative_library, path_error);
    if (path_error ||
        !std::filesystem::is_regular_file(canonical_library, path_error) ||
        path_error || !path_is_inside(manifest_directory, canonical_library)) {
      return {{}, manifest_error(PluginHostErrorCode::InvalidPath,
                                 "manifest_library", canonical_manifest,
                                 "manifest library must resolve to a regular file inside its package directory",
                                 manifest->name)};
    }
    manifest->library_path = canonical_library;
    return {std::move(manifest), {}};
  } catch (const std::bad_alloc &) {
    return {{}, manifest_error(PluginHostErrorCode::AllocationFailed,
                               "manifest", absolute_manifest_path,
                               "plugin manifest allocation failed")};
  } catch (const std::exception &error) {
    return {{}, manifest_error(PluginHostErrorCode::InvalidManifest,
                               "manifest", absolute_manifest_path,
                               error.what())};
  } catch (...) {
    return {{}, manifest_error(PluginHostErrorCode::InternalInvariant,
                               "manifest", absolute_manifest_path,
                               "plugin manifest parsing failed unexpectedly")};
  }
}

} // namespace flexUI::plugin_host_detail
