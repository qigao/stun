/*
 * flexUI - Tree-sitter Plugin Loader
 */

#include <flexUI/ts_plugin.h>
#include <string>
#include <vector>
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
#else
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

struct LoadedPlugin {
    std::string name;
    const TSPluginInfo* info;
    PluginHandle handle;
};

static std::vector<LoadedPlugin>& get_loaded_plugins() {
    static std::vector<LoadedPlugin> plugins;
    return plugins;
}

static std::unordered_map<std::string, const TSPluginInfo*>& get_plugin_map() {
    static std::unordered_map<std::string, const TSPluginInfo*> map;
    return map;
}

bool load_ts_plugin(const std::string& path) {
    PluginHandle handle = PLUGIN_LOAD(path.c_str());
    if (!handle) return false;

    auto get_info = (const TSPluginInfo* (*)())PLUGIN_SYM(handle, "ts_plugin_info");
    if (!get_info) {
        PLUGIN_UNLOAD(handle);
        return false;
    }

    const TSPluginInfo* info = get_info();
    if (!info || !info->name || !info->highlight) {
        PLUGIN_UNLOAD(handle);
        return false;
    }

    LoadedPlugin plugin;
    plugin.name = info->name;
    plugin.info = info;
    plugin.handle = handle;

    get_loaded_plugins().push_back(plugin);
    get_plugin_map()[plugin.name] = info;
    return true;
}

void load_ts_plugins_from_dir(const std::string& dir) {
    if (!std::filesystem::exists(dir)) return;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (filename.find("ts-lang-") == 0 && ends_with(filename, PLUGIN_EXT)) {
            load_ts_plugin(entry.path().string());
        }
    }
}

const TSPluginInfo* get_ts_plugin(const std::string& name) {
    auto& map = get_plugin_map();
    auto it = map.find(name);
    return (it != map.end()) ? it->second : nullptr;
}

void unload_all_ts_plugins() {
    for (auto& plugin : get_loaded_plugins()) {
        PLUGIN_UNLOAD(plugin.handle);
    }
    get_loaded_plugins().clear();
    get_plugin_map().clear();
}

} // namespace flexUI
