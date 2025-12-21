/*
 * Observable Pattern
 *
 * Base class for objects that can notify observers of changes.
 * Used for Model -> ViewModel -> View data flow.
 */

#pragma once

#include "types.h"
#include <vector>
#include <algorithm>

namespace editor {

class Observable {
public:
    using ObserverId = size_t;

    virtual ~Observable() = default;

    ObserverId addObserver(EventCallback callback) {
        observers_.push_back({next_id_, std::move(callback)});
        return next_id_++;
    }

    void removeObserver(ObserverId id) {
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [id](const auto& o) { return o.id == id; }),
            observers_.end());
    }

protected:
    void notify(EventType type, void* data = nullptr) {
        for (auto& observer : observers_) {
            observer.callback(type, data);
        }
    }

private:
    struct Observer {
        ObserverId id;
        EventCallback callback;
    };
    std::vector<Observer> observers_;
    ObserverId next_id_ = 1;
};

// Property<T> - Observable property with automatic change notification
template<typename T>
class Property {
public:
    Property() = default;
    explicit Property(T value) : value_(std::move(value)) {}

    const T& get() const { return value_; }
    operator const T&() const { return value_; }

    void set(T value) {
        if (value_ != value) {
            value_ = std::move(value);
            notifyChange();
        }
    }

    Property& operator=(T value) {
        set(std::move(value));
        return *this;
    }

    void onChange(PropertyChangeCallback cb, const std::string& name = "") {
        callbacks_.push_back({name, std::move(cb)});
    }

private:
    void notifyChange() {
        for (auto& cb : callbacks_) {
            cb.callback(cb.name);
        }
    }

    T value_{};
    struct CallbackEntry {
        std::string name;
        PropertyChangeCallback callback;
    };
    std::vector<CallbackEntry> callbacks_;
};

} // namespace editor
