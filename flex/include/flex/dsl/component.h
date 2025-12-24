/*
 * Component System
 * Reusable UI components with props
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <variant>

namespace flex {

class Node;
class Group;

// ============================================================================
// Prop Value Types
// ============================================================================

using PropValue = std::variant<
    float,           // Numbers (width, height, fontSize, etc.)
    std::string,     // Strings (content, id, etc.)
    bool,            // Booleans (visible, enabled, etc.)
    uint32_t         // Colors (stored as ARGB)
>;

// Prop definition with default value
struct PropDef {
    std::string name;
    PropValue default_value;
    std::string description;

    PropDef() = default;
    PropDef(const std::string& n, PropValue def, const std::string& desc = "")
        : name(n), default_value(def), description(desc) {}
};

// Props collection for an instance
using Props = std::map<std::string, PropValue>;

// ============================================================================
// Component - Reusable UI template
// ============================================================================

class Component {
public:
    using Ptr = std::shared_ptr<Component>;

    // Builder function type: takes props and returns a node tree
    using BuilderFunc = std::function<std::shared_ptr<Node>(const Props&)>;

    Component(const std::string& name);
    ~Component() = default;

    // Factory
    static Ptr create(const std::string& name) {
        return std::make_shared<Component>(name);
    }

    // Component metadata
    const std::string& name() const { return name_; }
    void set_description(const std::string& desc) { description_ = desc; }
    const std::string& description() const { return description_; }

    // Prop definitions
    void add_prop(const std::string& name, PropValue default_value, const std::string& desc = "");
    void add_prop(const PropDef& prop);
    bool has_prop(const std::string& name) const;
    const PropDef* get_prop_def(const std::string& name) const;
    const std::map<std::string, PropDef>& props() const { return prop_defs_; }

    // Builder function
    void set_builder(BuilderFunc builder) { builder_ = builder; }
    bool has_builder() const { return builder_ != nullptr; }

    // Instantiate component with given props
    std::shared_ptr<Node> instantiate(const Props& props = {}) const;

    // Validate props (check types, required props, etc.)
    bool validate_props(const Props& props, std::string& error) const;

private:
    std::string name_;
    std::string description_;
    std::map<std::string, PropDef> prop_defs_;
    BuilderFunc builder_;

    // Merge user props with defaults
    Props merge_props(const Props& user_props) const;
};

// ============================================================================
// Component Registry - Global component storage
// ============================================================================

class ComponentRegistry {
public:
    static ComponentRegistry& instance();

    // Register component
    void register_component(Component::Ptr component);
    void register_component(const std::string& name, Component::BuilderFunc builder);

    // Lookup
    Component::Ptr get(const std::string& name) const;
    bool has(const std::string& name) const;

    // List all
    std::vector<std::string> list_components() const;

    // Clear
    void clear();

private:
    ComponentRegistry() = default;
    std::map<std::string, Component::Ptr> components_;
};

// ============================================================================
// Helper Functions
// ============================================================================

// Register a component with builder function
inline void register_component(const std::string& name, Component::BuilderFunc builder) {
    ComponentRegistry::instance().register_component(name, builder);
}

// Get a registered component
inline Component::Ptr get_component(const std::string& name) {
    return ComponentRegistry::instance().get(name);
}

// Instantiate a component by name
inline std::shared_ptr<Node> create_component_instance(
    const std::string& name,
    const Props& props = {})
{
    auto component = get_component(name);
    if (!component) return nullptr;
    return component->instantiate(props);
}

// ============================================================================
// Prop Helpers
// ============================================================================

// Get prop value with type checking
template<typename T>
inline T get_prop(const Props& props, const std::string& name, T default_value) {
    auto it = props.find(name);
    if (it == props.end()) return default_value;

    try {
        return std::get<T>(it->second);
    } catch (const std::bad_variant_access&) {
        return default_value;
    }
}

// Specialized for strings
inline std::string get_prop_string(const Props& props, const std::string& name, const std::string& default_value = "") {
    return get_prop<std::string>(props, name, default_value);
}

// Specialized for floats
inline float get_prop_float(const Props& props, const std::string& name, float default_value = 0.0f) {
    return get_prop<float>(props, name, default_value);
}

// Specialized for bools
inline bool get_prop_bool(const Props& props, const std::string& name, bool default_value = false) {
    return get_prop<bool>(props, name, default_value);
}

// Specialized for colors
inline uint32_t get_prop_color(const Props& props, const std::string& name, uint32_t default_value = 0xFFFFFFFF) {
    return get_prop<uint32_t>(props, name, default_value);
}

} // namespace flex
