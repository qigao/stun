/*
 * flexUI - Tree-sitter Plugin Loader
 */

#include <flexUI/ts_plugin.h>
#include <string>
#include <memory>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#define PLUGIN_LOAD(path) LoadLibraryA(path)
#define PLUGIN_SYM(handle, name) (void*)GetProcAddress((HMODULE)handle, name)
#define PLUGIN_UNLOAD(handle) FreeLibrary((HMODULE)handle)
#define PLUGIN_EXT ".dll"
typedef HMODULE PluginHandle;
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#include <dlfcn.h>
#define PLUGIN_LOAD(path) dlopen(path, RTLD_LAZY)
#define PLUGIN_SYM(handle, name) dlsym(handle, name)
#define PLUGIN_UNLOAD(handle) dlclose(handle)
// CMake SHARED plugins on macOS use .dylib.
#define PLUGIN_EXT ".dylib"
typedef void* PluginHandle;
#else
#include <limits.h>
#include <unistd.h>
#include <dlfcn.h>
#define PLUGIN_LOAD(path) dlopen(path, RTLD_LAZY)
#define PLUGIN_SYM(handle, name) dlsym(handle, name)
#define PLUGIN_UNLOAD(handle) dlclose(handle)
#define PLUGIN_EXT ".so"
typedef void* PluginHandle;
#endif

namespace flexUI {

static bool ends_with(const std::string& str, const char* suffix) {
    size_t len = strlen(suffix);
    return str.size() >= len && str.compare(str.size() - len, len, suffix) == 0;
}

// Each registered plugin is one bounded DSO owner. A highlighter receives
// an aliasing shared_ptr lease so its TSPluginInfo and functions cannot outlive
// the library when unload_all_ts_plugins() clears the registry.
struct LoadedPlugin {
    std::string name;
    const TSPluginInfo* info = nullptr;
    PluginHandle handle = nullptr;
    ~LoadedPlugin() {
        if (handle) PLUGIN_UNLOAD(handle);
    }
};

static std::mutex& plugin_mutex() {
    static std::mutex m;
    return m;
}

static std::unordered_map<std::string, std::shared_ptr<LoadedPlugin>>&
get_plugin_map() {
    static std::unordered_map<std::string, std::shared_ptr<LoadedPlugin>> map;
    return map;
}

static std::filesystem::path executable_dir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    return std::filesystem::path(buffer.c_str()).parent_path();
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) return {};
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
#endif
}

bool load_ts_plugin(const std::string& path) {
    // Allocate the owner before loading the DSO so a bad_alloc cannot leak
    // a library handle between PLUGIN_LOAD and shared_ptr construction.
    auto plugin = std::make_shared<LoadedPlugin>();
    plugin->handle = PLUGIN_LOAD(path.c_str());
    if (!plugin->handle) return false;
    auto get_info = (const TSPluginInfo* (*)())PLUGIN_SYM(
        plugin->handle, "ts_plugin_info");
    if (!get_info) return false;
    const TSPluginInfo* info = get_info();
    if (!info || !info->name || !info->highlight || !info->free_result)
        return false;
    plugin->name = info->name;
    plugin->info = info;
    {
        std::lock_guard<std::mutex> lock(plugin_mutex());
        auto& map = get_plugin_map();
        if (map.find(plugin->name) != map.end())
            return true; // temporary additional dlopen lease is released
        map.emplace(plugin->name, std::move(plugin));
    }
    return true;
}

void load_ts_plugins_from_dir(const std::string& dir) {
    if (!std::filesystem::exists(dir)) return;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        // macOS accepts the CMake SHARED .dylib and MODULE .so forms.
        const bool supported_extension =
            ends_with(filename, PLUGIN_EXT)
#ifdef __APPLE__
            || ends_with(filename, ".so")
#endif
            ;
        if (filename.find("ts-lang-") == 0 && supported_extension) {
            load_ts_plugin(entry.path().string());
        }
    }
}

void load_default_ts_plugins() {
    const std::filesystem::path exe_dir = executable_dir();
    if (!exe_dir.empty()) {
        load_ts_plugins_from_dir(exe_dir.string());
    }

    const std::filesystem::path cwd = std::filesystem::current_path();
    if (cwd != exe_dir) {
        load_ts_plugins_from_dir(cwd.string());
    }
}

std::shared_ptr<const TSPluginInfo> acquire_ts_plugin(
    const std::string& name) {
    std::lock_guard<std::mutex> lock(plugin_mutex());
    const auto it = get_plugin_map().find(name);
    if (it == get_plugin_map().end()) return {};
    // Alias the owning DSO lease without allocating or copying plugin code.
    return std::shared_ptr<const TSPluginInfo>(it->second, it->second->info);
}

void unload_all_ts_plugins() {
    // Remove discovery references immediately. A cached/in-flight
    // TreeSitterHighlighter keeps the DSO alive until its last lease ends.
    // Close outside the registry lock to avoid plugin destructor reentry.
    std::unordered_map<std::string, std::shared_ptr<LoadedPlugin>> retired;
    {
        std::lock_guard<std::mutex> lock(plugin_mutex());
        retired.swap(get_plugin_map());
    }
    retired.clear();
}

} // namespace flexUI
