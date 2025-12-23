/*
 * Flex Engine - Scripting Implementation
 *
 * QuickJS integration.
 */

#include "flex/script.h"
#include "flex/flex.h"
#include <quickjs.h>
#include <fstream>
#include <sstream>

namespace flex {

// ============================================================================
// ScriptContext Implementation
// ============================================================================

struct ScriptContext::Impl {
    JSRuntime* runtime = nullptr;
    JSContext* ctx = nullptr;
    std::vector<ScriptContext::NativeFunc> native_funcs;
    Instance* bound_instance = nullptr;

    Impl() {
        runtime = JS_NewRuntime();
        ctx = JS_NewContext(runtime);
    }

    ~Impl() {
        if (ctx) JS_FreeContext(ctx);
        if (runtime) JS_FreeRuntime(runtime);
    }
};

// Native function callback wrapper
static JSValue native_func_callback(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv, int magic) {
    (void)this_val;
    
    // Get ScriptContext from context opaque
    auto* context = static_cast<ScriptContext*>(JS_GetContextOpaque(ctx));
    if (!context) return JS_UNDEFINED;

    // Convert arguments
    std::vector<ScriptValue> args;
    for (int i = 0; i < argc; ++i) {
        ScriptValue sv;
        if (JS_IsBool(argv[i])) {
            sv = ScriptValue::from_bool(JS_ToBool(ctx, argv[i]));
        } else if (JS_IsNumber(argv[i])) {
            double val;
            JS_ToFloat64(ctx, &val, argv[i]);
            sv = ScriptValue::from_number(val);
        } else if (JS_IsString(argv[i])) {
            const char* str = JS_ToCString(ctx, argv[i]);
            sv = ScriptValue::from_string(str ? str : "");
            if (str) JS_FreeCString(ctx, str);
        }
        args.push_back(sv);
    }

    // Call the native function via ScriptContext
    ScriptValue result = context->call_native(magic, args);

    // Convert result back to JS
    switch (result.type) {
        case ScriptValue::Type::Bool:
            return JS_NewBool(ctx, result.bool_val);
        case ScriptValue::Type::Number:
            return JS_NewFloat64(ctx, result.number_val);
        case ScriptValue::Type::String:
            return JS_NewString(ctx, result.string_val.c_str());
        case ScriptValue::Type::Null:
            return JS_NULL;
        default:
            return JS_UNDEFINED;
    }
}

ScriptContext::ScriptContext() : impl_(std::make_unique<Impl>()) {
    JS_SetContextOpaque(impl_->ctx, this);
}
ScriptContext::~ScriptContext() = default;

ScriptValue ScriptContext::call_native(int index, const std::vector<ScriptValue>& args) {
    if (index >= 0 && static_cast<size_t>(index) < impl_->native_funcs.size()) {
        return impl_->native_funcs[index](args);
    }
    return ScriptValue::undefined();
}

ScriptValue ScriptContext::eval(const std::string& code) {
    clear_error();

    JSValue result = JS_Eval(impl_->ctx, code.c_str(), code.length(), "<eval>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(impl_->ctx);
        const char* str = JS_ToCString(impl_->ctx, exception);
        has_error_ = true;
        error_message_ = str ? str : "Unknown error";
        if (str) JS_FreeCString(impl_->ctx, str);
        JS_FreeValue(impl_->ctx, exception);
        JS_FreeValue(impl_->ctx, result);
        return ScriptValue::undefined();
    }

    ScriptValue sv;
    if (JS_IsBool(result)) {
        sv = ScriptValue::from_bool(JS_ToBool(impl_->ctx, result));
    } else if (JS_IsNumber(result)) {
        double val;
        JS_ToFloat64(impl_->ctx, &val, result);
        sv = ScriptValue::from_number(val);
    } else if (JS_IsString(result)) {
        const char* str = JS_ToCString(impl_->ctx, result);
        sv = ScriptValue::from_string(str ? str : "");
        if (str) JS_FreeCString(impl_->ctx, str);
    } else if (JS_IsNull(result)) {
        sv = ScriptValue::null();
    }

    JS_FreeValue(impl_->ctx, result);
    return sv;
}

ScriptValue ScriptContext::call(const std::string& func_name, 
                               const std::vector<ScriptValue>& args) {
    clear_error();

    // Get global function
    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JSValue func = JS_GetPropertyStr(impl_->ctx, global, func_name.c_str());

    if (!JS_IsFunction(impl_->ctx, func)) {
        JS_FreeValue(impl_->ctx, func);
        JS_FreeValue(impl_->ctx, global);
        has_error_ = true;
        error_message_ = "Function not found: " + func_name;
        return ScriptValue::undefined();
    }

    // Convert arguments
    std::vector<JSValue> js_args;
    for (const auto& arg : args) {
        switch (arg.type) {
            case ScriptValue::Type::Bool:
                js_args.push_back(JS_NewBool(impl_->ctx, arg.bool_val));
                break;
            case ScriptValue::Type::Number:
                js_args.push_back(JS_NewFloat64(impl_->ctx, arg.number_val));
                break;
            case ScriptValue::Type::String:
                js_args.push_back(JS_NewString(impl_->ctx, arg.string_val.c_str()));
                break;
            default:
                js_args.push_back(JS_UNDEFINED);
                break;
        }
    }

    // Call function
    JSValue result = JS_Call(impl_->ctx, func, global, 
                             static_cast<int>(js_args.size()), js_args.data());

    // Cleanup args
    for (auto& arg : js_args) {
        JS_FreeValue(impl_->ctx, arg);
    }

    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(impl_->ctx);
        const char* str = JS_ToCString(impl_->ctx, exception);
        has_error_ = true;
        error_message_ = str ? str : "Unknown error";
        if (str) JS_FreeCString(impl_->ctx, str);
        JS_FreeValue(impl_->ctx, exception);
        JS_FreeValue(impl_->ctx, result);
        JS_FreeValue(impl_->ctx, func);
        JS_FreeValue(impl_->ctx, global);
        return ScriptValue::undefined();
    }

