/*
 * Flex Engine - Main Header
 *
 * Include this to use the Flex Engine.
 *
 * Features:
 * - Declarative scene graph (Group, Shape, Text, Image)
 * - Timeline animations with keyframes and easing
 * - State machine for logic control
 * - DSL parser for .flex files
 */

#pragma once

#include "flex/types.h"
#include "flex/geometry.h"
#include "flex/node.h"
#include "flex/group.h"
#include "flex/shape.h"
#include "flex/text.h"
#include "flex/image.h"
#include "flex/svg.h"
#include "flex/instance.h"
#include "flex/solo.h"
#include "flex/artboard.h"
#include "flex/renderer.h"
#include "flex/timeline.h"
#include "flex/machine.h"
  
#include "flex/script.h"
#include "flex/physics.h"
#include "flex/binding.h"
#include "flex/event.h"

#include <memory>
#include <string>
#include <functional>

namespace flex {

// ============================================================================
// Forward Declarations
// ============================================================================

// (AST removed - Parser directly builds Runtime objects)

// ============================================================================
// Definition - Immutable blueprint loaded from .flex file
// ============================================================================

class Definition {
public:
    using Ptr = std::shared_ptr<Definition>;

    ~Definition();

    // Load from .flex source
    static Ptr load(const char* source);
    static Ptr load_file(const char* path);

    // Check for parse errors
    bool has_error() const;
    const char* error_message() const;
    int error_line() const;
    int error_column() const;

    // Access parsed objects (no more ast::Document)
    Artboard::Ptr artboard() const;
    const std::vector<Timeline::Ptr>& timelines() const;
    Machine::Ptr machine() const;

private:
    Definition();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Instance - Mutable runtime state
// ============================================================================

class Instance {
public:
    using Ptr = std::shared_ptr<Instance>;

    ~Instance();

    // Create instance from definition
    static Ptr create(Definition::Ptr definition);

    // Create standalone instance (no definition)
    static Ptr create(float width, float height);

    // -------------------------------------------
    // Scene Access
    // -------------------------------------------

    Artboard* artboard() const;

    // -------------------------------------------
    // Input Control
    // -------------------------------------------

    void set_input(const std::string& name, float value);
    void set_input(const std::string& name, const std::string& value);

    // -------------------------------------------
    // Frame Update
    // -------------------------------------------

    // Advance by delta time (seconds)
    void advance(float dt);

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer);

    // -------------------------------------------
    // Event Handling
    // -------------------------------------------

    void send_pointer_event(float x, float y, bool is_down);
    void send_event(const std::string& name);

    // -------------------------------------------
    // Animation Control (Phase 2)
    // -------------------------------------------

    // Get animation controller
    AnimationController* animation_controller() const;

    // Add a timeline
    void add_timeline(Timeline::Ptr timeline);

    // Play a timeline on a node
    TimelinePlayer* play(const std::string& timeline_name, Node* target);
    TimelinePlayer* play(const std::string& timeline_name);  // Play on artboard root

    // Stop animations
    void stop(const std::string& timeline_name);
    void stop_all();

    // -------------------------------------------
    // State Machine Control (Phase 2)
    // -------------------------------------------

    // Set the state machine
    void set_machine(Machine::Ptr machine);
    Machine* machine() const;

    // Get current state of a layer
    const std::string& current_state(const std::string& layer) const;

    // Get input value (for state machine conditions)
    float get_input(const std::string& name) const;
    
    // Asset resolution
    void register_asset(const std::string& name, const std::string& path);
    const std::string& resolve_asset(const std::string& name) const;

private:
    Instance();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Parser API
// ============================================================================

namespace parser {

// Parse .flex source and build Runtime objects directly
// Returns nullptr on error - use get_error() for details
Artboard::Ptr parse(const char* source,
                    std::vector<Timeline::Ptr>* out_timelines = nullptr,
                    Machine::Ptr* out_machine = nullptr);

// Get last parse error
const char* get_error();
int get_error_line();
int get_error_column();

} // namespace parser

// ============================================================================
// Engine Initialization
// ============================================================================

// Initialize the Flex engine (call once at startup)
void init();

// Shutdown the Flex engine (call at exit)
void shutdown();

// ============================================================================
// Font Management
// ============================================================================

// Load a font from file (TTF, OTF)
// Returns true on success
bool load_font(const char* path);

// Load a font from file with a custom name
bool load_font(const char* name, const char* path);

// Load a font from memory
bool load_font_data(const char* name, const char* data, uint32_t size);

// Unload a previously loaded font
void unload_font(const char* name);

} // namespace flex
