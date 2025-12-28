/*
 * Flex Engine - Data Binding
 *
 * Reactive bindings between inputs and node properties.
 * Supports:
 *   - Simple input binding: $InputName
 *   - Expression binding: ${ sin($Time) * 10 }
 */

#pragma once

#include "flex/runtime/types.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace flex {

// Forward declarations
class Node;
class ScriptContext;

// ============================================================================
// Binding Types
// ============================================================================

enum class BindingType : uint8_t {
    Input,       // Direct input reference: $InputName
    Expression,  // JavaScript expression: ${ expr }
};

// ============================================================================
// Binding - Single property binding
// ============================================================================

struct Binding {
    BindingType type = BindingType::Input;
    Symbol input_name;        // For Input type: the input name
    std::string expression;   // For Expression type: the JS expression

    // Target
    Node* target = nullptr;
    Symbol property;          // Property name on target node

    // Create input binding
    static Binding input(const std::string& input_name) {
        Binding b;
        b.type = BindingType::Input;
        b.input_name = Symbol(input_name);
        return b;
    }

    // Create expression binding
    static Binding expr(const std::string& expression) {
        Binding b;
        b.type = BindingType::Expression;
        b.expression = expression;
        return b;
    }
};

// ============================================================================
// BindingContext - Manages all bindings and input values
// ============================================================================

class BindingContext {
public:
    BindingContext();
    ~BindingContext();

    // -------------------------------------------
    // Input Management
    // -------------------------------------------

    void set_input(Symbol name, float value);
    void set_input(Symbol name, const std::string& value);
    void set_input(Symbol name, const char* value);
    void set_input(Symbol name, bool value);

    float get_float_input(Symbol name) const;
    const std::string& get_string_input(Symbol name) const;
    bool get_bool_input(Symbol name) const;

    bool has_input(Symbol name) const;

    // -------------------------------------------
    // Binding Registration
    // -------------------------------------------

    // Add a binding for a property
    void add_binding(Node* target, Symbol property, const Binding& binding);

    // Remove all bindings for a node
    void remove_bindings(Node* target);

    // Remove a specific binding
    void remove_binding(Node* target, Symbol property);

    // -------------------------------------------
    // Evaluation
    // -------------------------------------------

    // Evaluate all bindings and update target properties
    void evaluate();

    // Evaluate bindings for a specific node
    void evaluate(Node* target);

    // -------------------------------------------
    // Dirty Tracking (for optimization)
    // -------------------------------------------

    // Mark bindings as dirty (needs evaluation)
    void mark_dirty() { dirty_ = true; }

    // Check if bindings need evaluation
    bool is_dirty() const { return dirty_; }

    // Clear dirty flag
    void clear_dirty() { dirty_ = false; }

    // -------------------------------------------
    // Script Context
    // -------------------------------------------

    // Set the script context for expression evaluation
    void set_script_context(ScriptContext* ctx);

    // Get built-in time value
    float time() const { return time_; }
    void set_time(float t) { time_ = t; }
    void advance_time(float dt) { time_ += dt; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float time_ = 0.0f;
    bool dirty_ = true;  // Start dirty to evaluate on first frame
};

// ============================================================================
// Helper Functions
// ============================================================================

// Parse a binding string (e.g., "$Speed" or "${ sin($Time) * 10 }")
// Returns nullopt if not a binding
bool is_binding_string(const std::string& str);

// Extract binding from string
Binding parse_binding(const std::string& str);

} // namespace flex
