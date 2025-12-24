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
 * - Arena allocator for zero-allocation performance
 */

#pragma once

#include "flex/allocator.h"
#include "flex/binding.h"
#include "flex/dsl/animation.h"
#include "flex/dsl/artboard.h"
#include "flex/dsl/component.h"
#include "flex/dsl/event.h"
#include "flex/dsl/fsm.h"
#include "flex/dsl/geometry.h"
#include "flex/dsl/group.h"
#include "flex/dsl/image.h"
#include "flex/dsl/instance.h"
#include "flex/dsl/layout.h"
#include "flex/dsl/path.h"
#include "flex/dsl/physics.h"
#include "flex/dsl/script.h"
#include "flex/dsl/shape.h"
#include "flex/dsl/state_binding.h"
#include "flex/dsl/svg.h"
#include "flex/dsl/text.h"
#include "flex/dsl/timeline.h"
#include "flex/node.h"
#include "flex/renderer.h"
#include "flex/runtime_machine.h"
#include "flex/solo.h"
#include "flex/types.h"
#include "parser/flex_ast.h"
#include "parser/flex_parser.h"
#include "parser/flex_token.h"

#include <fmtlog.h>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
// ThorVG for rendering
#include <thorvg.h>

namespace flex {

// ============================================================================
// Forward Declarations
// ============================================================================

// Forward declarations for friend classes
namespace parser {
class AstToRuntimeConverter;
}

// ============================================================================
// Definition - Immutable blueprint loaded from .flex file
// ============================================================================

class Definition {
  friend class Instance;
  friend class AstToRuntimeConverter; // In flex namespace

public:
  using Ptr = std::shared_ptr<Definition>;

  ~Definition() = default;

  // Load from .flex source
  static Ptr load(const char *source);
  static Ptr load_file(const char *path);

  // Check for parse errors
  bool has_error() const { return impl_->has_error; }
  const char *error_message() const { return impl_->error_message.c_str(); }
  int error_line() const { return impl_->error_line; }
  int error_column() const { return impl_->error_column; }

  // Access parsed objects (no more ast::Document)
  Artboard::Ptr artboard() const { return impl_->artboard; }
  const std::vector<Timeline::Ptr> &timelines() const { return impl_->timelines; }

  // Access runtime objects
  const std::vector<std::shared_ptr<class RuntimeStateMachine>> &machines() const {
    return impl_->machines;
  }
  const std::unordered_map<std::string, std::shared_ptr<class RuntimeAnimation>> &
  animations() const {
    return impl_->animations;
  }

private:
  Definition() = default;
  struct Impl {
    // Arena allocator for this definition's objects
    ArenaAllocator object_alloc{64 * 1024}; // 64KB

    Artboard::Ptr artboard;
    std::vector<Timeline::Ptr> timelines;

    // Runtime objects
    std::vector<std::shared_ptr<class RuntimeStateMachine>> machines;
    std::unordered_map<std::string, std::shared_ptr<class RuntimeAnimation>> animations;

    std::string error_message;
    int error_line = 0;
    int error_column = 0;
    bool has_error = false;
  };
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

  // Create instance with custom arena allocator (optional)
  static Ptr create(float width, float height, ArenaAllocator &frame_alloc,
                    ArenaAllocator &object_alloc);

  // Get raw pointer (for TimelinePlayer)
  Instance *get_ptr() { return this; }

  // -------------------------------------------
  // Scene Access
  // -------------------------------------------

  Artboard *artboard() const { return impl_->artboard.get(); }

  // -------------------------------------------
  // Input Control
  // -------------------------------------------

  void set_input(const char *name, float value);
  void set_input(const char *name, const char *value);

  // -------------------------------------------
  // Frame Update
  // -------------------------------------------

  // Advance by delta time (seconds) - resets frame allocator
  void advance(float dt);

  // -------------------------------------------
  // Rendering
  // -------------------------------------------

  void render(Renderer &renderer);

  // -------------------------------------------
  // Event Handling
  // -------------------------------------------

  void send_pointer_event(float x, float y, bool is_down);
  void send_event(const char *name);

  // -------------------------------------------
  // Animation Control (Phase 2)
  // -------------------------------------------

  // Get animation controller
  AnimationController *animation_controller() const;

  // Add a timeline
  void add_timeline(Timeline::Ptr timeline);

  // Play a timeline on a node
  TimelinePlayer *play(const char *timeline_name, Node *target);
  TimelinePlayer *play(const char *timeline_name); // Play on artboard root

