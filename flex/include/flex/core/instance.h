/*
 * Flex Engine - Instance Node
 *
 * Component instance node for reusing .flex components.
 */

#pragma once

#include "flex/core/node.h"
#include "flex/core/types.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace flex {

// Forward declarations
class Definition;
class Instance;
class Scene;

class InstanceNode : public Node {
public:
    using SharedPtr = std::shared_ptr<InstanceNode>;
    using Ptr = SharedPtr;

    InstanceNode() = default;
    ~InstanceNode() override = default;

    static SharedPtr create() { return std::make_shared<InstanceNode>(); }

    NodeType type() const override { return NodeType::Instance; }
    const char* type_name() const override { return "Instance"; }

    // Source component (file path or asset name)
    const std::string& source() const { return source_; }
    void set_source(const std::string& src);

    // Input overrides
    void set_input(const std::string& name, float value);
    void set_input(const std::string& name, const std::string& value);
    void set_input(const std::string& name, const char* value);
    void set_input(const std::string& name, bool value);

    float get_float_input(const std::string& name) const;
    const std::string& get_string_input(const std::string& name) const;
    bool get_bool_input(const std::string& name) const;
    const std::unordered_map<std::string, float>& float_inputs() const { return float_inputs_; }
    const std::unordered_map<std::string, std::string>& string_inputs() const { return string_inputs_; }
    const std::unordered_map<std::string, bool>& bool_inputs() const { return bool_inputs_; }

    // Access the instantiated content
    Scene* content() const;

    // Load the component (call after set_source)
    bool load();
    bool is_loaded() const { return loaded_; }

    // Rendering
    void render(Renderer& renderer) override;

    // Update (propagate to inner instance)
    void advance(float dt);

private:
    std::string source_;
    bool loaded_ = false;

    // Input overrides
    std::unordered_map<std::string, float> float_inputs_;
    std::unordered_map<std::string, std::string> string_inputs_;
    std::unordered_map<std::string, bool> bool_inputs_;

    // The instantiated component (opaque pointers to avoid bridge dependency)
    void* definition_;  // std::shared_ptr<Definition>*
    void* instance_;    // std::shared_ptr<Instance>*
};

} // namespace flex
