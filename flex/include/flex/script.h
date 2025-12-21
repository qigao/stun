/*
 * Flex Engine - Scripting System
 *
 * QuickJS integration for JavaScript logic.
 */

#pragma once

#include "flex/types.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace flex {

// Forward declarations
class Instance;
class Node;

// ============================================================================
// ScriptValue - Wrapper for JS values
// ============================================================================

struct ScriptValue {
    enum class Type { Undefined, Null, Bool, Number, String, Object, Array, Function };

    Type type = Type::Undefined;

    union {
        bool bool_val;
        double number_val;
    };
    std::string string_val;

    // Constructors
    static ScriptValue undefined() { return ScriptValue{Type::Undefined}; }
    static ScriptValue null() { return ScriptValue{Type::Null}; }
    static ScriptValue from_bool(bool v) { ScriptValue sv; sv.type = Type::Bool; sv.bool_val = v; return sv; }
    static ScriptValue from_number(double v) { ScriptValue sv; sv.type = Type::Number; sv.number_val = v; return sv; }
    static ScriptValue from_string(const std::string& v) { ScriptValue sv; sv.type = Type::String; sv.string_val = v; return sv; }

    // Accessors
    bool is_undefined() const { return type == Type::Undefined; }
    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }

    bool as_bool() const { return bool_val; }
    double as_number() const { return number_val; }
    const std::string& as_string() const { return string_val; }
};

// ============================================================================
// ScriptContext - Per-instance script execution
// ============================================================================

class ScriptContext {
public:
    ScriptContext();
    ~ScriptContext();

    // Evaluate JavaScript code
    ScriptValue eval(const std::string& code);

    // Call a global function
    ScriptValue call(const std::string& func_name, 
                    const std::vector<ScriptValue>& args = {});

    // Get/set global variable
    ScriptValue get_global(const std::string& name);
    void set_global(const std::string& name, const ScriptValue& value);

    // Bind a C++ function to JS
    using NativeFunc = std::function<ScriptValue(const std::vector<ScriptValue>&)>;
    void bind_function(const std::string& name, NativeFunc func);

    // Instance binding (for safe access from bindings)
    void set_instance(Instance* instance);
    Instance* get_instance() const;
    void clear_instance();

    // Error handling
    bool has_error() const { return has_error_; }
    const std::string& error_message() const { return error_message_; }
    void clear_error() { has_error_ = false; error_message_.clear(); }

    // Internal - Call registered native function by index
    ScriptValue call_native(int index, const std::vector<ScriptValue>& args);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool has_error_ = false;
    std::string error_message_;
};

// ============================================================================
// ScriptEngine - Manages script contexts and bindings
// ============================================================================

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Create a context for an instance
    std::shared_ptr<ScriptContext> create_context();

    // Bind Flex API to a context
    void bind_flex_api(ScriptContext* ctx, Instance* instance);

    // Load and execute a script file
    bool load_script(ScriptContext* ctx, const std::string& path);

    // Global initialization
    static void init();
    static void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Script Bindings - Flex API exposed to JS
// ============================================================================

namespace script {

// Bind math functions (sin, cos, lerp, clamp, etc.)
void bind_math(ScriptContext* ctx);

// Bind input control
void bind_inputs(ScriptContext* ctx, Instance* instance);

// Bind node access  
void bind_nodes(ScriptContext* ctx, Instance* instance);

// Bind event handling
void bind_events(ScriptContext* ctx, Instance* instance);

// Bind animation control
void bind_animations(ScriptContext* ctx, Instance* instance);

// Bind state machine control
void bind_machine(ScriptContext* ctx, Instance* instance);

} // namespace script

} // namespace flex
