/*
 * Flex Engine - Runtime State Machine System
 * Converts AST machines to tinyfsm-based runtime objects
 */

#pragma once

#include "flex/core/debug.h"
#include "flex/core/expr_compiled.h"
#include "flex/core/fsm.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace flex {

// ============================================================================
// State Change Info (replaces 6-param callback)
// ============================================================================

struct RuntimeStateAction {
    std::string node_id;
    std::string property;
    std::string expression;
};

struct StateChangeInfo {
    std::string layer;
    std::string from_state;
    std::string to_state;
    std::string animation;
    std::string play_audio;
    std::string stop_audio;
    std::vector<RuntimeStateAction> actions;
    std::unordered_map<std::string, std::string> animation_params;
};

// ============================================================================
// Runtime State Machine
// ============================================================================

class RuntimeStateMachine {
public:
  using SharedPtr = std::shared_ptr<RuntimeStateMachine>;
  using Ptr = SharedPtr;
  using StateChangeCallback = std::function<void(const StateChangeInfo&)>;

  RuntimeStateMachine(const std::string &name);
  ~RuntimeStateMachine() = default;

  void add_layer(const std::string &name);
  class RuntimeLayer *get_layer(const std::string &name);
  void update(float dt);
  void trigger_initial_animations();
  void set_input(Symbol input_name, float value);

  void set_state_change_callback(StateChangeCallback callback) {
    state_change_callback_ = callback;
  }

  void fire_state_change(const StateChangeInfo& info);

  const std::string &name() const { return name_; }
  SharedPtr clone() const;

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

  void add_state(const std::string &name, bool initial, const std::string &animation,
                 const std::string &play_audio = "", const std::string &stop_audio = "",
                 const std::vector<RuntimeStateAction>& actions = {},
                 const std::unordered_map<std::string, std::string>& animation_params = {});

  void add_transition(const std::string &from, const std::string &to,
                      const std::string &condition_expr);

  void update(float dt);
  void trigger_initial_animation();
  const std::string &current_state() const { return current_state_; }
  void set_input(Symbol input_name, float value);
  const std::string &name() const { return name_; }
  std::unique_ptr<RuntimeLayer> clone(RuntimeStateMachine *machine) const;

private:
  std::string name_;
  RuntimeStateMachine *machine_;
  std::string current_state_;
  std::unordered_map<std::string, std::unique_ptr<class RuntimeState>> states_;
  std::vector<std::unique_ptr<class RuntimeTransition>> transitions_;
  std::unordered_map<Symbol, float, SymbolHash> inputs_;

  void transition_to(const std::string &state_name);
};

// ============================================================================
// Runtime State
// ============================================================================

class RuntimeState {
public:
  RuntimeState(const std::string &name, bool initial, const std::string &animation,
               const std::string &play_audio = "", const std::string &stop_audio = "",
               const std::vector<RuntimeStateAction>& actions = {},
               const std::unordered_map<std::string, std::string>& animation_params = {});
  ~RuntimeState() = default;

  const std::string &name() const { return name_; }
  bool initial() const { return initial_; }
  const std::string &animation() const { return animation_; }
  const std::string &play_audio() const { return play_audio_; }
  const std::string &stop_audio() const { return stop_audio_; }
  const std::vector<RuntimeStateAction>& actions() const { return actions_; }
  const std::unordered_map<std::string, std::string>& animation_params() const { return animation_params_; }

  std::unique_ptr<RuntimeState> clone() const {
      return std::make_unique<RuntimeState>(name_, initial_, animation_, play_audio_, stop_audio_, actions_, animation_params_);
  }

private:
  std::string name_;
  bool initial_;
  std::string animation_;
  std::string play_audio_;
  std::string stop_audio_;
  std::vector<RuntimeStateAction> actions_;
  std::unordered_map<std::string, std::string> animation_params_;
};

// ============================================================================
// Runtime Transition
// ============================================================================

class RuntimeTransition {
public:
  RuntimeTransition(const std::string &from, const std::string &to,
                    const std::string &condition_expr);
  ~RuntimeTransition() = default;

  const std::string &from() const { return from_; }
  const std::string &to() const { return to_; }
  const std::string &condition_expr() const { return condition_expr_; }

  bool check_transition(const std::unordered_map<Symbol, float, SymbolHash> &inputs) const;

