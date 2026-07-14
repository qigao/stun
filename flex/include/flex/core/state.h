/*
 * Flex Engine - State Management
 *
 * Observable state for reactive UI updates
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <variant>
#include <vector>

namespace flex {

// Forward declarations
class Component;
class Node;

// ============================================================================
// State Value Types
// ============================================================================

using StateValue = std::variant<
    float,           // Numbers
    std::string,     // Strings
    bool,            // Booleans
    uint32_t         // Colors (ARGB)
>;

// ============================================================================
// State Observer Callback
// ============================================================================

using StateObserver = std::function<void(const std::string& key, const StateValue& value)>;

// ============================================================================
// ObservableState - Observable reactive state container
// ============================================================================

class ObservableState {
public:
    using SharedPtr = std::shared_ptr<ObservableState>;
    using Ptr = SharedPtr;

    ObservableState() = default;
    ~ObservableState() = default;

    // Factory
    static SharedPtr create() {
        return std::make_shared<ObservableState>();
    }

    // -------------------------------------------
    // Value Getters
    // -------------------------------------------

    template<typename T>
    T get(const std::string& key, T default_value = T{}) const {
        auto it = values_.find(key);
        if (it == values_.end()) return default_value;

        try {
            return std::get<T>(it->second);
        } catch (const std::bad_variant_access&) {
            return default_value;
        }
    }

    // Specialized getters
    float get_float(const std::string& key, float default_value = 0.0f) const {
        return get<float>(key, default_value);
    }

    std::string get_string(const std::string& key, const std::string& default_value = "") const {
        return get<std::string>(key, default_value);
    }

    bool get_bool(const std::string& key, bool default_value = false) const {
        return get<bool>(key, default_value);
    }

    uint32_t get_color(const std::string& key, uint32_t default_value = 0xFFFFFFFF) const {
        return get<uint32_t>(key, default_value);
    }

    bool has(const std::string& key) const {
        return values_.find(key) != values_.end();
    }

    // -------------------------------------------
    // Value Setters (trigger observers)
    // -------------------------------------------

    template<typename T>
    void set(const std::string& key, const T& value) {
        StateValue new_value = value;

        // Check if value actually changed
        auto it = values_.find(key);
        if (it != values_.end() && it->second == new_value) {
            return;  // No change, don't notify
        }

        // Update value
        values_[key] = new_value;

        // Notify observers
        notify(key, new_value);
    }

    // Specialized setters
    void set_float(const std::string& key, float value) {
        set<float>(key, value);
    }

    void set_string(const std::string& key, const std::string& value) {
        set<std::string>(key, value);
    }

    void set_bool(const std::string& key, bool value) {
        set<bool>(key, value);
    }

    void set_color(const std::string& key, uint32_t value) {
        set<uint32_t>(key, value);
    }

    // -------------------------------------------
    // Observation
    // -------------------------------------------

    // Watch a specific key
    size_t watch(const std::string& key, StateObserver observer) {
        size_t id = next_observer_id_++;
        key_observers_[key].push_back({id, observer});
        return id;
    }

    // Watch all keys
    size_t watch_all(StateObserver observer) {
        size_t id = next_observer_id_++;
        global_observers_.push_back({id, observer});
        return id;
    }

    // Unwatch by ID
    void unwatch(size_t observer_id) {
        // Remove from key observers
        for (auto& [key, observers] : key_observers_) {
            observers.erase(
                std::remove_if(observers.begin(), observers.end(),
                    [observer_id](const auto& pair) { return pair.first == observer_id; }),
                observers.end()
            );
        }

        // Remove from global observers
        global_observers_.erase(
            std::remove_if(global_observers_.begin(), global_observers_.end(),
                [observer_id](const auto& pair) { return pair.first == observer_id; }),
            global_observers_.end()
        );
    }

    // Clear all observers
    void clear_observers() {
        key_observers_.clear();
        global_observers_.clear();
    }

    // -------------------------------------------
    // Batch Updates
    // -------------------------------------------

    // Start batch update (suspend notifications)
    void begin_batch() {
        batching_ = true;
        batch_changes_.clear();
    }

    // End batch update (send all notifications at once)
    void end_batch() {
        batching_ = false;

        // Notify all batched changes
        for (const auto& [key, value] : batch_changes_) {
            notify(key, value);
        }

        batch_changes_.clear();
    }

    // -------------------------------------------
    // Debugging
    // -------------------------------------------

    size_t size() const { return values_.size(); }

    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        for (const auto& [key, _] : values_) {
            result.push_back(key);
        }
        return result;
    }

private:
    std::map<std::string, StateValue> values_;

    // Observers: key -> list of (id, callback)
    std::map<std::string, std::vector<std::pair<size_t, StateObserver>>> key_observers_;
    std::vector<std::pair<size_t, StateObserver>> global_observers_;

    size_t next_observer_id_ = 1;

    // Batch update support
    bool batching_ = false;
    std::map<std::string, StateValue> batch_changes_;

    void notify(const std::string& key, const StateValue& value) {
        if (batching_) {
            batch_changes_[key] = value;
            return;
        }

        // Notify key-specific observers
        auto it = key_observers_.find(key);
        if (it != key_observers_.end()) {
            for (const auto& [id, observer] : it->second) {
                observer(key, value);
            }
        }

        // Notify global observers
        for (const auto& [id, observer] : global_observers_) {
            observer(key, value);
        }
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

// Create state from map
inline ObservableState::SharedPtr create_state(const std::map<std::string, StateValue>& initial_values) {
    auto state = ObservableState::create();
    for (const auto& [key, value] : initial_values) {
        std::visit([&](const auto& val) {
            state->set(key, val);
        }, value);
    }
    return state;
}

} // namespace flex
