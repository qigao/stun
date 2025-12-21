/*
 * Flex Engine - State Machine Implementation
 */

#include "flex/machine.h"
#include <algorithm>

namespace flex {

// ============================================================================
// Transition Implementation
// ============================================================================

Transition::Transition(const std::string& from, const std::string& to)
    : from_(from)
    , to_(to)
{
}

Transition* Transition::when_event(const std::string& event) {
    condition_ = Condition::event(event);
    return this;
}

Transition* Transition::when_input_gt(const std::string& input, float value) {
    condition_ = Condition::input_greater(input, value);
    return this;
}

Transition* Transition::when_input_lt(const std::string& input, float value) {
    condition_ = Condition::input_less(input, value);
    return this;
}

Transition* Transition::when_input_eq(const std::string& input, float value) {
    condition_ = Condition::input_equals(input, value);
    return this;
}

Transition* Transition::after(float seconds) {
    condition_ = Condition::after_time(seconds);
    return this;
}

Transition* Transition::on_anim_end() {
    condition_ = Condition::on_anim_end();
    return this;
}

// ============================================================================
// State Implementation
// ============================================================================

State::State(const std::string& name)
    : name_(name)
{
}

// ============================================================================
// Layer Implementation
// ============================================================================

Layer::Layer(const std::string& name)
    : name_(name)
{
}

State* Layer::add_state(const std::string& name) {
    auto state = std::make_unique<State>(name);
    State* ptr = state.get();
    states_[name] = std::move(state);

    // First state becomes initial by default
    if (initial_state_.empty()) {
        initial_state_ = name;
    }

    return ptr;
}

State* Layer::get_state(const std::string& name) const {
    auto it = states_.find(name);
    return (it != states_.end()) ? it->second.get() : nullptr;
}

Transition* Layer::add_transition(const std::string& from, const std::string& to) {
    auto trans = std::make_unique<Transition>(from, to);
    Transition* ptr = trans.get();
    transitions_.push_back(std::move(trans));
    return ptr;
}

void Layer::on_enter(const std::string& state, StateCallback callback) {
    enter_listeners_[state].push_back(callback);
}

void Layer::on_exit(const std::string& state, StateCallback callback) {
    exit_listeners_[state].push_back(callback);
}

void Layer::init() {
    current_state_ = initial_state_;
    time_in_state_ = 0;
}

bool Layer::check_condition(const Transition& trans,
                           const std::function<float(const std::string&)>& get_input,
                           const std::function<bool(const std::string&)>& is_event_fired,
                           const std::function<bool(const std::string&)>& is_anim_finished) const {
    const auto& cond = trans.condition();

    switch (cond.type) {
        case ConditionType::Event:
            return is_event_fired(cond.event_name);

        case ConditionType::InputEquals: {
            float val = get_input(cond.input_name);
            return std::abs(val - cond.value) < 0.0001f;
        }

        case ConditionType::InputGreater:
            return get_input(cond.input_name) > cond.value;

        case ConditionType::InputLess:
            return get_input(cond.input_name) < cond.value;

        case ConditionType::AfterTime:
            return time_in_state_ >= cond.duration;

        case ConditionType::OnAnimEnd: {
            auto* state = get_state(current_state_);
            if (state && !state->animation().empty()) {
                return is_anim_finished(state->animation());
            }
            return false;
        }

        default:
            return false;
    }
}

void Layer::transition_to(const std::string& state) {
    std::string old_state = current_state_;

    // Fire exit listeners for old state
    auto exit_it = exit_listeners_.find(old_state);
    if (exit_it != exit_listeners_.end()) {
        for (const auto& cb : exit_it->second) {
            cb();
        }
    }

    current_state_ = state;
    time_in_state_ = 0;

    // Fire enter listeners for new state
    auto enter_it = enter_listeners_.find(state);
    if (enter_it != enter_listeners_.end()) {
        for (const auto& cb : enter_it->second) {
            cb();
        }
    }

    if (on_change_) {
        on_change_(old_state, current_state_);
    }
}

bool Layer::update(float dt,
                  const std::function<float(const std::string&)>& get_input,
                  const std::function<bool(const std::string&)>& is_event_fired,
                  const std::function<bool(const std::string&)>& is_anim_finished) {
    time_in_state_ += dt;

    // Check transitions from current state
    for (const auto& trans : transitions_) {
        if (trans->from_state() != current_state_) {
            continue;
        }

        if (check_condition(*trans, get_input, is_event_fired, is_anim_finished)) {
            transition_to(trans->to_state());
            return true;
        }
    }

    return false;
}

// ============================================================================
// Machine Implementation
// ============================================================================

Machine::Machine(const std::string& name)
    : name_(name)
{
}

Layer::Ptr Machine::add_layer(const std::string& name) {
    auto layer = Layer::create(name);
    layers_.push_back(layer);
    return layer;
}

Layer* Machine::get_layer(const std::string& name) const {
    for (const auto& layer : layers_) {
        if (layer->name() == name) {
            return layer.get();
        }
    }
    return nullptr;
}

void Machine::init() {
    for (auto& layer : layers_) {
        layer->init();
    }
}

void Machine::update(float dt,
                    const std::function<float(const std::string&)>& get_input,
                    const std::function<bool(const std::string&)>& is_event_fired,
                    const std::function<bool(const std::string&)>& is_anim_finished) {
    for (auto& layer : layers_) {
        layer->update(dt, get_input, is_event_fired, is_anim_finished);
    }
}

const std::string& Machine::current_state(const std::string& layer_name) const {
    static const std::string empty;
    auto* layer = get_layer(layer_name);
    return layer ? layer->current_state() : empty;
}

} // namespace flex