  // Stop animations
  void stop(const char *timeline_name);
  void stop_all();

  // Get input value (for state machine conditions)
  float get_input(const char *name) const;

  // -------------------------------------------
  // Runtime Systems Access
  // -------------------------------------------

  // Get state machine by name
  class RuntimeStateMachine *get_machine(const std::string &name);

  // Get animation by name
  class RuntimeAnimation *get_animation(const std::string &name);

  // Start animation
  void start_animation(const std::string &name);

  // Stop animation
  void stop_animation(const std::string &name);

  // Asset resolution
  void register_asset(const char *name, const char *path);
  const char *resolve_asset(const char *name) const;

  // -------------------------------------------
  // Memory Pool Access
  // -------------------------------------------

  // Get frame allocator (for temporary allocations)
  ArenaAllocator *frame_allocator() const;

  // Get object allocator (for persistent allocations)
  ArenaAllocator *object_allocator() const;

  // Reset frame allocator (call at start of each frame)
  void reset_frame();

private:
  Instance();
  struct Impl {
    Definition::Ptr definition;
    Artboard::Ptr artboard;
    float time = 0;

    // Arena allocators (based on memory_pool) - MUST be declared first!
    ArenaAllocator frame_alloc{16 * 1024};  // 16KB
    ArenaAllocator object_alloc{64 * 1024}; // 64KB

    // Input values (Phase 2.3: Single map with variant)
    using InputValue = std::variant<float, std::string>;
    std::unordered_map<std::string, InputValue> inputs;
    std::unordered_map<std::string, std::string> assets;

    // Data bindings
    std::unique_ptr<BindingContext> bindings;

    // Script context for expression evaluation
    std::unique_ptr<ScriptContext> script_ctx;

    // Phase 2: Animation and State Machine
    AnimationController animation_controller{object_alloc};
    std::set<std::string> fired_events;

    // Runtime State Machine
    std::map<std::string, std::string> layer_states;

    // Runtime systems (Phase 3)
    std::vector<std::shared_ptr<class RuntimeStateMachine>> machines;
    std::unique_ptr<class AnimationManager> animation_manager;

    // Pointer state for event handling
    std::weak_ptr<Node> hover_node;
    std::weak_ptr<Node> pointer_down_node;
    bool is_pointer_down = false;
  };
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Parser API
// ============================================================================

namespace parser {

// Parse .flex source and build Runtime objects directly
// Returns nullptr on error - use get_error() for details
Artboard::Ptr parse(const char *source, std::vector<Timeline::Ptr> *out_timelines,
                    void *out_machine, // REMOVED: Machine::Ptr* out_machine (old system)
                    ArenaAllocator &alloc);

// Get last parse error
const char *get_error();
int get_error_line();
int get_error_column();

} // namespace parser

// ============================================================================
// Engine Initialization
// ============================================================================

// Initialize the Flex engine (call once at startup)
inline void init() {
  // Initialize ThorVG
  tvg::Initializer::init(0);

  // Initialize fmtlog
  fmtlog::setLogLevel(fmtlog::DBG); // Enable all log levels (DBG, INF, WRN, ERR)
  fmtlog::setThreadName("main");

  // Optional: Output to file for debugging
  // fmtlog::setLogFile("flex_engine.log", true);
}

// Shutdown the Flex engine (call at exit)
inline void shutdown() {
  // Flush and shutdown fmtlog
  fmtlog::shutdown();

  // Terminate ThorVG
  tvg::Initializer::term();
}

// ============================================================================
// Font Management
// ============================================================================

// Load a font from file (TTF, OTF)
// Returns true on success
inline bool load_font(const char *path) {
  if (!path)
    return false;
  return tvg::Text::load(path) == tvg::Result::Success;
}

// Load a font from file with a custom name
inline bool load_font(const char *name, const char *path) {
  // ThorVG's Text::load(filename) uses filename as the font name
  // For custom name, we need to load from memory
  if (!name || !path)
    return false;

  // Read file into memory
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size))
    return false;

  return tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) ==
         tvg::Result::Success;
}

// Load a font from memory
inline bool load_font_data(const char *name, const char *data, uint32_t size) {
  if (!name || !data || size == 0)
    return false;
  return tvg::Text::load(name, data, size, "ttf", true) == tvg::Result::Success;
}

// Unload a previously loaded font
inline void unload_font(const char *name) {
  if (name) {
    tvg::Text::unload(name);
  }
}

} // namespace flex
