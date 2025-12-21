/*
 * Flex Engine - Instance Node Implementation
 */

#include "flex/instance.h"
#include "flex/flex.h"
#include "flex/renderer.h"

namespace flex {

void InstanceNode::set_source(const std::string& src) {
    if (source_ != src) {
        source_ = src;
        loaded_ = false;
        instance_.reset();
        definition_.reset();
    }
}

void InstanceNode::set_input(const std::string& name, float value) {
    float_inputs_[name] = value;
    if (instance_) {
        instance_->set_input(name, value);
    }
}

void InstanceNode::set_input(const std::string& name, const std::string& value) {
    string_inputs_[name] = value;
    if (instance_) {
        instance_->set_input(name, value);
    }
}

void InstanceNode::set_input(const std::string& name, const char* value) {
    set_input(name, std::string(value ? value : ""));
}

void InstanceNode::set_input(const std::string& name, bool value) {
    bool_inputs_[name] = value;
    // Instance doesn't have bool input setter yet, store locally
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

Artboard* InstanceNode::content() const {
    return instance_ ? instance_->artboard() : nullptr;
}

bool InstanceNode::load() {
    if (source_.empty()) return false;
    if (loaded_) return true;

    // Load the definition from file
    definition_ = Definition::load_file(source_.c_str());
    if (!definition_ || definition_->has_error()) {
        return false;
    }

    // Create instance
    instance_ = Instance::create(definition_);
    if (!instance_) {
        return false;
    }

    // Apply stored inputs
    for (const auto& [name, value] : float_inputs_) {
        instance_->set_input(name, value);
    }
    for (const auto& [name, value] : string_inputs_) {
        instance_->set_input(name, value);
    }

    loaded_ = true;
    return true;
}

void InstanceNode::render(Renderer& renderer) {
    if (!visible()) return;
    if (!loaded_ && !load()) return;
    if (!instance_) return;

    renderer.save();
    renderer.translate(x(), y());
    renderer.rotate(rotation());
    renderer.scale(scale_x(), scale_y());
    renderer.set_global_alpha(opacity());

    instance_->render(renderer);

    renderer.restore();
}

void InstanceNode::advance(float dt) {
    if (instance_) {
        instance_->advance(dt);
    }
}

} // namespace flex