  std::unique_ptr<RuntimeTransition> clone() const {
     auto copy = std::make_unique<RuntimeTransition>(from_, to_, condition_expr_);
     // compiled_ left null for lazy recompile
     return copy;
  }

private:
  std::string from_;
  std::string to_;
  std::string condition_expr_;
  mutable std::shared_ptr<void> compiled_;
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

inline void RuntimeStateMachine::trigger_initial_animations() {
  for (auto &[name, layer] : layers_) {
    layer->trigger_initial_animation();
  }
}

inline void RuntimeStateMachine::set_input(Symbol input_name, float value) {
  for (auto &[name, layer] : layers_) {
    layer->set_input(input_name, value);
  }
}

inline void RuntimeStateMachine::fire_state_change(const StateChangeInfo& info) {
  if (state_change_callback_) {
    state_change_callback_(info);
  }
}

inline RuntimeStateMachine::SharedPtr RuntimeStateMachine::clone() const {
    auto copy = std::make_shared<RuntimeStateMachine>(name_);
    for (const auto& [name, layer] : layers_) {
        copy->layers_[name] = layer->clone(copy.get());
    }
    return copy;
}

// ---------------------------------------------------------------------------

inline RuntimeLayer::RuntimeLayer(const std::string &name, RuntimeStateMachine *machine)
    : name_(name), machine_(machine), current_state_("") {}

inline void RuntimeLayer::add_state(const std::string &name, bool initial,
                                    const std::string &animation,
                                    const std::string &play_audio,
                                    const std::string &stop_audio,
                                    const std::vector<RuntimeStateAction>& actions,
                                    const std::unordered_map<std::string, std::string>& animation_params) {
  states_[name] = std::make_unique<RuntimeState>(name, initial, animation, play_audio, stop_audio, actions, animation_params);

  if (initial || current_state_.empty()) {
    current_state_ = name;
  }
}

inline void RuntimeLayer::add_transition(const std::string &from, const std::string &to,
                                         const std::string &condition_expr) {
  transitions_.push_back(
      std::make_unique<RuntimeTransition>(from, to, condition_expr));
}

inline void RuntimeLayer::update(float dt) {
  for (const auto &trans : transitions_) {
    if (trans->from() != current_state_)
      continue;

    if (trans->condition_expr().empty()) {
        transition_to(trans->to());
        break;
    } else if (trans->check_transition(inputs_)) {
      transition_to(trans->to());
      break;
    }
  }
}

inline void RuntimeLayer::set_input(Symbol input_name, float value) {
  inputs_[input_name] = value;
}

inline void RuntimeLayer::trigger_initial_animation() {
  if (current_state_.empty()) return;

  auto it = states_.find(current_state_);
  if (it != states_.end() && machine_) {
    const auto& state = it->second;
    const bool has_state_change_payload =
        !state->animation().empty() ||
        !state->play_audio().empty() ||
        !state->stop_audio().empty() ||
        !state->actions().empty() ||
        !state->animation_params().empty();
    if (has_state_change_payload) {
      StateChangeInfo info;
      info.layer = name_;
      info.from_state = "";
      info.to_state = current_state_;
      info.animation = state->animation();
      info.play_audio = state->play_audio();
      info.stop_audio = state->stop_audio();
      info.actions = state->actions();
      info.animation_params = state->animation_params();
      machine_->fire_state_change(info);
    }
  }
}

inline void RuntimeLayer::transition_to(const std::string &state_name) {
  auto it = states_.find(state_name);
  if (it != states_.end()) {
    std::string from_state = current_state_;
    current_state_ = state_name;

    if (machine_) {
      StateChangeInfo info;
      info.layer = name_;
      info.from_state = from_state;
      info.to_state = state_name;
      info.animation = it->second->animation();
      info.play_audio = it->second->play_audio();
      info.stop_audio = it->second->stop_audio();
      info.actions = it->second->actions();
      info.animation_params = it->second->animation_params();
      machine_->fire_state_change(info);
    }
  }
}

inline std::unique_ptr<RuntimeLayer> RuntimeLayer::clone(RuntimeStateMachine *machine) const {
    auto copy = std::make_unique<RuntimeLayer>(name_, machine);
    copy->current_state_ = current_state_;
    copy->inputs_ = inputs_;

    for (const auto& [name, state] : states_) {
        copy->states_[name] = state->clone();
    }

    for (const auto& trans : transitions_) {
        copy->transitions_.push_back(trans->clone());
    }

    return copy;
}

// ---------------------------------------------------------------------------

inline RuntimeState::RuntimeState(const std::string &name, bool initial,
                                  const std::string &animation,
                                  const std::string &play_audio,
                                  const std::string &stop_audio,
                                  const std::vector<RuntimeStateAction>& actions,
                                  const std::unordered_map<std::string, std::string>& animation_params)
    : name_(name), initial_(initial), animation_(animation),
      play_audio_(play_audio), stop_audio_(stop_audio),
      actions_(actions), animation_params_(animation_params) {}

// ---------------------------------------------------------------------------

inline RuntimeTransition::RuntimeTransition(const std::string &from, const std::string &to,
                                            const std::string &condition_expr)
    : from_(from), to_(to), condition_expr_(condition_expr) {}

inline bool RuntimeTransition::check_transition(const std::unordered_map<Symbol, float, SymbolHash> &inputs) const {
  if (condition_expr_.empty())
    return true;

  float result = evaluate_exprtk_inputs(condition_expr_, compiled_, inputs);
  return result != 0.0f;
}

} // namespace flex
