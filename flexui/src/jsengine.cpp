#include <flexui/jsengine.h>
#include <flexui/screen.h>
#include <flexui/widget.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
#include <flexui/slider.h>
#include <flexui/progressbar.h>
#include <flexui/dropdown.h>
#include <flexui/label.h>
#include <flexui/radiobutton.h>
#include <fmtlog.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace flexui {

// Class ID for Widget JS objects
static JSClassID js_widget_class_id = 0;

// Store engine pointer in context for callbacks
static JSEngine* getEngine(JSContext* ctx) {
    return static_cast<JSEngine*>(JS_GetContextOpaque(ctx));
}

// ============================================================================
// console.log implementation
// ============================================================================

static JSValue js_console_log(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    std::string output;
    for (int i = 0; i < argc; i++) {
        if (i > 0) output += " ";
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            output += str;
            JS_FreeCString(ctx, str);
        }
    }
    logi("[JS] {}", output);
    std::cout << "[JS] " << output << std::endl;
    return JS_UNDEFINED;
}

// ============================================================================
// setTimeout implementation
// ============================================================================

static JSValue js_setTimeout(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;

    JSEngine* engine = getEngine(ctx);
    if (!engine) return JS_UNDEFINED;

    // Get delay
    int delay = 0;
    JS_ToInt32(ctx, &delay, argv[1]);

    // Duplicate callback function
    JSValue callback = JS_DupValue(ctx, argv[0]);

    int id = engine->addTimer(delay, callback);
    return JS_NewInt32(ctx, id);
}

static JSValue js_clearTimeout(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    JSEngine* engine = getEngine(ctx);
    if (!engine) return JS_UNDEFINED;

    int id = 0;
    JS_ToInt32(ctx, &id, argv[0]);
    engine->removeTimer(id);

    return JS_UNDEFINED;
}

// ============================================================================
// Widget API implementation
// ============================================================================

// Widget wrapper class stored in JS object
struct WidgetWrapper {
    Widget* widget;
    Screen* screen;
};

static void js_widget_finalizer(JSRuntime* rt, JSValue val) {
    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(val, js_widget_class_id));
    delete wrapper;
}

static JSValue js_widget_getText(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    // Try different widget types
    if (auto* textbox = dynamic_cast<TextBox*>(wrapper->widget)) {
        return JS_NewString(ctx, textbox->getInputText().c_str());
    }
    if (auto* label = dynamic_cast<Label*>(wrapper->widget)) {
        return JS_NewString(ctx, label->getLabelText().c_str());
    }

    return JS_NewString(ctx, "");
}

