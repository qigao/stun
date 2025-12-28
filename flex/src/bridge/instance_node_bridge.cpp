/*
 * Flex Engine - InstanceNode Bridge Implementation
 *
 * Bridge layer implementation for InstanceNode methods that need
 * access to Definition and Instance classes.
 */

#include "flex.h"

namespace flex {

// Helper to cast opaque pointers
#define DEF_PTR (reinterpret_cast<std::shared_ptr<Definition>*>(definition_))
#define INST_PTR (reinterpret_cast<std::shared_ptr<Instance>*>(instance_))

// These methods are defined here instead of instance.cpp to access bridge types

Scene* InstanceNode::content() const {
    if (!instance_) return nullptr;
    return (*INST_PTR)->scene();
}

bool InstanceNode::load() {
    if (source_.empty()) return false;
    if (loaded_) return true;

    // Load definition from file
    if (!definition_) {
        definition_ = new std::shared_ptr<Definition>();
    }
    *DEF_PTR = Definition::load_file(source_.c_str());

    if (!*DEF_PTR || (*DEF_PTR)->has_error()) {
        return false;
    }

    // Create instance
    if (!instance_) {
        instance_ = new std::shared_ptr<Instance>();
    }
    *INST_PTR = Instance::create(*DEF_PTR);

    if (!*INST_PTR) {
        return false;
    }

    // Apply stored inputs
    for (const auto& [name, value] : float_inputs_) {
        (*INST_PTR)->set_input(name.c_str(), value);
    }
    for (const auto& [name, value] : string_inputs_) {
        (*INST_PTR)->set_input(name.c_str(), value.c_str());
    }

    loaded_ = true;
    return true;
}

void InstanceNode::render(Renderer& renderer) {
    if (!visible()) return;
    if (!loaded_ && !load()) return;
    if (!instance_ || !*INST_PTR) return;

    renderer.save();
    renderer.translate(x(), y());
    renderer.rotate(rotation());
    renderer.scale(scale_x(), scale_y());
    renderer.set_global_alpha(opacity());

    (*INST_PTR)->render(renderer);

    renderer.restore();
}

void InstanceNode::advance(float dt) {
    if (loaded_ && instance_ && *INST_PTR) {
        (*INST_PTR)->advance(dt);
    }
}

#undef DEF_PTR
#undef INST_PTR

} // namespace flex
