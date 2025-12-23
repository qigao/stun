/*
 * Flex Engine - Runtime State Machine System
 * Converts AST machines to tinyfsm-based runtime objects
 */

#pragma once

#include "flex/flex_ast.h"
#include "flex/fsm.h"
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace flex {

// ============================================================================
// Runtime State Machine
// ============================================================================

class RuntimeStateMachine {
public:
  using Ptr = std::shared_ptr<RuntimeStateMachine>;
  using StateChangeCallback =
      std::function<void(const std::string &layer, const std::string &from_state,
                         const std::string &to_state, const std::string &animation)>;

  RuntimeStateMachine(const std::string &name);
  ~RuntimeStateMachine() = default;

  // Add a layer to the machine
  void add_layer(const std::string &name);

  // Get a layer by name
  class RuntimeLayer *get_layer(const std::string &name);

  // Update all layers
  void update(float dt);

  // Set input value (triggers InputChange event)
  void set_input(const std::string &input_name, float value);

  // Set callback for state changes
  void set_state_change_callback(StateChangeCallback callback) {
    state_change_callback_ = callback;
  }

  // Fire state change (called by RuntimeLayer)
  void fire_state_change(const std::string &layer, const std::string &from_state,
                         const std::string &to_state, const std::string &animation);

  const std::string &name() const { return name_; }

private:
  std::string name_;
  std::unordered_map<std::string, std::unique_ptr<RuntimeLayer>> layers_;
  StateChangeCallback state_change_callback_;
};

// ============================================================================
// Runtime Layer
// ============================================================================

class RuntimeLayer {
public:
  RuntimeLayer(const std::string &name, RuntimeStateMachine *machine);
  ~RuntimeLayer() = default;

  // Add a state
  void add_state(const std::string &name, bool initial, const std::string &animation);

  // Add a transition
  void add_transition(const std::string &from, const std::string &to,
                      const std::string &condition_var, const std::string &condition_op,
                      float condition_val);

  // Update layer
  void update(float dt);

  // Get current state
  const std::string &current_state() const { return current_state_; }

  // Set input value
  void set_input(const std::string &input_name, float value);

  const std::string &name() const { return name_; }

private:
  std::string name_;
  RuntimeStateMachine *machine_;

  std::string current_state_;
  std::unordered_map<std::string, std::unique_ptr<class RuntimeState>> states_;
  std::vector<std::unique_ptr<class RuntimeTransition>> transitions_;

  // Input values
  std::unordered_map<std::string, float> inputs_;

  // Helper methods
  bool check_condition(const RuntimeTransition &trans) const;
  void transition_to(const std::string &state_name);
};

// ============================================================================
// Runtime State
// ============================================================================

class RuntimeState {
public:
  RuntimeState(const std::string &name, bool initial, const std::string &animation);
  ~RuntimeState() = default;

  const std::string &name() const { return name_; }
  bool initial() const { return initial_; }
  const std::string &animation() const { return animation_; }

private:
  std::string name_;
  bool initial_;
  std::string animation_;
};

// ============================================================================
// Runtime Transition
// ============================================================================

class RuntimeTransition {
public:
  RuntimeTransition(const std::string &from, const std::string &to,
                    const std::string &condition_var, const std::string &condition_op,
                    float condition_val);
  ~RuntimeTransition() = default;

  const std::string &from() const { return from_; }
  const std::string &to() const { return to_; }
  const std::string &condition_var() const { return condition_var_; }
  const std::string &condition_op() const { return condition_op_; }
  float condition_val() const { return condition_val_; }

  bool check(const std::unordered_map<std::string, float> &inputs) const;

private:
  std::string from_;
  std::string to_;
  std::string condition_var_;
  std::string condition_op_;
  float condition_val_;
};

// ============================================================================
// Implementation
// ============================================================================

inline RuntimeStateMachine::RuntimeStateMachine(const std::string &name) : name_(name) {}

inline void RuntimeStateMachine::add_layer(const std::string &name) {
  layers_[name] = std::make_unique<RuntimeLayer>(name, this);
}

inline RuntimeLayer *RuntimeStateMachine::get_layer(const std::string &name) {
  auto it = layers_.find(name);
  return (it != layers_.end()) ? it->second.get() : nullptr;
}

inline void RuntimeStateMachine::update(float dt) {
  for (auto &[name, layer] : layers_) {
    layer->update(dt);
  }
}

inline void RuntimeStateMachine::set_input(const std::string &input_name, float value) {
  // Broadcast to all layers
  for (auto &[name, layer] : layers_) {
    layer->set_input(input_name, value);
  }
}

inline void RuntimeStateMachine::fire_state_change(const std::string &layer,
                                                   const std::string &from_state,
                                                   const std::string &to_state,
                                                   const std::string &animation) {
  if (state_change_callback_) {
    state_change_callback_(layer, from_state, to_state, animation);
  }
}

// ---------------------------------------------------------------------------

inline RuntimeLayer::RuntimeLayer(const std::string &name, RuntimeStateMachine *machine)
    : name_(name), machine_(machine), current_state_("") {}

inline void RuntimeLayer::add_state(const std::string &name, bool initial,
                                    const std::string &animation) {
  states_[name] = std::make_unique<RuntimeState>(name, initial, animation);

  // Set as current state if initial
  if (initial || current_state_.empty()) {
    current_state_ = name;
  }
}

inline void RuntimeLayer::add_transition(const std::string &from, const std::string &to,
                                         const std::string &condition_var,
                                         const std::string &condition_op, float condition_val) {
  transitions_.push_back(
      std::make_unique<RuntimeTransition>(from, to, condition_var, condition_op, condition_val));
}

inline void RuntimeLayer::update(float dt) {
  // Check transitions from current state
  for (const auto &trans : transitions_) {
    if (trans->from() != current_state_)
      continue;

    if (trans->condition_var().empty()) {
      // Unconditional transition
      transition_to(trans->to());
      break;
    } else if (check_condition(*trans)) {
      // Conditional transition
      transition_to(trans->to());
      break;
    }
  }
}

inline void RuntimeLayer::set_input(const std::string &input_name, float value) {
  inputs_[input_name] = value;
}

inline bool RuntimeLayer::check_condition(const RuntimeTransition &trans) const {
  auto it = inputs_.find(trans.condition_var());
  if (it == inputs_.end())
    return false;

  float input_value = it->second;

  if (trans.condition_op() == ">")
    return input_value > trans.condition_val();
  if (trans.condition_op() == "<")
    return input_value < trans.condition_val();
  if (trans.condition_op() == "==")
    return std::abs(input_value - trans.condition_val()) < 0.001f;
  if (trans.condition_op() == "!=")
    return std::abs(input_value - trans.condition_val()) >= 0.001f;

  return false;
}

inline void RuntimeLayer::transition_to(const std::string &state_name) {
  auto it = states_.find(state_name);
  if (it != states_.end()) {
    std::string from_state = current_state_;
    current_state_ = state_name;
    std::cout << "[FSM] Layer '" << name_ << "' transitioned: " << from_state << " -> "
              << state_name << "\n";

    // Fire callback to trigger animation
    const auto &anim_name = it->second->animation();
    if (machine_) {
      machine_->fire_state_change(name_, from_state, state_name, anim_name);
    }
  }
}

// ---------------------------------------------------------------------------

inline RuntimeState::RuntimeState(const std::string &name, bool initial,
                                  const std::string &animation)
    : name_(name), initial_(initial), animation_(animation) {}

// ---------------------------------------------------------------------------

inline RuntimeTransition::RuntimeTransition(const std::string &from, const std::string &to,
                                            const std::string &condition_var,
                                            const std::string &condition_op, float condition_val)
    : from_(from), to_(to), condition_var_(condition_var), condition_op_(condition_op),
      condition_val_(condition_val) {}

inline bool RuntimeTransition::check(const std::unordered_map<std::string, float> &inputs) const {
  if (condition_var_.empty())
    return true;

  auto it = inputs.find(condition_var_);
  if (it == inputs.end())
    return false;

  float input_value = it->second;

  if (condition_op_ == ">")
    return input_value > condition_val_;
  if (condition_op_ == "<")
    return input_value < condition_val_;
  if (condition_op_ == "==")
    return std::abs(input_value - condition_val_) < 0.001f;
  if (condition_op_ == "!=")
    return std::abs(input_value - condition_val_) >= 0.001f;

  return false;
}

} // namespace flex
