#include "native_plugin_library.hpp"

#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace flexUI::plugin_host_detail {
namespace {

#ifdef _WIN32
std::string windows_error(const char *operation) {
  return std::string(operation) + " failed with Win32 error " +
         std::to_string(static_cast<unsigned long>(GetLastError()));
}
#endif

} // namespace

NativePluginLibrary::~NativePluginLibrary() { close(); }

NativePluginLibrary::NativePluginLibrary(NativePluginLibrary &&other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), path_(std::move(other.path_)) {}

NativePluginLibrary &NativePluginLibrary::operator=(NativePluginLibrary &&other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, nullptr);
    path_ = std::move(other.path_);
  }
  return *this;
}

bool NativePluginLibrary::open(const std::filesystem::path &absolute_path,
                               std::string &message) noexcept {
  close();
  message.clear();
  try {
    if (!absolute_path.is_absolute()) {
      message = "plugin path must be absolute";
      return false;
    }
    std::error_code error;
    auto canonical = std::filesystem::canonical(absolute_path, error);
    if (error || !std::filesystem::is_regular_file(canonical, error) || error) {
      message = "plugin path must identify an existing regular file";
      return false;
    }

#ifdef _WIN32
    auto *loaded = LoadLibraryExW(
        canonical.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (loaded == nullptr) {
      message = windows_error("LoadLibraryExW");
      return false;
    }
    handle_ = loaded;
#else
    void *loaded = dlopen(canonical.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (loaded == nullptr) {
      const char *error_text = dlerror();
      message = error_text != nullptr ? error_text : "dlopen failed";
      return false;
    }
    handle_ = loaded;
#endif
    path_ = std::move(canonical);
    return true;
  } catch (const std::exception &error) {
    message = error.what();
    return false;
  } catch (...) {
    message = "plugin path validation failed unexpectedly";
    return false;
  }
}

void *NativePluginLibrary::symbol(const char *name, std::string &message) const noexcept {
  message.clear();
  if (handle_ == nullptr || name == nullptr || *name == '\0') {
    message = "library and symbol name are required";
    return nullptr;
  }
#ifdef _WIN32
  auto *result = reinterpret_cast<void *>(
      GetProcAddress(static_cast<HMODULE>(handle_), name));
  if (result == nullptr) {
    message = windows_error("GetProcAddress");
  }
  return result;
#else
  dlerror();
  void *result = dlsym(handle_, name);
  if (const char *error_text = dlerror(); error_text != nullptr) {
    message = error_text;
    return nullptr;
  }
  return result;
#endif
}

void NativePluginLibrary::close() noexcept {
  if (handle_ == nullptr) {
    return;
  }
#ifdef _WIN32
  FreeLibrary(static_cast<HMODULE>(handle_));
#else
  dlclose(handle_);
#endif
  handle_ = nullptr;
  path_.clear();
}

void NativePluginLibrary::abandon() noexcept {
  handle_ = nullptr;
  path_.clear();
}

NativePluginLibrary::operator bool() const noexcept { return handle_ != nullptr; }

const std::filesystem::path &NativePluginLibrary::path() const noexcept { return path_; }

} // namespace flexUI::plugin_host_detail
