#pragma once

#include <filesystem>
#include <string>

namespace flexUI::plugin_host_detail {

class NativePluginLibrary final {
public:
  NativePluginLibrary() = default;
  ~NativePluginLibrary();

  NativePluginLibrary(const NativePluginLibrary &) = delete;
  NativePluginLibrary &operator=(const NativePluginLibrary &) = delete;
  NativePluginLibrary(NativePluginLibrary &&other) noexcept;
  NativePluginLibrary &operator=(NativePluginLibrary &&other) noexcept;

  bool open(const std::filesystem::path &absolute_path, std::string &message) noexcept;
  void *symbol(const char *name, std::string &message) const noexcept;
  void close() noexcept;
  void abandon() noexcept;
  explicit operator bool() const noexcept;
  const std::filesystem::path &path() const noexcept;

private:
  void *handle_ = nullptr;
  std::filesystem::path path_;
};

} // namespace flexUI::plugin_host_detail
