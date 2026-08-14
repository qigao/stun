/*
 * Flex Engine - InstanceNode Implementation (Runtime Layer)
 *
 * InstanceNode is a runtime scene graph node that embeds another .flex component.
 * Uses opaque pointers to avoid depending on bridge layer (Instance/Definition).
 *
 * Real implementations are in src/bridge/instance_node_bridge.cpp
 */

#include "flex/core/instance.h"

namespace flex {

void InstanceNode::set_source(const std::string& src) {
    if (source_ != src) {
        source_ = src;
        loaded_ = false;
    }
}

void InstanceNode::set_input(const std::string& name, float value) {
    float_inputs_[name] = value;
}

void InstanceNode::set_input(const std::string& name, const std::string& value) {
    string_inputs_[name] = value;
}

void InstanceNode::set_input(const std::string& name, const char* value) {
    set_input(name, std::string(value ? value : ""));
}

void InstanceNode::set_input(const std::string& name, bool value) {
    bool_inputs_[name] = value;
}

float InstanceNode::get_float_input(const std::string& name) const {
    auto it = float_inputs_.find(name);
    return (it != float_inputs_.end()) ? it->second : 0.0f;
}

const std::string& InstanceNode::get_string_input(const std::string& name) const {
    static const std::string empty;
    auto it = string_inputs_.find(name);
    return (it != string_inputs_.end()) ? it->second : empty;
}

bool InstanceNode::get_bool_input(const std::string& name) const {
    auto it = bool_inputs_.find(name);
    return (it != bool_inputs_.end()) ? it->second : false;
}

} // namespace flex
