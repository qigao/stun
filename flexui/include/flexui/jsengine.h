#pragma once

#include <quickjs.h>
#include <string>
#include <functional>
#include <vector>
#include <chrono>

namespace flexui {

class Screen;  // Forward declaration

// Timer entry for setTimeout
struct TimerEntry {
    int id;
    std::chrono::steady_clock::time_point triggerTime;
    JSValue callback;
    JSContext* ctx;
};

class JSEngine {
public:
    JSEngine();
    ~JSEngine();

    // Initialize with Screen pointer for widget access
    void init(Screen* screen);

    // Load and execute JavaScript
    bool eval(const std::string& code, const std::string& filename = "<eval>");
    bool evalModule(const std::string& code, const std::string& filename = "<module>");
    bool loadFile(const std::string& path);
    bool loadModule(const std::string& path);

    // Call a JavaScript function by name
    bool callFunction(const std::string& name);
    bool callFunction(const std::string& name, const std::string& arg);

    // Process pending timers (call this in main loop)
    void processPendingTimers();

    // Check if a function exists
    bool hasFunction(const std::string& name);

    JSRuntime* runtime() { return rt_; }
    JSContext* context() { return ctx_; }
    Screen* screen() { return screen_; }
    const std::string& basePath() const { return basePath_; }

    // Timer management
    int addTimer(int delayMs, JSValue callback);
    void removeTimer(int id);

private:
    JSRuntime* rt_ = nullptr;
    JSContext* ctx_ = nullptr;
    Screen* screen_ = nullptr;

    std::vector<TimerEntry> timers_;
    int nextTimerId_ = 1;
    std::string basePath_;  // Base path for module resolution

    void setupGlobalFunctions();
    void setupConsole();
    void setupWidgetAPI();
    void setupModuleLoader();
};

} // namespace flexui