    ScriptValue sv;
    if (JS_IsBool(result)) {
        sv = ScriptValue::from_bool(JS_ToBool(impl_->ctx, result));
    } else if (JS_IsNumber(result)) {
        double val;
        JS_ToFloat64(impl_->ctx, &val, result);
        sv = ScriptValue::from_number(val);
    } else if (JS_IsString(result)) {
        const char* str = JS_ToCString(impl_->ctx, result);
        sv = ScriptValue::from_string(str ? str : "");
        if (str) JS_FreeCString(impl_->ctx, str);
    }

    JS_FreeValue(impl_->ctx, result);
    JS_FreeValue(impl_->ctx, func);
    JS_FreeValue(impl_->ctx, global);
    return sv;
}

ScriptValue ScriptContext::get_global(const std::string& name) {
    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JSValue val = JS_GetPropertyStr(impl_->ctx, global, name.c_str());

    ScriptValue sv;
    if (JS_IsBool(val)) {
        sv = ScriptValue::from_bool(JS_ToBool(impl_->ctx, val));
    } else if (JS_IsNumber(val)) {
        double v;
        JS_ToFloat64(impl_->ctx, &v, val);
        sv = ScriptValue::from_number(v);
    } else if (JS_IsString(val)) {
        const char* str = JS_ToCString(impl_->ctx, val);
        sv = ScriptValue::from_string(str ? str : "");
        if (str) JS_FreeCString(impl_->ctx, str);
    }

    JS_FreeValue(impl_->ctx, val);
    JS_FreeValue(impl_->ctx, global);
    return sv;
}

