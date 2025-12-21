/*
 * Flex Engine - State Machine System
 *
 * Hierarchical state machine with layers, states, and transitions.
 */

#pragma once

#include "flex/types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace flex {

// Forward declarations
class Timeline;
class AnimationController;

// ============================================================================
// Condition - Transition trigger
// ============================================================================

enum class ConditionType {
    Event,          // Triggered by named event
    InputEquals,    // input == value
    InputGreater,   // input > value
    InputLess,      // input < value
    AfterTime,      // After duration in current state
    OnAnimEnd,      // When animation finishes
};

struct Condition {
    ConditionType type = ConditionType::Event;
    std::string event_name;     // For Event type
    std::string input_name;     // For Input* types
    float value = 0;            // For comparisons
    float duration = 0;         // For AfterTime

    // Factory methods
    static Condition event(const std::string& name) {
        Condition c;
        c.type = ConditionType::Event;
        c.event_name = name;
        return c;
    }

    static Condition input_equals(const std::string& name, float val) {
        Condition c;
        c.type = ConditionType::InputEquals;
        c.input_name = name;
        c.value = val;
        return c;
    }

    static Condition input_greater(const std::string& name, float val) {
        Condition c;
        c.type = ConditionType::InputGreater;
        c.input_name = name;
        c.value = val;
        return c;
    }

    static Condition input_less(const std::string& name, float val) {
        Condition c;
        c.type = ConditionType::InputLess;
        c.input_name = name;
        c.value = val;
        return c;
    }

    static Condition after_time(float seconds) {
        Condition c;
        c.type = ConditionType::AfterTime;
        c.duration = seconds;
        return c;
    }

    static Condition on_anim_end() {
        Condition c;
        c.type = ConditionType::OnAnimEnd;
        return c;
    }
};

// ============================================================================
// Transition - State change rule
// ============================================================================

class Transition {
public:
    Transition(const std::string& from, const std::string& to);
    ~Transition() = default;

    const std::string& from_state() const { return from_; }
    const std::string& to_state() const { return to_; }

    // Condition
    const Condition& condition() const { return condition_; }
    void set_condition(const Condition& c) { condition_ = c; }

    // Fluent API
    Transition* when_event(const std::string& event);
    Transition* when_input_gt(const std::string& input, float value);
    Transition* when_input_lt(const std::string& input, float value);
    Transition* when_input_eq(const std::string& input, float value);
    Transition* after(float seconds);
    Transition* on_anim_end();

    // Transition duration (for blending)
    float duration() const { return duration_; }
    void set_duration(float d) { duration_ = d; }

private:
    std::string from_;
    std::string to_;
    Condition condition_;
    float duration_ = 0;  // Instant transition if 0
};

// ============================================================================
// State - Named state with animations
// ============================================================================

class State {
public:
    State(const std::string& name);
    ~State() = default;

    const std::string& name() const { return name_; }

    // Animation to play while in this state
    const std::string& animation() const { return animation_; }
    void set_animation(const std::string& anim) { animation_ = anim; }

    // Entry/exit animations (optional)
    const std::string& on_enter_animation() const { return on_enter_; }
    void set_on_enter(const std::string& anim) { on_enter_ = anim; }

    const std::string& on_exit_animation() const { return on_exit_; }
    void set_on_exit(const std::string& anim) { on_exit_ = anim; }

private:
    std::string name_;
    std::string animation_;
    std::string on_enter_;
    std::string on_exit_;
};

// ============================================================================
// Layer - Independent state machine track
// ============================================================================

class Layer {
public:
    using Ptr = std::shared_ptr<Layer>;

    Layer(const std::string& name);
    ~Layer() = default;

    // Factory
    static Ptr create(const std::string& name) {
        return std::make_shared<Layer>(name);
    }

    const std::string& name() const { return name_; }

    // State management
    State* add_state(const std::string& name);
    State* get_state(const std::string& name) const;

    // Initial state
    const std::string& initial_state() const { return initial_state_; }
    void set_initial_state(const std::string& name) { initial_state_ = name; }

    // Transition management
    Transition* add_transition(const std::string& from, const std::string& to);
    const std::vector<std::unique_ptr<Transition>>& transitions() const { return transitions_; }

    // Runtime state
    const std::string& current_state() const { return current_state_; }
    float time_in_state() const { return time_in_state_; }

    // Initialize (call before first update)
    void init();

    // Update - check transitions and advance time
    // Returns true if state changed
    bool update(float dt, 
                const std::function<float(const std::string&)>& get_input,
                const std::function<bool(const std::string&)>& is_event_fired,
                const std::function<bool(const std::string&)>& is_anim_finished);

    // State change callback
    using StateChangeCallback = std::function<void(
        const std::string& from, const std::string& to)>;
    void on_state_change(StateChangeCallback callback) { on_change_ = callback; }

    // State event listeners
    using StateCallback = std::function<void()>;
    void on_enter(const std::string& state, StateCallback callback);
    void on_exit(const std::string& state, StateCallback callback);

private:
    std::string name_;
    std::unordered_map<std::string, std::unique_ptr<State>> states_;
    std::vector<std::unique_ptr<Transition>> transitions_;
    std::string initial_state_;

    // Runtime
    std::string current_state_;
    float time_in_state_ = 0;
    StateChangeCallback on_change_;
    std::unordered_map<std::string, std::vector<StateCallback>> enter_listeners_;
    std::unordered_map<std::string, std::vector<StateCallback>> exit_listeners_;

    // Check if transition condition is met
    bool check_condition(const Transition& trans,
                        const std::function<float(const std::string&)>& get_input,
                        const std::function<bool(const std::string&)>& is_event_fired,
                        const std::function<bool(const std::string&)>& is_anim_finished) const;

    // Perform state transition
    void transition_to(const std::string& state);
};

// ============================================================================
// Machine - Collection of layers
// ============================================================================

class Machine {
public:
    using Ptr = std::shared_ptr<Machine>;

    Machine(const std::string& name);
    ~Machine() = default;

    // Factory
    static Ptr create(const std::string& name) {
        return std::make_shared<Machine>(name);
    }

    const std::string& name() const { return name_; }

    // Layer management
    Layer::Ptr add_layer(const std::string& name);
    Layer* get_layer(const std::string& name) const;
    const std::vector<Layer::Ptr>& layers() const { return layers_; }

    // Initialize all layers
    void init();

    // Update all layers
    void update(float dt,
               const std::function<float(const std::string&)>& get_input,
               const std::function<bool(const std::string&)>& is_event_fired,
               const std::function<bool(const std::string&)>& is_anim_finished);

    // Get current state of a layer
    const std::string& current_state(const std::string& layer) const;

private:
    std::string name_;
    std::vector<Layer::Ptr> layers_;
};

} // namespace flex
