# Unified State Machine Architecture

## Overview

Successfully unified all state machines in Flex to use **tinyfsm.hpp** as the single foundation.

## Before vs After

### Before: Dual State Machine Systems

| System | Foundation | Purpose | Status |
|--------|-----------|---------|--------|
| **Component FSM** | tinyfsm.hpp | UI interaction (hover, pressed) | ✅ Active |
| **Global FSM** | Custom implementation | Business logic (statusTracker) | ❌ Deleted |

### After: Single Unified System

| System | Foundation | Purpose | Status |
|--------|-----------|---------|--------|
| **Component FSM** | tinyfsm.hpp | UI interaction (hover, pressed) | ✅ Active |
| **Global FSM** | tinyfsm.hpp | Business logic (statusTracker) | ✅ Active |

## Key Changes

### 1. Extended Event System (`flex/include/flex/fsm.h`)

Added new events for global state machines:

```cpp
namespace events {
    // Existing events
    struct MouseEnter : tinyfsm::Event { ... };
    struct MouseDown : tinyfsm::Event { ... };
    struct Update : tinyfsm::Event { ... };

    // NEW: Global state machine events
    struct InputChange : tinyfsm::Event {
        std::string name;
        float value;
    };

    struct AnimEnd : tinyfsm::Event {
        std::string animation_name;
    };

    struct CustomEvent : tinyfsm::Event {
        std::string name;
    };
}
```

### 2. Global FSM Base (`flex/include/flex/status_tracker_fsm.h`)

Created `StatusTrackerFsm` using tinyfsm:

```cpp
struct StatusTrackerFsm : tinyfsm::Fsm<StatusTrackerFsm> {
    float counter = 0.0f;

    void react(events::InputChange const& e) {
        if (e.name == "counter") {
            counter = e.value;
        }
    }
};

struct NeutralState : StatusTrackerFsm {
    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);
        if (e.value > 0) {
            transit<PositiveState>();
        }
    }
};

struct PositiveState : StatusTrackerFsm { ... };
struct HighState : StatusTrackerFsm { ... };
struct VeryHighState : StatusTrackerFsm { ... };
struct NegativeState : StatusTrackerFsm { ... };
struct VeryLowState : StatusTrackerFsm { ... };

// Set initial state
FSM_INITIAL_STATE(flex::StatusTrackerFsm, flex::NeutralState)
```

### 3. StatusTracker Wrapper

Simple wrapper for managing the FSM:

```cpp
class StatusTracker {
public:
    void set_counter(float value) {
        fsm_->dispatch(events::InputChange("counter", value));
    }

    void update(float dt) {
        fsm_->dispatch(events::Update(dt));
    }

private:
    std::shared_ptr<StatusTrackerFsm> fsm_;
};
```

## Usage Examples

### Component-Level FSM (Button)

```cpp
struct ButtonFsm : tinyfsm::Fsm<ButtonFsm> {
    void react(events::MouseEnter const&) {
        owner->set_scale(1.05f, 1.05f);
    }
};

struct ButtonNormal : ButtonFsm { ... };
struct ButtonHover : ButtonFsm { ... };
struct ButtonPressed : ButtonFsm { ... };

FSM_INITIAL_STATE(ButtonFsm, ButtonNormal)
```

### Global FSM (StatusTracker)

```cpp
auto status_tracker = std::make_shared<StatusTracker>();

// Update counter value
status_tracker->set_counter(5.0f);

// This triggers state transitions:
// Neutral -> Positive -> High -> VeryHigh

// Update FSM each frame
status_tracker->update(dt);
```

## Benefits

1. **Single Foundation**: All state machines use tinyfsm.hpp
2. **Unified API**: Same patterns for both component and global FSMs
3. **Type Safety**: Template-based design with compile-time checks
4. **Performance**: Zero overhead, no runtime polymorphism
5. **Maintainability**: One state machine library to maintain
6. **Consistency**: Same event system, same transition patterns

## Files Created/Modified

### Created
- `flex/include/flex/status_tracker_fsm.h` - Global state machine implementation
- `flex/examples/unified_statemachine_demo.cpp` - Unified demo

### Modified
- `flex/include/flex/fsm.h` - Extended with new events and base classes

## Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│                 tinyfsm.hpp                         │
│        (Single Foundation for ALL FSMs)             │
└─────────────────────────────────────────────────────┘
                          │
          ┌───────────────┴───────────────┐
          │                               │
    ┌─────▼──────┐                  ┌────▼──────┐
    │ Component  │                  │  Global   │
    │   FSMs     │                  │   FSMs    │
    │            │                  │           │
    │ • Button   │                  │ • Status  │
    │ • Slider   │                  │ • Counter │
    │ • Toggle   │                  │ • Health  │
    └─────┬──────┘                  └────┬──────┘
          │                               │
    ┌─────▼──────────────────────────────▼──────┐
    │           Event System                    │
    │                                           │
    │ • Mouse events (enter, leave, down, up)   │
    │ • Input events (counter change)           │
    │ • Custom events (submit, success, error)  │
    └───────────────────────────────────────────┘
```

## Conclusion

✅ **Mission Accomplished**: tinyfsm.hpp is now the ONLY state machine foundation for the entire Flex framework.

All state machines, whether for UI interaction or business logic, now use the same proven, lightweight, and efficient state machine library.