static JSValue js_widget_setText(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    const char* text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_UNDEFINED;

    if (auto* textbox = dynamic_cast<TextBox*>(wrapper->widget)) {
        textbox->setText(text);
    } else if (auto* label = dynamic_cast<Label*>(wrapper->widget)) {
        label->setLabelText(text);
    }

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_widget_getValue(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    if (auto* slider = dynamic_cast<Slider*>(wrapper->widget)) {
        return JS_NewFloat64(ctx, slider->getValue());
    }
    if (auto* progress = dynamic_cast<ProgressBar*>(wrapper->widget)) {
        return JS_NewFloat64(ctx, progress->getProgress());
    }
    if (auto* dropdown = dynamic_cast<Dropdown*>(wrapper->widget)) {
        return JS_NewInt32(ctx, dropdown->getSelectedIndex());
    }

    return JS_UNDEFINED;
}

static JSValue js_widget_setValue(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    double value = 0;
    JS_ToFloat64(ctx, &value, argv[0]);

    if (auto* slider = dynamic_cast<Slider*>(wrapper->widget)) {
        slider->setValue((float)value);
    } else if (auto* progress = dynamic_cast<ProgressBar*>(wrapper->widget)) {
        progress->setProgress((float)value);
    } else if (auto* dropdown = dynamic_cast<Dropdown*>(wrapper->widget)) {
        dropdown->setSelectedIndex((int)value);
    }

    return JS_UNDEFINED;
}

static JSValue js_widget_isChecked(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_FALSE;

    if (auto* checkbox = dynamic_cast<Checkbox*>(wrapper->widget)) {
        return JS_NewBool(ctx, checkbox->isChecked());
    }
    if (auto* radio = dynamic_cast<RadioButton*>(wrapper->widget)) {
        return JS_NewBool(ctx, radio->isChecked());
    }

    return JS_FALSE;
}

static JSValue js_widget_setChecked(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    int checked = JS_ToBool(ctx, argv[0]);

    if (auto* checkbox = dynamic_cast<Checkbox*>(wrapper->widget)) {
        checkbox->setChecked(checked);
    } else if (auto* radio = dynamic_cast<RadioButton*>(wrapper->widget)) {
        radio->setChecked(checked);
    }

    return JS_UNDEFINED;
}

static JSValue js_widget_getId(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    WidgetWrapper* wrapper = static_cast<WidgetWrapper*>(JS_GetOpaque(this_val, js_widget_class_id));
    if (!wrapper || !wrapper->widget) return JS_UNDEFINED;

    return JS_NewString(ctx, wrapper->widget->id().c_str());
}

// getWidget(id) global function
static JSValue js_getWidget(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    JSEngine* engine = getEngine(ctx);
    if (!engine || !engine->screen()) return JS_NULL;

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;

    Widget* widget = engine->screen()->findWidget(id);
    JS_FreeCString(ctx, id);

    if (!widget) return JS_NULL;

    // Create Widget JS object
    JSValue obj = JS_NewObjectClass(ctx, js_widget_class_id);
    WidgetWrapper* wrapper = new WidgetWrapper{widget, engine->screen()};
    JS_SetOpaque(obj, wrapper);

    return obj;
}

// ============================================================================
// JSEngine implementation
// ============================================================================

JSEngine::JSEngine() {
    rt_ = JS_NewRuntime();
    ctx_ = JS_NewContext(rt_);
}

JSEngine::~JSEngine() {
    // Free pending timer callbacks
    for (auto& timer : timers_) {
        JS_FreeValue(ctx_, timer.callback);
    }
    timers_.clear();

    if (ctx_) JS_FreeContext(ctx_);
    if (rt_) JS_FreeRuntime(rt_);
}

void JSEngine::init(Screen* screen) {
    screen_ = screen;
    basePath_ = std::filesystem::current_path().string();
    JS_SetContextOpaque(ctx_, this);

    // Register Widget class (without designated initializers for C++17 compatibility)
    JS_NewClassID(rt_, &js_widget_class_id);

    JSClassDef widget_class_def;
    memset(&widget_class_def, 0, sizeof(widget_class_def));
    widget_class_def.class_name = "Widget";
    widget_class_def.finalizer = js_widget_finalizer;

    JS_NewClass(rt_, js_widget_class_id, &widget_class_def);

    // Create Widget prototype with methods
    JSValue proto = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, proto, "getText",
                      JS_NewCFunction(ctx_, js_widget_getText, "getText", 0));
    JS_SetPropertyStr(ctx_, proto, "setText",
                      JS_NewCFunction(ctx_, js_widget_setText, "setText", 1));
    JS_SetPropertyStr(ctx_, proto, "getValue",
                      JS_NewCFunction(ctx_, js_widget_getValue, "getValue", 0));
    JS_SetPropertyStr(ctx_, proto, "setValue",
                      JS_NewCFunction(ctx_, js_widget_setValue, "setValue", 1));
    JS_SetPropertyStr(ctx_, proto, "isChecked",
                      JS_NewCFunction(ctx_, js_widget_isChecked, "isChecked", 0));
    JS_SetPropertyStr(ctx_, proto, "setChecked",
                      JS_NewCFunction(ctx_, js_widget_setChecked, "setChecked", 1));
    JS_SetPropertyStr(ctx_, proto, "getId",
                      JS_NewCFunction(ctx_, js_widget_getId, "getId", 0));

    JS_SetClassProto(ctx_, js_widget_class_id, proto);

    setupGlobalFunctions();
    setupConsole();
    setupModuleLoader();
}

