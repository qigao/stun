/*
 * Flex Engine - Data Binding Implementation
 */

#include "flex/binding.h"
#include "flex/node.h"
#include "flex/script.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace flex {

// ============================================================================
// BindingContext Implementation
// ============================================================================

struct BindingContext::Impl {
    // Input storage
    std::unordered_map<std::string, float> float_inputs;
    std::unordered_map<std::string, std::string> string_inputs;
    std::unordered_map<std::string, bool> bool_inputs;

    // All bindings
    std::vector<Binding> bindings;

    // Script context for expression evaluation
    ScriptContext* script_ctx = nullptr;

    // Empty string for default return
    std::string empty_string;
};

BindingContext::BindingContext() : impl_(std::make_unique<Impl>()) {}

BindingContext::~BindingContext() = default;

// -------------------------------------------
// Input Management
// -------------------------------------------

void BindingContext::set_input(const std::string& name, float value) {
    impl_->float_inputs[name] = value;
}

void BindingContext::set_input(const std::string& name, const std::string& value) {
    impl_->string_inputs[name] = value;
}

void BindingContext::set_input(const std::string& name, const char* value) {
    impl_->string_inputs[name] = value ? value : "";
}

void BindingContext::set_input(const std::string& name, bool value) {
    impl_->bool_inputs[name] = value;
}

float BindingContext::get_float_input(const std::string& name) const {
    auto it = impl_->float_inputs.find(name);
    if (it != impl_->float_inputs.end()) {
        return it->second;
    }
    return 0.0f;
}

const std::string& BindingContext::get_string_input(const std::string& name) const {
    auto it = impl_->string_inputs.find(name);
    if (it != impl_->string_inputs.end()) {
        return it->second;
    }
    return impl_->empty_string;
}

bool BindingContext::get_bool_input(const std::string& name) const {
    auto it = impl_->bool_inputs.find(name);
    if (it != impl_->bool_inputs.end()) {
        return it->second;
    }
    return false;
}

bool BindingContext::has_input(const std::string& name) const {
    return impl_->float_inputs.count(name) > 0 ||
           impl_->string_inputs.count(name) > 0 ||
           impl_->bool_inputs.count(name) > 0;
}

// -------------------------------------------
// Binding Registration
// -------------------------------------------

void BindingContext::add_binding(Node* target, const std::string& property, const Binding& binding) {
    Binding b = binding;
    b.target = target;
    b.property = property;
    impl_->bindings.push_back(b);
}

void BindingContext::remove_bindings(Node* target) {
    impl_->bindings.erase(
        std::remove_if(impl_->bindings.begin(), impl_->bindings.end(),
            [target](const Binding& b) { return b.target == target; }),
        impl_->bindings.end()
    );
}

void BindingContext::remove_binding(Node* target, const std::string& property) {
    impl_->bindings.erase(
        std::remove_if(impl_->bindings.begin(), impl_->bindings.end(),
            [target, &property](const Binding& b) {
                return b.target == target && b.property == property;
            }),
        impl_->bindings.end()
    );
}

// -------------------------------------------
// Evaluation
// -------------------------------------------


// -------------------------------------------
// Script Context
// -------------------------------------------

void BindingContext::set_script_context(ScriptContext* ctx) {
    impl_->script_ctx = ctx;
}

// ============================================================================
// Helper Functions
// ============================================================================

bool is_binding_string(const std::string& str) {
    if (str.empty()) return false;

    // Check for ${ expression }
    if (str.size() >= 3 && str[0] == '$' && str[1] == '{') {
        return true;
    }

    // Check for $InputName (starts with $ but not ${)
    if (str[0] == '$' && (str.size() < 2 || str[1] != '{')) {
        return true;
    }

    return false;
}

Binding parse_binding(const std::string& str) {
    if (str.empty() || str[0] != '$') {
        return Binding::input("");
    }

    // Expression binding: ${ ... }
    if (str.size() >= 3 && str[1] == '{') {
        // Find closing brace
        size_t end = str.rfind('}');
        if (end != std::string::npos && end > 2) {
            std::string expr = str.substr(2, end - 2);
            return Binding::expr(expr);
        }
        return Binding::expr(str.substr(2));
    }

    // Simple input binding: $InputName
    return Binding::input(str.substr(1));
}

void BindingContext::evaluate() {
    // Bindings removed - simplified implementation
}

} // namespace flex
