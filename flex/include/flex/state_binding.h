/*
 * Flex Engine - State Binding
 *
 * Helpers to bind Observable State to Component props for reactive UI
 */

#pragma once

#include "flex/state.h"
#include "flex/component.h"
#include "flex/node.h"
#include "flex/group.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace flex {

// ============================================================================
// State Component Binding - Automatic Component Recreation on State Change
// ============================================================================

class StateComponentBinding {
public:
    using Ptr = std::shared_ptr<StateComponentBinding>;

    StateComponentBinding(ObservableState::Ptr state,
                         const std::string& component_name,
                         const Props& initial_props)
        : state_(state)
        , component_name_(component_name)
        , props_(initial_props)
        , component_(create_component_instance(component_name, initial_props))
    {}

    ~StateComponentBinding() {
        // Unwatch all observers
        for (auto observer_id : observer_ids_) {
            state_->unwatch(observer_id);
        }
    }

    // Get current component instance
    Node::Ptr component() const { return component_; }

    // Bind a state key to a component prop
    // When state[state_key] changes, component prop updates and rebuilds
    void bind(const std::string& state_key, const std::string& prop_name) {
        // Watch state changes
        auto observer_id = state_->watch(state_key, [this, state_key, prop_name](
            const std::string& key, const StateValue& value) {

            // Update prop value
            std::visit([this, &prop_name](const auto& val) {
                props_[prop_name] = val;
            }, value);

            // Rebuild component
            rebuild();
        });

        observer_ids_.push_back(observer_id);

        // Also initialize prop from current state value
        if (state_->has(state_key)) {
            if (auto val = state_->get<float>(state_key, 0.0f); val != 0.0f) {
                props_[prop_name] = val;
            } else if (auto str = state_->get<std::string>(state_key, ""); !str.empty()) {
                props_[prop_name] = str;
            } else if (auto b = state_->get<bool>(state_key, false); b) {
                props_[prop_name] = b;
            } else if (auto c = state_->get<uint32_t>(state_key, 0); c != 0) {
                props_[prop_name] = c;
            }
            rebuild();
        }
    }

    // Set callback when component rebuilds (useful for updating scene graph)
    void on_rebuild(std::function<void(Node::Ptr old_node, Node::Ptr new_node)> callback) {
        rebuild_callback_ = callback;
    }

private:
    void rebuild() {
        auto old_component = component_;

        // Recreate component with updated props
        component_ = create_component_instance(component_name_, props_);

        // Copy node properties (position, opacity, etc.) from old component
        if (old_component && component_) {
            component_->set_position(old_component->x(), old_component->y());
            component_->set_opacity(old_component->opacity());
            component_->set_rotation(old_component->rotation());
            component_->set_scale(old_component->scale_x(), old_component->scale_y());
            component_->set_visible(old_component->visible());
        }

        // Notify rebuild callback
        if (rebuild_callback_) {
            rebuild_callback_(old_component, component_);
        }
    }

    ObservableState::Ptr state_;
    std::string component_name_;
    Props props_;
    Node::Ptr component_;
    std::vector<size_t> observer_ids_;
    std::function<void(Node::Ptr, Node::Ptr)> rebuild_callback_;
};

// ============================================================================
// Helper Functions
// ============================================================================

// Create a state-bound component that auto-updates when state changes
inline StateComponentBinding::Ptr create_bound_component(
    ObservableState::Ptr state,
    const std::string& component_name,
    const Props& initial_props,
    const std::unordered_map<std::string, std::string>& state_to_prop_bindings)
{
    auto binding = std::make_shared<StateComponentBinding>(state, component_name, initial_props);

    // Setup bindings
    for (const auto& [state_key, prop_name] : state_to_prop_bindings) {
        binding->bind(state_key, prop_name);
    }

    return binding;
}

// ============================================================================
// Reactive Group - Group that manages state-bound children
// ============================================================================

class ReactiveGroup : public Group {
public:
    using Ptr = std::shared_ptr<ReactiveGroup>;

    static Ptr create(ObservableState::Ptr state) {
        auto group = std::shared_ptr<ReactiveGroup>(new ReactiveGroup());
        group->state_ = state;
        return group;
    }

    // Add a component that's bound to state
    void add_bound_component(
        const std::string& component_name,
        const Props& initial_props,
        const std::unordered_map<std::string, std::string>& state_to_prop_bindings,
        float x = 0, float y = 0)
    {
        auto binding = create_bound_component(state_, component_name, initial_props, state_to_prop_bindings);

        auto component = binding->component();
        component->set_position(x, y);
        add_child(component);

        // When component rebuilds, replace it in scene graph
        binding->on_rebuild([this](Node::Ptr old_node, Node::Ptr new_node) {
            if (!old_node || !new_node) return;

            // Find old node index
            const auto& children = this->children();
            for (size_t i = 0; i < children.size(); ++i) {
                if (children[i] == old_node) {
                    // Replace child
                    this->remove_child(old_node.get());
                    this->insert_child(new_node, i);
                    break;
                }
            }
        });

        // Store binding to keep it alive
        bindings_.push_back(binding);
    }

    ObservableState::Ptr state() const { return state_; }

private:
    ReactiveGroup() = default;

    ObservableState::Ptr state_;
    std::vector<StateComponentBinding::Ptr> bindings_;
};

} // namespace flex