void JSEngine::setupGlobalFunctions() {
    JSValue global = JS_GetGlobalObject(ctx_);

    // getWidget(id)
    JS_SetPropertyStr(ctx_, global, "getWidget",
                      JS_NewCFunction(ctx_, js_getWidget, "getWidget", 1));

    // setTimeout / clearTimeout
    JS_SetPropertyStr(ctx_, global, "setTimeout",
                      JS_NewCFunction(ctx_, js_setTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx_, global, "clearTimeout",
                      JS_NewCFunction(ctx_, js_clearTimeout, "clearTimeout", 1));

    JS_FreeValue(ctx_, global);
}

void JSEngine::setupConsole() {
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue console = JS_NewObject(ctx_);

    JS_SetPropertyStr(ctx_, console, "log",
                      JS_NewCFunction(ctx_, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx_, console, "info",
                      JS_NewCFunction(ctx_, js_console_log, "info", 1));
    JS_SetPropertyStr(ctx_, console, "warn",
                      JS_NewCFunction(ctx_, js_console_log, "warn", 1));
    JS_SetPropertyStr(ctx_, console, "error",
                      JS_NewCFunction(ctx_, js_console_log, "error", 1));

    JS_SetPropertyStr(ctx_, global, "console", console);
    JS_FreeValue(ctx_, global);
}

bool JSEngine::eval(const std::string& code, const std::string& filename) {
    JSValue result = JS_Eval(ctx_, code.c_str(), code.size(), filename.c_str(),
                             JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            loge("[JS Error] {}", str);
            std::cerr << "[JS Error] " << str << std::endl;
            JS_FreeCString(ctx_, str);
        }
        JS_FreeValue(ctx_, exception);
        JS_FreeValue(ctx_, result);
        return false;
    }

    JS_FreeValue(ctx_, result);
    return true;
}

bool JSEngine::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        loge("[JS] Failed to open file: {}", path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return eval(buffer.str(), path);
}

bool JSEngine::callFunction(const std::string& name) {
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue func = JS_GetPropertyStr(ctx_, global, name.c_str());

    if (!JS_IsFunction(ctx_, func)) {
        JS_FreeValue(ctx_, func);
        JS_FreeValue(ctx_, global);
        return false;
    }

    JSValue result = JS_Call(ctx_, func, global, 0, nullptr);

    bool success = !JS_IsException(result);
    if (!success) {
        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            loge("[JS Error in {}] {}", name, str);
            JS_FreeCString(ctx_, str);
        }
        JS_FreeValue(ctx_, exception);
    }

    JS_FreeValue(ctx_, result);
    JS_FreeValue(ctx_, func);
    JS_FreeValue(ctx_, global);

    return success;
}

bool JSEngine::callFunction(const std::string& name, const std::string& arg) {
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue func = JS_GetPropertyStr(ctx_, global, name.c_str());

    if (!JS_IsFunction(ctx_, func)) {
        JS_FreeValue(ctx_, func);
        JS_FreeValue(ctx_, global);
        return false;
    }

    JSValue argv[1] = { JS_NewString(ctx_, arg.c_str()) };
    JSValue result = JS_Call(ctx_, func, global, 1, argv);
    JS_FreeValue(ctx_, argv[0]);

    bool success = !JS_IsException(result);
    if (!success) {
        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            loge("[JS Error in {}] {}", name, str);
            JS_FreeCString(ctx_, str);
        }
        JS_FreeValue(ctx_, exception);
    }

    JS_FreeValue(ctx_, result);
    JS_FreeValue(ctx_, func);
    JS_FreeValue(ctx_, global);

    return success;
}

bool JSEngine::evalModule(const std::string& code, const std::string& filename) {
    JSValue result = JS_Eval(ctx_, code.c_str(), code.size(), filename.c_str(),
                             JS_EVAL_TYPE_MODULE);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            loge("[JS Module Error] {}", str);
            std::cerr << "[JS Module Error] " << str << std::endl;
            JS_FreeCString(ctx_, str);
        }
        JS_FreeValue(ctx_, exception);
        JS_FreeValue(ctx_, result);
        return false;
    }

    JS_FreeValue(ctx_, result);
    return true;
}

bool JSEngine::loadModule(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        loge("[JS] Failed to open module: {}", path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    
    // Update base path for relative imports - use absolute path
    std::filesystem::path p(path);
    std::filesystem::path abs_path = std::filesystem::absolute(p);
    
    if (abs_path.has_parent_path()) {
        basePath_ = abs_path.parent_path().string();
        logi("[JS] Set base path to: {}", basePath_);
    }
    
    return evalModule(buffer.str(), abs_path.string());
}

bool JSEngine::hasFunction(const std::string& name) {
    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue func = JS_GetPropertyStr(ctx_, global, name.c_str());
    bool exists = JS_IsFunction(ctx_, func);
    JS_FreeValue(ctx_, func);
    JS_FreeValue(ctx_, global);
    return exists;
}

int JSEngine::addTimer(int delayMs, JSValue callback) {
    int id = nextTimerId_++;
    auto triggerTime = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(delayMs);

    timers_.push_back({id, triggerTime, callback, ctx_});
    return id;
}

void JSEngine::removeTimer(int id) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->id == id) {
            JS_FreeValue(it->ctx, it->callback);
            timers_.erase(it);
            return;
        }
    }
}

