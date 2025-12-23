/*
 * StatusTracker FSM - Global state machine for data binding
 * Uses tinyfsm as the single state machine foundation
 */

#pragma once

#include "flex/fsm.h"
#include "flex/types.h"
#include <tinyfsm.hpp>
#include <iostream>
#include <unordered_map>

namespace flex {

// Forward declarations
struct StatusFsm;
struct NeutralState;
struct PositiveState;
struct HighState;
struct VeryHighState;
struct NegativeState;
struct VeryLowState;

// ============================================================================
// StatusTracker FSM - Based on tinyfsm
// ============================================================================

struct StatusTrackerFsm : tinyfsm::Fsm<StatusTrackerFsm> {
    // Runtime state
    float counter = 0.0f;
    float time_in_state = 0.0f;

    // Helper: Check transition conditions
    bool check_condition(const std::string& op, float value) const {
        switch (op[0]) {
            case '>': return counter > value;
            case '<': return counter < value;
            case '=': return std::abs(counter - value) < 0.1f;
            default: return false;
        }
    }

    // Helper: Trigger animation
    void trigger_animation(const std::string& anim_name) {
        std::cout << "[FSM] Trigger animation: " << anim_name << "\n";
        // TODO: Integrate with animation system
    }

    // Default react methods
    void react(events::InputChange const& e) {
        if (e.name == "counter") {
            counter = e.value;
        }
    }

    void react(events::Update const& e) {
        time_in_state += e.dt;
    }

    void entry(void) {}
    void exit(void) {}
};

// ============================================================================
// States
// ============================================================================

struct NeutralState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter NeutralState\n";
        trigger_animation("toNeutral");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value > 0) {
                transit<PositiveState>();
            } else if (e.value < 0) {
                transit<NegativeState>();
            }
        }
    }
};

struct PositiveState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter PositiveState\n";
        trigger_animation("toPositive");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value > 5) {
                transit<HighState>();
            } else if (e.value < 0.1f) {
                transit<NeutralState>();
            }
        }
    }
};

struct HighState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter HighState\n";
        trigger_animation("toHigh");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value > 10) {
                transit<VeryHighState>();
            } else if (e.value < 5.1f) {
                transit<PositiveState>();
            }
        }
    }
};

struct VeryHighState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter VeryHighState\n";
        trigger_animation("toVeryHigh");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value < 10.1f) {
                transit<HighState>();
            }
        }
    }
};

struct NegativeState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter NegativeState\n";
        trigger_animation("toNegative");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value < -5) {
                transit<VeryLowState>();
            } else if (e.value > -0.1f) {
                transit<NeutralState>();
            }
        }
    }
};

struct VeryLowState : StatusTrackerFsm {
    void entry(void) {
        std::cout << "[FSM] Enter VeryLowState\n";
        trigger_animation("toVeryLow");
    }

    void react(events::InputChange const& e) {
        StatusTrackerFsm::react(e);

        if (e.name == "counter") {
            if (e.value > -5.1f) {
                transit<NegativeState>();
            }
        }
    }
};

// ============================================================================
// StatusTracker - Wrapper for managing the FSM
// ============================================================================

class StatusTracker {
public:
    StatusTracker() {
        fsm_ = std::make_shared<StatusTrackerFsm>();
        fsm_->start();
    }

    void set_counter(float value) {
        fsm_->dispatch(events::InputChange("counter", value));
    }

    float get_counter() const {
        return fsm_->counter;
    }

    void update(float dt) {
        fsm_->dispatch(events::Update(dt));
    }

    std::shared_ptr<StatusTrackerFsm> fsm() const { return fsm_; }

private:
    std::shared_ptr<StatusTrackerFsm> fsm_;
};

} // namespace flex

// Set initial state (must be outside namespace for macro to work)
FSM_INITIAL_STATE(flex::StatusTrackerFsm, flex::NeutralState)
