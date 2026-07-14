/*
 * Flex Engine - Instance Context Interface
 *
 * Abstract interface for script bindings to access instance functionality.
 * This avoids script.cpp depending on the full Instance class (which is in the bridge layer).
 */

#pragma once

#include "flex/core/types.h"
#include <string>

namespace flex {

// Forward declarations
class Node;
class Scene;
class AnimationController;
class TimelinePlayer;
class AssetManager;
class RuntimeStateMachine;

// ============================================================================
// IInstanceContext - Interface for runtime systems to access instance state
// ============================================================================

class IInstanceContext {
public:
    virtual ~IInstanceContext() = default;

    // Input control
    virtual void set_input(const char* name, float value) = 0;
    virtual void set_input(const char* name, const char* value) = 0;
    virtual float get_input(const char* name) const = 0;

    // Scene access
    virtual Scene* scene() const = 0;

    // Animation control
    virtual AnimationController* animation_controller() const = 0;
    virtual TimelinePlayer* play(const char* timeline_name, Node* target) = 0;
    virtual TimelinePlayer* play(const char* timeline_name) = 0;
    virtual void stop(const char* timeline_name) = 0;
    virtual void stop_all() = 0;

    // State machine
    virtual RuntimeStateMachine* get_machine(const std::string& name) = 0;

    // Asset management
    virtual AssetManager* asset_manager() const = 0;
    virtual const char* resolve_asset(const char* name) const = 0;

    // Event handling
    virtual void send_event(const char* name) = 0;
};

} // namespace flex