void JSEngine::processPendingTimers() {
    auto now = std::chrono::steady_clock::now();

    // Collect expired timers first to avoid iterator invalidation
    // (callbacks may add new timers via setTimeout)
    std::vector<TimerEntry> expired;
    for (auto it = timers_.begin(); it != timers_.end();) {
        if (now >= it->triggerTime) {
            expired.push_back(*it);
            it = timers_.erase(it);
        } else {
            ++it;
        }
    }

    // Now execute callbacks safely
    for (auto& timer : expired) {
        JSValue global = JS_GetGlobalObject(ctx_);
        JSValue result = JS_Call(ctx_, timer.callback, global, 0, nullptr);

        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(ctx_);
            const char* str = JS_ToCString(ctx_, exception);
            if (str) {
                loge("[JS Timer Error] {}", str);
                JS_FreeCString(ctx_, str);
            }
            JS_FreeValue(ctx_, exception);
        }

        JS_FreeValue(ctx_, result);
        JS_FreeValue(ctx_, global);
        JS_FreeValue(ctx_, timer.callback);
    }
}

// ============================================================================
// Built-in modules
// ============================================================================

static const char* FLEXUI_STDLIB = R"JS(
export const FlexUI = {
    getWidget: (id) => getWidget(id),
    setTimeout: (fn, ms) => setTimeout(fn, ms),
    clearTimeout: (id) => clearTimeout(id),
    log: (...args) => console.log(...args),
    info: (...args) => console.info(...args),
    warn: (...args) => console.warn(...args),
    error: (...args) => console.error(...args)
};

export class WidgetHelper {
    static setText(id, text) {
        const widget = getWidget(id);
        if (widget) widget.setText(text);
    }
    
    static getText(id) {
        const widget = getWidget(id);
        return widget ? widget.getText() : '';
    }
    
    static setValue(id, value) {
        const widget = getWidget(id);
        if (widget) widget.setValue(value);
    }
    
    static getValue(id) {
        const widget = getWidget(id);
        return widget ? widget.getValue() : null;
    }
    
    static setChecked(id, checked) {
        const widget = getWidget(id);
        if (widget) widget.setChecked(checked);
    }
    
    static isChecked(id) {
        const widget = getWidget(id);
        return widget ? widget.isChecked() : false;
    }
}

export class Animation {
    static lerp(start, end, t) {
        return start + (end - start) * t;
    }
    
    static easeInOut(t) {
        return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }
    
    static animate(duration, callback) {
        const startTime = Date.now();
        
        function tick() {
            const elapsed = Date.now() - startTime;
            const progress = Math.min(elapsed / duration, 1);
            
            callback(progress);
            
            if (progress < 1) {
                setTimeout(tick, 16);
            }
        }
        
        tick();
    }
}

export class EventBus {
    constructor() {
        this.listeners = {};
    }
    
    on(event, callback) {
        if (!this.listeners[event]) {
            this.listeners[event] = [];
        }
        this.listeners[event].push(callback);
    }
    
    off(event, callback) {
        if (!this.listeners[event]) return;
        this.listeners[event] = this.listeners[event].filter(cb => cb !== callback);
    }
    
    emit(event, data) {
        if (!this.listeners[event]) return;
        this.listeners[event].forEach(cb => cb(data));
    }
}

export default FlexUI;
)JS";

// ============================================================================
// Module loader implementation
// ============================================================================

static JSModuleDef* js_module_loader(JSContext* ctx, const char* module_name, void* opaque) {
    JSEngine* engine = static_cast<JSEngine*>(opaque);
    
    // Check for built-in modules
    std::string name(module_name);
    std::string code;
    std::string resolved_name;
    
    if (name == "flexui" || name == "flexui.js") {
        code = FLEXUI_STDLIB;
        resolved_name = "flexui";
    } else {
        // Load from file system
        std::filesystem::path module_path(module_name);
        
        // Handle relative paths - resolve against basePath_ (parent module directory)
        if (module_path.is_relative()) {
            const std::string& base = engine->basePath();
            if (base.empty()) {
                loge("[JS] Cannot resolve relative module '{}' without base path", module_name);
                return nullptr;
            }
            module_path = std::filesystem::path(base) / module_path;
            logi("[JS] Resolved '{}' to '{}'", module_name, module_path.string());
        }
        
        module_path = module_path.lexically_normal();
        resolved_name = module_path.string();
        
        // Read file
        std::ifstream file(module_path);
        if (!file.is_open()) {
            loge("[JS] Module not found: {}", module_path.string());
            return nullptr;
        }
        
        code = std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    }
    
    // Compile module
    JSValue func = JS_Eval(ctx, code.c_str(), code.size(),
                           resolved_name.c_str(),
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    
    if (JS_IsException(func)) {
        JSValue exception = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exception);
        if (str) {
            loge("[JS] Module compile error in {}: {}", resolved_name, str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, exception);
        return nullptr;
    }
    
    JSModuleDef* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(func));
    JS_FreeValue(ctx, func);
    return m;
}

void JSEngine::setupModuleLoader() {
    JS_SetModuleLoaderFunc(rt_, nullptr, js_module_loader, this);
}

} // namespace flexui