void ScriptContext::set_global(const std::string& name, const ScriptValue& value) {
    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JSValue val = JS_UNDEFINED;

    switch (value.type) {
        case ScriptValue::Type::Bool:
            val = JS_NewBool(impl_->ctx, value.bool_val);
            break;
        case ScriptValue::Type::Number:
            val = JS_NewFloat64(impl_->ctx, value.number_val);
            break;
        case ScriptValue::Type::String:
            val = JS_NewString(impl_->ctx, value.string_val.c_str());
            break;
        case ScriptValue::Type::Null:
            val = JS_NULL;
            break;
        default:
            val = JS_UNDEFINED;
            break;
    }

    JS_SetPropertyStr(impl_->ctx, global, name.c_str(), val);
    JS_FreeValue(impl_->ctx, global);
}

void ScriptContext::bind_function(const std::string& name, NativeFunc func) {
    int index = static_cast<int>(impl_->native_funcs.size());
    impl_->native_funcs.push_back(func);

    // Create function with magic index
    // Note: JS_NewCFunctionMagic takes length for args, we use 0 for varargs/generic
    JSValue js_func = JS_NewCFunctionMagic(impl_->ctx, native_func_callback, name.c_str(), 0, JS_CFUNC_generic_magic, index);

    JSValue global = JS_GetGlobalObject(impl_->ctx);
    JS_SetPropertyStr(impl_->ctx, global, name.c_str(), js_func);
    JS_FreeValue(impl_->ctx, global);
}

void ScriptContext::set_instance(Instance* instance) {
    impl_->bound_instance = instance;
}

Instance* ScriptContext::get_instance() const {
    return impl_->bound_instance;
}

void ScriptContext::clear_instance() {
    impl_->bound_instance = nullptr;
}

// ============================================================================
// ScriptEngine Implementation
// ============================================================================

struct ScriptEngine::Impl {
    // Engine-level state if needed
};

ScriptEngine::ScriptEngine() : impl_(std::make_unique<Impl>()) {}
ScriptEngine::~ScriptEngine() = default;

std::shared_ptr<ScriptContext> ScriptEngine::create_context() {
    return std::make_shared<ScriptContext>();
}

void ScriptEngine::bind_flex_api(ScriptContext* ctx, Instance* instance) {
    ctx->set_instance(instance);
    script::bind_math(ctx);
    script::bind_inputs(ctx, instance);
    script::bind_nodes(ctx, instance);
    script::bind_events(ctx, instance);
    script::bind_animations(ctx, instance);
    script::bind_machine(ctx, instance);
}

bool ScriptEngine::load_script(ScriptContext* ctx, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    ctx->eval(buffer.str());

    return !ctx->has_error();
}

void ScriptEngine::init() {
    // Global QuickJS init if needed
}

void ScriptEngine::shutdown() {
    // Global QuickJS cleanup if needed
}

// ============================================================================
// Script Bindings
// ============================================================================

namespace script {

void bind_math(ScriptContext* ctx) {
    // Trigonometric functions
    ctx->bind_function("sin", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::sin(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    ctx->bind_function("cos", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::cos(args[0].as_number()));
        }
        return ScriptValue::from_number(1);
    });

    ctx->bind_function("tan", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::tan(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Interpolation
    ctx->bind_function("lerp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 3 && args[0].is_number() && args[1].is_number() && args[2].is_number()) {
            double a = args[0].as_number();
            double b = args[1].as_number();
            double t = args[2].as_number();
            return ScriptValue::from_number(a + (b - a) * t);
        }
        return ScriptValue::from_number(0);
    });

