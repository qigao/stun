/*
 * Component System Implementation
 */

#include <flex/component.h>
#include <flex/node.h>
#include <iostream>

namespace flex {

// ============================================================================
// Component
// ============================================================================

Component::Component(const std::string& name)
    : name_(name)
    , builder_(nullptr)
{
}

void Component::add_prop(const std::string& name, PropValue default_value, const std::string& desc) {
    prop_defs_[name] = PropDef{name, default_value, desc};
}

void Component::add_prop(const PropDef& prop) {
    prop_defs_[prop.name] = prop;
}

bool Component::has_prop(const std::string& name) const {
    return prop_defs_.find(name) != prop_defs_.end();
}

const PropDef* Component::get_prop_def(const std::string& name) const {
    auto it = prop_defs_.find(name);
    if (it == prop_defs_.end()) return nullptr;
    return &it->second;
}

std::shared_ptr<Node> Component::instantiate(const Props& props) const {
    if (!builder_) {
        std::cerr << "Component '" << name_ << "' has no builder function\n";
        return nullptr;
    }

    // Validate props
    std::string error;
    if (!validate_props(props, error)) {
        std::cerr << "Component '" << name_ << "' prop validation failed: " << error << "\n";
        return nullptr;
    }

    // Merge user props with defaults
    Props merged = merge_props(props);

    // Call builder function
    return builder_(merged);
}

bool Component::validate_props(const Props& props, std::string& error) const {
    // Check for unknown props
    for (const auto& [name, value] : props) {
        if (!has_prop(name)) {
            error = "Unknown prop: " + name;
            return false;
        }
    }

    // TODO: Add type checking
    // For now, just check for unknown props

    return true;
}

Props Component::merge_props(const Props& user_props) const {
    Props merged;

    // Start with defaults
    for (const auto& [name, def] : prop_defs_) {
        merged[name] = def.default_value;
    }

    // Override with user props
    for (const auto& [name, value] : user_props) {
        merged[name] = value;
    }

    return merged;
}

// ============================================================================
// ComponentRegistry
// ============================================================================

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

void ComponentRegistry::register_component(Component::Ptr component) {
    if (!component) return;

    components_[component->name()] = component;
}

void ComponentRegistry::register_component(const std::string& name, Component::BuilderFunc builder) {
    auto component = Component::create(name);
    component->set_builder(builder);
    register_component(component);
}

Component::Ptr ComponentRegistry::get(const std::string& name) const {
    auto it = components_.find(name);
    if (it == components_.end()) return nullptr;
    return it->second;
}

bool ComponentRegistry::has(const std::string& name) const {
    return components_.find(name) != components_.end();
}

std::vector<std::string> ComponentRegistry::list_components() const {
    std::vector<std::string> names;
    for (const auto& [name, component] : components_) {
        names.push_back(name);
    }
    return names;
}

void ComponentRegistry::clear() {
    components_.clear();
}

} // namespace flex
