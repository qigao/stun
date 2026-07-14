/*
 * Flex Engine - Component State Machine Integration
 *
 * Integrates tinyfsm for component-level state machines with DSL pseudo-class styles.
 * - State logic (behavior, data) in C++ using tinyfsm
 * - Visual styles in DSL using pseudo-classes (:idle, :hover, :dragging, etc.)
 */

#pragma once

#include "flex/core/types.h"
#include "tinyfsm.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>

namespace flex {

// Forward declarations
class Node;
class Shape;
class Text;
class Group;

// ============================================================================
// Pseudo-Class Style System
// ============================================================================

/**
 * PropertyValue - Type-safe value for node properties
 */
struct PropertyValue {
    using Value = std::variant<float, Color, std::string, bool>;
    Value value;

    PropertyValue() : value(0.0f) {}
    PropertyValue(float v) : value(v) {}
    PropertyValue(const Color& c) : value(c) {}
    PropertyValue(const std::string& s) : value(s) {}
    PropertyValue(const char* s) : value(std::string(s)) {}
    PropertyValue(bool b) : value(b) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(value); }

    template<typename T>
    T get() const { return std::get<T>(value); }

    template<typename T>
    T get_or(const T& default_val) const {
        return is<T>() ? get<T>() : default_val;
    }
};

/**
 * PropertySetter - Function that applies a property value to a node
 */
struct PropertySetter {
    std::string path;           // e.g., "thumb.fill"
    std::string child_id;       // "thumb"
    std::string prop_name;      // "fill"
    PropertyValue value;

    PropertySetter() = default;
    PropertySetter(const std::string& p, const PropertyValue& v) : path(p), value(v) {
        // Pre-parse the path
        size_t dot_pos = p.find('.');
        if (dot_pos != std::string::npos) {
            child_id = p.substr(0, dot_pos);
            prop_name = p.substr(dot_pos + 1);
        } else {
            prop_name = p;
        }
    }
};

/**
 * PseudoClassStyle - Collection of property setters for a pseudo-class
 * Example: :hover { bg.fill: #00e5ff, scale: 1.05 }
 */
class PseudoClassStyle {
public:
    PseudoClassStyle() = default;

    // Add property setter
    void add_property(const std::string& path, const PropertyValue& value) {
        properties_.emplace_back(path, value);
    }

    // Apply all properties to a node
    void apply_to(Node* node) const;

    const std::vector<PropertySetter>& properties() const { return properties_; }

private:
    std::vector<PropertySetter> properties_;
};

// ============================================================================
// Component FSM Base Class
// ============================================================================

/**
 * ComponentFsm - Base class for all component state machines
 *
 * Integrates tinyfsm with Flex nodes and pseudo-class styles.
 *
 * Usage:
 *   struct SliderFsm : ComponentFsm<SliderFsm> {
 *       void react(MouseMove const& e) override { ... }
 *   };
 */
template<typename Derived>
class ComponentFsm : public tinyfsm::Fsm<Derived> {
public:
    ComponentFsm() = default;
    virtual ~ComponentFsm() = default;

    // Owner node (the component this FSM controls)
    Node* owner = nullptr;

    // Delta time for update (set by advance())
    float dt = 0;

    // Apply DSL-defined pseudo-class style
    // Example: apply_dsl_style(":hover")
    void apply_dsl_style(const char* pseudo_class);

    // Virtual update method for states that need per-frame updates
    // (e.g., animation states like Snapping)
    virtual void update(float delta_time) {}

    // Helper: Find child node by ID
    Node* find(const std::string& id);

    // Helper: Clamp value
    static float clamp(float val, float min, float max) {
        return val < min ? min : (val > max ? max : val);
    }
};

// ============================================================================
// Common FSM Events
// ============================================================================

namespace events {

struct MouseEnter : tinyfsm::Event {
    float x, y;
    MouseEnter(float x = 0, float y = 0) : x(x), y(y) {}
};

struct MouseLeave : tinyfsm::Event {};

struct MouseMove : tinyfsm::Event {
    float x, y;
    MouseMove(float x = 0, float y = 0) : x(x), y(y) {}
};

struct MouseDown : tinyfsm::Event {
    float x, y;
    MouseDown(float x = 0, float y = 0) : x(x), y(y) {}
};

struct MouseUp : tinyfsm::Event {
    float x, y;
    MouseUp(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Click : tinyfsm::Event {
    float x, y;
    Click(float x = 0, float y = 0) : x(x), y(y) {}
};

struct KeyPress : tinyfsm::Event {
    int key;
    KeyPress(int k = 0) : key(k) {}
};

struct Update : tinyfsm::Event {
    float dt;
    Update(float dt = 0) : dt(dt) {}
};

struct InputChange : tinyfsm::Event {
    std::string name;
    float value;
    InputChange(const std::string& n = "", float v = 0.0f) : name(n), value(v) {}
};

struct AnimEnd : tinyfsm::Event {
    std::string animation_name;
    AnimEnd(const std::string& n = "") : animation_name(n) {}
};

struct CustomEvent : tinyfsm::Event {
    std::string name;
    CustomEvent(const std::string& n = "") : name(n) {}
};

} // namespace events

// ============================================================================
// GlobalFsm - Base for global/stateTracker state machines
// ============================================================================

template<typename Derived>
class GlobalFsm : public tinyfsm::Fsm<Derived> {
public:
    GlobalFsm() = default;
    virtual ~GlobalFsm() = default;

    // Input management
    void set_input(const std::string& name, float value) {
        inputs_[name] = value;
    }

    float get_input(const std::string& name) const {
        auto it = inputs_.find(name);
        return (it != inputs_.end()) ? it->second : 0.0f;
    }

    // Event queue for global events
    void fire_event(const std::string& event) {
        fired_events_.push_back(event);
    }

    bool has_event(const std::string& event) const {
        return std::find(fired_events_.begin(), fired_events_.end(), event) != fired_events_.end();
    }

    void clear_events() {
        fired_events_.clear();
    }

protected:
    std::unordered_map<std::string, float> inputs_;
    std::vector<std::string> fired_events_;
};

} // namespace flex