    // Clamping
    ctx->bind_function("clamp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 3 && args[0].is_number() && args[1].is_number() && args[2].is_number()) {
            double v = args[0].as_number();
            double lo = args[1].as_number();
            double hi = args[2].as_number();
            return ScriptValue::from_number(v < lo ? lo : (v > hi ? hi : v));
        }
        return ScriptValue::from_number(0);
    });

    // Min/Max
    ctx->bind_function("min", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].is_number() && args[1].is_number()) {
            return ScriptValue::from_number(std::min(args[0].as_number(), args[1].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    ctx->bind_function("max", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].is_number() && args[1].is_number()) {
            return ScriptValue::from_number(std::max(args[0].as_number(), args[1].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Absolute value
    ctx->bind_function("abs", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::abs(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Floor/Ceil/Round
    ctx->bind_function("floor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::floor(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    ctx->bind_function("ceil", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::ceil(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    ctx->bind_function("round", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::round(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Power/Sqrt
    ctx->bind_function("pow", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2 && args[0].is_number() && args[1].is_number()) {
            return ScriptValue::from_number(std::pow(args[0].as_number(), args[1].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    ctx->bind_function("sqrt", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::sqrt(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Exponential/Logarithm
    ctx->bind_function("exp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::exp(args[0].as_number()));
        }
        return ScriptValue::from_number(1);
    });

    ctx->bind_function("log", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 1 && args[0].is_number()) {
            return ScriptValue::from_number(std::log(args[0].as_number()));
        }
        return ScriptValue::from_number(0);
    });

    // Constants
    ctx->set_global("PI", ScriptValue::from_number(3.14159265358979323846));
    ctx->set_global("E", ScriptValue::from_number(2.71828182845904523536));
}

void bind_inputs(ScriptContext* ctx, Instance* /*instance*/) {
    // flex.setInput(name, value)
    ctx->bind_function("flex_setInput", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::undefined();
        if (args.size() >= 2 && args[0].is_string() && args[1].is_number()) {
            inst->set_input(args[0].as_string().c_str(), static_cast<float>(args[1].as_number()));
        }
        return ScriptValue::undefined();
    });

    // flex.getInput(name) -> number
    ctx->bind_function("flex_getInput", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::from_number(0);
        if (args.size() >= 1 && args[0].is_string()) {
            return ScriptValue::from_number(inst->get_input(args[0].as_string().c_str()));
        }
        return ScriptValue::from_number(0);
    });
}

void bind_nodes(ScriptContext* ctx, Instance* /*instance*/) {
    // flex.getProperty(nodeId, prop) -> value
    ctx->bind_function("flex_getProperty", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst || !inst->artboard()) return ScriptValue::undefined();
        return ScriptValue::undefined();
    });

    // flex.setProperty(nodeId, prop, value)
    ctx->bind_function("flex_setProperty", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst || !inst->artboard()) return ScriptValue::undefined();
        return ScriptValue::undefined();
    });
}

void bind_events(ScriptContext* ctx, Instance* /*instance*/) {
    // flex.sendEvent(name)
    ctx->bind_function("flex_sendEvent", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::undefined();
        if (args.size() >= 1 && args[0].is_string()) {
            inst->send_event(args[0].as_string().c_str());
        }
        return ScriptValue::undefined();
    });
}

void bind_animations(ScriptContext* ctx, Instance* /*instance*/) {
    // flex.play(timelineName)
    ctx->bind_function("flex_play", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::undefined();
        if (args.size() >= 1 && args[0].is_string()) {
            inst->play(args[0].as_string().c_str());
        }
        return ScriptValue::undefined();
    });

    // flex.stop(timelineName)
    ctx->bind_function("flex_stop", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::undefined();
        if (args.size() >= 1 && args[0].is_string()) {
            inst->stop(args[0].as_string().c_str());
        }
        return ScriptValue::undefined();
    });
}

void bind_machine(ScriptContext* ctx, Instance* /*instance*/) {
    // flex.currentState(layer) -> string
    #if 0  // REMOVED: current_state() is part of old Machine system
    ctx->bind_function("flex_currentState", [ctx](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* inst = ctx->get_instance();
        if (!inst) return ScriptValue::from_string("");
        if (args.size() >= 1 && args[0].is_string()) {
            return ScriptValue::from_string(inst->current_state(args[0].as_string().c_str()));
        }
        return ScriptValue::from_string("");
    });
    #endif
}

} // namespace script

} // namespace flex
