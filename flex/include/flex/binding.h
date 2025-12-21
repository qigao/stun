/*
 * Flex Engine - Data Binding
 *
 * Reactive bindings between inputs and node properties.
 * Supports:
 *   - Simple input binding: $InputName
 *   - Expression binding: ${ sin($Time) * 10 }
 */

#pragma once

#include "flex/types.h"
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
    std::string input_name;   // For Input type: the input name
    std::string expression;   // For Expression type: the JS expression

    // Target
    Node* target = nullptr;
    std::string property;     // Property name on target node

    // Create input binding
    static Binding input(const std::string& input_name) {
        Binding b;
        b.type = BindingType::Input;
        b.input_name = input_name;
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

    void set_input(const std::string& name, float value);
    void set_input(const std::string& name, const std::string& value);
    void set_input(const std::string& name, const char* value);
    void set_input(const std::string& name, bool value);

    float get_float_input(const std::string& name) const;
    const std::string& get_string_input(const std::string& name) const;
    bool get_bool_input(const std::string& name) const;

    bool has_input(const std::string& name) const;

    // -------------------------------------------
    // Binding Registration
    // -------------------------------------------

    // Add a binding for a property
    void add_binding(Node* target, const std::string& property, const Binding& binding);

    // Remove all bindings for a node
    void remove_bindings(Node* target);

    // Remove a specific binding
    void remove_binding(Node* target, const std::string& property);

    // -------------------------------------------
    // Evaluation
    // -------------------------------------------

    // Evaluate all bindings and update target properties
    void evaluate();

    // Evaluate bindings for a specific node
    void evaluate(Node* target);

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
