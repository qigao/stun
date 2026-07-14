/*
 * Flex Engine - Data Binding
 *
 * Reactive bindings between inputs and node properties.
 * Supports:
 *   - Simple input binding: $InputName
 *   - Expression binding: ${ sin($Time) * 10 }
 */

#pragma once

#include "flex/core/types.h"
#include "flex/core/component.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <map>
#include <functional>

namespace flex {

// Forward declarations
class Node;
struct ComponentPropBinding;

// ============================================================================
// Binding Types
// ============================================================================

enum class BindingType : uint8_t {
    Input,       // Direct input reference: $InputName
    ExprTk,      // Legacy expression binding: ${ expr }
};

// ============================================================================
// Binding - Single property binding
// ============================================================================

struct Binding {
    BindingType type = BindingType::Input;
    Symbol input_name;        // For Input type: the input name
    std::string expression;   // For expression bindings

    // Target
    Node* target = nullptr;
    Symbol property;          // Property name on target node
    PropertyID property_id = PropertyID::Unknown;
    std::string property_name;


    // Create input binding
    static Binding input(const std::string& input_name) {
        Binding b;
        b.type = BindingType::Input;
        b.input_name = Symbol(input_name);
        return b;
    }

    // Create expression binding. Name retained for legacy callers.
    static Binding exprtk(const std::string& expression) {
        Binding b;
        b.type = BindingType::ExprTk;
        b.expression = expression;
        return b;
    }

    // Internal cache for compiled expressions
    std::shared_ptr<void> compiled_expr;
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
    void add_binding(Node* target, const char* property_name, const Binding& binding);
    void add_component_binding(const struct ComponentPropBinding& binding);

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

    // Get built-in time value
    float time() const { return time_; }
    void set_time(float t) {
        if (time_ == t) return;
        time_ = t;
        dirty_ = true;
    }
    void advance_time(float dt) {
        if (dt == 0.0f) return;
        time_ += dt;
        dirty_ = true;
    }

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

struct ComponentPropBinding {
    std::string component_name;
    ComponentNodePtr node;
    Component::SharedPtr component;
    Props base_props;
    std::map<std::string, Binding> prop_bindings;
};

} // namespace flex
